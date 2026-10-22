#!/usr/bin/env python3
"""Reproduce the bundled search face from the pinned public Noto TTC input.

Use FontTools4.5.0, the version recorded in the asset manifest, for byte-identical
output. The supported code points equal the existing design font, rather than
being restricted to characters appearing in reference screenshots.
"""
import argparse
import hashlib
import json
from pathlib import Path
import fontTools
from fontTools import subset
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parents[1]
SOURCE_SHA256 = "5dcd1c336cc9344cb77c03a0cd8982ca8a7dc97d620fd6c9c434e02dcb1ceeb3"

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--coverage-font", type=Path,
                        default=ROOT / "components/hmi/src/fonts/DshanSans-Regular.otf")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--manifest-output", required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists() or args.manifest_output.exists():
        parser.error("output already exists; choose a new destination")
    if digest(args.source) != SOURCE_SHA256:
        parser.error("source differs from the pinned Noto CJK TTC")
    coverage_font = TTFont(str(args.coverage_font))
    coverage = set(coverage_font.getBestCmap())
    font = TTFont(str(args.source), fontNumber=0, recalcTimestamp=False)
    if coverage - set(font.getBestCmap()):
        parser.error("source cannot cover the existing design character set")
    options = subset.Options()
    options.name_IDs = ["*"]
    options.name_languages = ["*"]
    options.name_legacy = True
    options.layout_features = ["*"]
    options.recalc_timestamp = False
    builder = subset.Subsetter(options=options)
    builder.populate(unicodes=coverage)
    builder.subset(font)
    names = {1: "Dshan Search", 2: "Regular", 3: "DshanSearch-Regular-UI1",
             4: "Dshan Search Regular", 6: "DshanSearch-Regular",
             16: "Dshan Search", 17: "Regular", 18: "Dshan Search Regular"}
    for record in font["name"].names:
        if record.nameID in names:
            record.string = names[record.nameID].encode(record.getEncoding())
    cff = font["CFF "].cff
    cff.fontNames = ["DshanSearch-Regular"]
    top = cff.topDictIndex[0]
    top.FullName = "Dshan Search Regular"
    top.FamilyName = "Dshan Search"
    top.Weight = "Regular"
    for index, dictionary in enumerate(top.FDArray):
        if hasattr(dictionary, "FontName"):
            dictionary.FontName = "DshanSearch-Regular-FD" + str(index)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.manifest_output.parent.mkdir(parents=True, exist_ok=True)
    font.save(str(args.output))
    result = TTFont(str(args.output))
    if set(result.getBestCmap()) != coverage:
        raise RuntimeError("generated character coverage changed")
    metadata = {
        "source": args.source.name, "source_sha256": SOURCE_SHA256,
        "source_face_index": 0, "source_family": "Noto Sans CJK JP",
        "fonttools_version": fontTools.__version__, "family": "Dshan Search",
        "coverage_basis": args.coverage_font.name,
        "coverage_basis_sha256": digest(args.coverage_font),
        "codepoint_count": len(coverage),
        "coverage_sha256": hashlib.sha256(json.dumps(sorted(coverage), separators=(",", ":")).encode()).hexdigest(),
        "output": args.output.name, "sha256": digest(args.output),
        "bytes": args.output.stat().st_size, "license": "SIL Open Font License1.1",
        "glyph_count": result["maxp"].numGlyphs,
    }
    args.manifest_output.write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + "\n")
    print(metadata["sha256"])

if __name__ == "__main__":
    main()
