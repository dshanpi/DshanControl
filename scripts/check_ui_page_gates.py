#!/usr/bin/env python3
"""Check the page-delivery ledger; evidence truth still requires human review."""
import argparse
import hashlib
import json
from pathlib import Path

STATES = ("normal", "offline", "empty", "pending", "failure", "cancel")
DELETED = "deleted"


def fingerprints(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted((root / "components/hmi").rglob("*"))
            if p.is_file()}


def check(root, phase="verify", selected=()):
    errors = []
    folder = root / "docs/design/page-gates"
    manifest = json.loads((folder / "manifest.json").read_text(encoding="utf-8"))
    ids = manifest["pages"]
    if not ids or len(set(ids)) != len(ids):
        errors.append("manifest: empty or duplicate page IDs")
    pages = {}
    def require(condition, message):
        if not condition:
            errors.append(message)
    def text(value):
        return isinstance(value, str) and bool(value.strip())
    def texts(value):
        return isinstance(value, list) and bool(value) and all(text(x) for x in value)
    def sha(value):
        return isinstance(value, str) and len(value) == 64 and all(c in "0123456789abcdef" for c in value)
    for page_id in ids:
        require(isinstance(page_id, str) and page_id.startswith("P") and page_id[1:].isdigit(),
                "manifest: invalid page ID")
        if not isinstance(page_id, str) or "/" in page_id or "\\" in page_id:
            continue
        page = json.loads((folder / (page_id + ".json")).read_text(encoding="utf-8"))
        pages[page_id] = page
        require(page.get("id") == page_id, page_id + ": identity mismatch")
        for key in ("name", "route", "business_goal", "prerequisites", "primary_action",
                    "confirmed_result", "failure_recovery"):
            require(text(page.get(key)), page_id + ": missing " + key)
        for key in ("operation_steps", "business_flow"):
            require(texts(page.get(key)), page_id + ": missing " + key)
        for state in STATES:
            require(text(page.get("state_coverage", {}).get(state)), page_id + ": missing state " + state)
        changes = page.get("function_changes", {})
        for key in ("retained", "added", "changed", "removed", "proposed_not_implemented"):
            value = changes.get(key)
            require(isinstance(value, list) and all(text(x) for x in value),
                    page_id + ": function change list required: " + key)
        require(isinstance(page.get("implementation", {}).get("history"), list),
                page_id + ": implementation history list required")
        review = page.get("design_review", {})
        require(review.get("decision") in ("pending", "approved", "changes_requested"),
                page_id + ": invalid design decision")
        require(text(review.get("preview_id")) and sha(review.get("preview_sha256")),
                page_id + ": design preview ID/SHA required")
        require(isinstance(review.get("known_gaps"), list), page_id + ": known_gaps list required")
        require(page.get("status") in ("design_review", "implementing", "implemented"),
                page_id + ": invalid status")

    def approved(page_id):
        if page_id not in pages:
            errors.append("unknown affected page: " + str(page_id))
            return
        authorization = pages[page_id].get("implementation_authorization", {})
        authorized = (authorization.get("decision") == "explicit_user_request"
                      and text(authorization.get("reference"))
                      and text(authorization.get("scope")))
        if phase == "design" and authorized:
            return
        review = pages[page_id].get("design_review", {})
        reviewed = review.get("decision") == "approved" and text(review.get("reference"))
        if authorized and not reviewed:
            resolution = pages[page_id].get("implementation_design_review", {})
            require(resolution.get("decision") == "reviewed_against_authorized_direction"
                    and text(resolution.get("reference"))
                    and texts(resolution.get("resolved_discrepancies"))
                    and texts(resolution.get("actual_review_screenshots")),
                    page_id + ": authorized implementation still needs actual design resolution")
        else:
            require(reviewed, page_id + ": design needs actual user review reference")
        require(review.get("known_gaps") == [], page_id + ": unresolved rendering gaps")

    if phase == "design":
        require(bool(selected), "--phase design requires --pages")
        for page_id in selected:
            approved(page_id)
        return errors

    current = fingerprints(root)
    baseline = manifest["baseline_hashes"]
    require(bool(baseline), "baseline must not be empty")
    for path, digest in baseline.items():
        require(path.startswith("components/hmi/") and sha(digest), "invalid baseline: " + path)
    changed = {path: current.get(path, DELETED) for path in set(baseline) | set(current)
               if baseline.get(path) != current.get(path)}
    reviews = manifest.get("impact_reviews", [])
    require(isinstance(reviews, list), "impact_reviews must be a list")
    impacts = {}
    for item in reviews if isinstance(reviews, list) else []:
        path = item.get("path")
        require(isinstance(path, str) and path not in impacts, "invalid/duplicate impact path")
        impacts[path] = item
    for path, digest in sorted(changed.items()):
        item = impacts.get(path, {})
        require(item.get("sha256") == digest, path + ": missing current impact review")
        affected = item.get("pages", [])
        require(texts(affected) and len(affected) == len(set(affected)),
                path + ": affected page IDs required")
        require(text(item.get("rationale")), path + ": impact rationale required")
        for page_id in affected if isinstance(affected, list) else []:
            approved(page_id)
            if page_id not in pages:
                continue
            page = pages[page_id]
            impl = page.get("implementation", {})
            require(page.get("status") == "implemented", page_id + ": implementation incomplete")
            require(text(impl.get("summary")), page_id + ": implementation summary required")
            require(impl.get("source_hashes", {}).get(path) == digest, page_id + ": source binding mismatch")
            require(texts(impl.get("verification")), page_id + ": verification references required")
            require(texts(impl.get("actual_screenshots")), page_id + ": actual UI screenshots required")
            require(isinstance(impl.get("remaining_risks"), list), page_id + ": remaining risks required")
            history = impl.get("history", [])
            matching = [entry for entry in history if isinstance(entry, dict)
                        and entry.get("source_hashes", {}).get(path) == digest]
            require(bool(matching), page_id + ": current change history required")
            for entry in matching:
                require(text(entry.get("id")) and text(entry.get("summary")),
                        page_id + ": history identity/summary required")
                for key in ("business_flow", "operation_steps", "verification", "actual_screenshots"):
                    require(texts(entry.get(key)), page_id + ": history missing " + key)
                delta = entry.get("function_changes", {})
                for key in ("retained", "added", "changed", "removed", "proposed_not_implemented"):
                    require(isinstance(delta.get(key), list) and all(text(x) for x in delta[key]),
                            page_id + ": history function changes required")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--phase", choices=("design", "verify"), default="verify")
    parser.add_argument("--pages", nargs="*", default=[])
    args = parser.parse_args()
    try:
        errors = check(args.root, args.phase, args.pages)
    except (OSError, ValueError, KeyError, TypeError, AttributeError) as exc:
        print("FAIL: invalid page ledger: " + str(exc))
        return 1
    if errors:
        for error in errors:
            print("FAIL: " + error)
        return 1
    print("PASS: page ledger structure and current source bindings; not hardware acceptance")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
