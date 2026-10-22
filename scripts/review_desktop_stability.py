#!/usr/bin/env python3
"""Offline evidence review; never connects to or changes a board.

A structurally complete run still requires performance and physical acceptance.
The JSON output deliberately has no overall PASS verdict.
"""
import argparse
import hashlib
import json
import math
import statistics
from pathlib import Path

APPS = ['overview', 'fieldbus', 'network', 'devices', 'alarms', 'settings',
        'cloud_bind', 'reporting', 'voice', 'home', 'video', 'update']


# Exact reviewed schemas only; a future revision requires source/provenance review.
REVIEWED_PROBE_REVISIONS = frozenset({
    'r21-fast-identity-full-hash-every-10-cycles-or-10-soak-samples',
    'r23-fast-identity-full-hash-every-10-cycles-or-10-soak-samples',
})

def review(report, switches, soak, observation, expected_sha, expected_boot):
    issues = set()

    def check(condition, message):
        if not condition:
            issues.add(message)

    def number(row, key):
        value = row.get(key)
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
            issues.add('missing/nonfinite numeric field: ' + key)
            return None
        return value

    check(report.get('state') == 'completed', 'run is not completed')
    check(report.get('phase') == 'complete', 'terminal phase is not complete')
    check(not report.get('error'), 'runner recorded failure')
    check(report.get('hmi_sha256') == expected_sha, 'candidate SHA mismatch')
    check(report.get('boot_id') == expected_boot == observation.get('current_boot_id'), 'boot identity mismatch or missing')
    check(observation.get('runner_alive') is False, 'runner termination not proven')
    check(observation.get('state') == report.get('state'), 'export/report terminal state mismatch')
    check(bool(report.get('hmi_start')), 'HMI start identity missing')
    check(report.get('probe_revision') in REVIEWED_PROBE_REVISIONS, 'probe revision differs; review measurement scope')
    check(report.get('layout_sha256') == report.get('layout_sha256_after_soak') and bool(report.get('layout_sha256')), 'layout preservation not proven')
    check(len(switches) == report.get('switches_completed') == report.get('switches_required') == 1000, 'not exactly 1000 raw transitions')
    check(report.get('switch_result') == 'completed', 'switching not completed')
    check(len(soak) == report.get('soak_samples') and len(soak) >= 2, 'soak row count mismatch or insufficient')
    check(report.get('soak_required_seconds') == 86400, 'required soak duration changed')
    for i, row in enumerate(switches):
        check(row.get('label') == ('desktop' if i % 2 else APPS[(i // 2) % 12]), 'switch order/title mismatch')
        keys = ['return_title_seconds'] if i % 2 else ['drawer_title_seconds', 'application_title_seconds']
        for key in keys:
            value = number(row, key)
            check(value is not None and 0 <= value < 10, 'title settlement timeout/invalid duration')

    start = number(report, 'soak_started_monotonic')
    elapsed = number(report, 'soak_elapsed_seconds')
    finished = number(report, 'switch_finished_monotonic')
    ended = number(report, 'ended_monotonic')
    if start is not None and finished is not None:
        check(start >= finished, 'soak overlaps switching')
    if switches and finished is not None:
        last = number(switches[-1], 'monotonic')
        check(last is not None and last <= finished, 'switch records extend past switching phase')
    if soak:
        last_t = number(soak[-1], 'monotonic')
        last_e = number(soak[-1], 'elapsed_seconds')
        check(last_e is not None and last_e >= 86400, 'raw soak does not reach 86400 seconds')
        check(elapsed is not None and last_e is not None and abs(elapsed - last_e) < .001, 'progress/raw soak duration mismatch')
        check(last_t is not None and ended is not None and ended >= last_t, 'terminal time precedes last raw sample or missing')
    else:
        issues.add('no raw soak samples')
    for i, row in enumerate(soak):
        check(row.get('label') == 'soak', 'invalid soak label')
        timestamp, duration = number(row, 'monotonic'), number(row, 'elapsed_seconds')
        if None not in (timestamp, duration, start):
            check(duration >= 0 and abs(timestamp - start - duration) < .001, 'elapsed time reset/inconsistent with monotonic time')
        if i % 10 == 0:
            check('health' in row, 'scheduled native health sample missing')

    def health_ok(health):
        return (isinstance(health, dict) and health.get('ok') is True and
                health.get('hmi_functional_heartbeat') is True and
                health.get('processes', {}).get('hmi') == report.get('hmi_pid'))

    check(health_ok(observation.get('health')), 'export native health/HMI identity not proven')
    previous = None
    for row in switches + soak:
        t, beat = number(row, 'monotonic'), number(row, 'heartbeat_monotonic')
        cpu, hz = number(row, 'cpu_ticks'), number(row, 'clock_ticks_per_second')
        rss, hwm = number(row, 'rss_kib'), number(row, 'hwm_kib')
        available = number(row, 'mem_available_kib')
        if None not in (t, beat):
            check(0 <= t - beat < 15, 'stale or future GUI heartbeat')
        if None not in (cpu, hz, rss, hwm, available):
            check(cpu >= 0 and hz > 0 and 0 < rss <= hwm and available >= 0, 'invalid process/memory sample')
        if previous:
            pt, pc, ph = previous
            if None not in (t, pt):
                check(t > pt, 'non-increasing monotonic samples')
            if None not in (cpu, pc, hz, ph):
                check(cpu >= pc and hz == ph, 'CPU counters reset or frequency changed')
        previous = t, cpu, hz
        if 'health' in row:
            check(health_ok(row['health']), 'native health failed or HMI PID changed')

    def metrics(rows):
        if len(rows) < 2:
            return {'samples': len(rows)}
        try:
            times = [float(r['monotonic']) for r in rows]
            rss = [float(r['rss_kib']) for r in rows]
            if not all(math.isfinite(x) for x in times + rss) or times[-1] <= times[0]:
                return {'samples': len(rows), 'metrics_unavailable': True}
            span = times[-1] - times[0]
            quarter = max(1, len(rows) // 4)
            mean_t, mean_r = statistics.mean(times), statistics.mean(rss)
            denom = sum((t - mean_t) ** 2 for t in times)
            slope = sum((t - mean_t) * (r - mean_r) for t, r in zip(times, rss)) / denom * 3600
            return {'samples': len(rows), 'sample_span_seconds': span,
                    'max_sample_gap_seconds': max(b - a for a, b in zip(times, times[1:])),
                    'rss_first_kib': rss[0], 'rss_last_kib': rss[-1], 'rss_min_kib': min(rss),
                    'rss_max_kib': max(rss), 'rss_regression_kib_per_hour': slope,
                    'rss_last_minus_first_quarter_median_kib': statistics.median(rss[-quarter:]) - statistics.median(rss[:quarter]),
                    'cpu_average_one_core_percent': (rows[-1]['cpu_ticks'] - rows[0]['cpu_ticks']) / rows[0]['clock_ticks_per_second'] / span * 100}
        except (KeyError, TypeError, ValueError, ZeroDivisionError):
            return {'samples': len(rows), 'metrics_unavailable': True}

    return {'evidence_integrity': 'REVIEW_REQUIRED' if not issues else 'INCOMPLETE_OR_INVALID',
            'issues': sorted(issues), 'switches': metrics(switches), 'soak': metrics(soak),
            'performance_acceptance': 'PENDING_MANUAL_REVIEW', 'physical_lcd_acceptance': 'NOT_ASSESSED',
            'scope': 'Title-region transitions and sampled process health only; no full-frame FPS, physical touch latency or external business proof. Process continuity relies on the archived runner guards and its provenance.',
            'candidate_acceptance': 'NOT_ASSESSED', 'probe_revision': report.get('probe_revision')}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('evidence_directory', type=Path)
    parser.add_argument('--observation', type=Path, required=True)
    parser.add_argument('--expected-sha', required=True)
    parser.add_argument('--expected-boot', required=True)
    args = parser.parse_args()
    paths = [args.evidence_directory / n for n in ('progress.json', 'switches.jsonl', 'soak.jsonl')] + [args.observation]
    try:
        report = json.loads(paths[0].read_text())
        rows = [[json.loads(line) for line in path.read_text().splitlines()] for path in paths[1:3]]
        result = review(report, *rows, json.loads(paths[3].read_text()), args.expected_sha, args.expected_boot)
        result['input_sha256'] = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    except (OSError, ValueError, TypeError, KeyError, AttributeError) as error:
        result = {'evidence_integrity': 'INCOMPLETE_OR_INVALID', 'parse_error': str(error)}
    print(json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False))
    return 0 if result['evidence_integrity'] == 'REVIEW_REQUIRED' else 1


if __name__ == '__main__':
    raise SystemExit(main())
