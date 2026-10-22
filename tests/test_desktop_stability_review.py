"""Synthetic validator tests; none of these fixtures are device acceptance evidence."""
import copy
import importlib.util
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location('stability_review', Path(__file__).resolve().parents[1] / 'scripts/review_desktop_stability.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class StabilityEvidenceReviewTest(unittest.TestCase):
    def setUp(self):
        self.health = {'ok': True, 'hmi_functional_heartbeat': True, 'processes': {'hmi': 42}}
        def sample(t, label):
            return dict(label=label, monotonic=t, heartbeat_monotonic=t-.5,
                        rss_kib=65536, hwm_kib=66000, mem_available_kib=120000,
                        cpu_ticks=t*3, clock_ticks_per_second=100)
        self.switches = []
        for i in range(1000):
            row = sample(i+1, 'desktop' if i%2 else module.APPS[(i//2)%12])
            row.update({'return_title_seconds': .2} if i%2 else {'drawer_title_seconds': .2, 'application_title_seconds': .2})
            self.switches.append(row)
        self.soak = []
        for i in range(2881):
            row = sample(1001+i*30, 'soak'); row['elapsed_seconds'] = i*30
            if i%10 == 0: row['health'] = copy.deepcopy(self.health)
            self.soak.append(row)
        self.report = dict(state='completed', phase='complete', hmi_sha256='fixture-sha', boot_id='fixture-boot',
                           hmi_pid=42, hmi_start='1', layout_sha256='layout', layout_sha256_after_soak='layout',
                           probe_revision='r21-fast-identity-full-hash-every-10-cycles-or-10-soak-samples',
                           switches_completed=1000, switches_required=1000, switch_result='completed',
                           soak_samples=2881, soak_required_seconds=86400, soak_started_monotonic=1001,
                           soak_elapsed_seconds=86400, switch_finished_monotonic=1000.5, ended_monotonic=87402)
        self.observation = dict(state='completed', current_boot_id='fixture-boot', runner_alive=False, health=self.health)

    def result(self):
        return module.review(self.report, self.switches, self.soak, self.observation, 'fixture-sha', 'fixture-boot')

    def test_complete_fixture_still_requires_manual_review(self):
        r=self.result(); self.assertEqual(r['issues'], []); self.assertEqual(r['evidence_integrity'], 'REVIEW_REQUIRED')
        self.assertEqual(r['performance_acceptance'], 'PENDING_MANUAL_REVIEW')
        self.assertEqual(r['soak']['rss_regression_kib_per_hour'], 0)
        self.assertEqual(r['soak']['cpu_average_one_core_percent'], 3)

    def test_r23_probe_still_requires_manual_review(self):
        self.report['probe_revision'] = 'r23-fast-identity-full-hash-every-10-cycles-or-10-soak-samples'
        result = self.result()
        self.assertEqual(result['issues'], [])
        self.assertEqual(result['evidence_integrity'], 'REVIEW_REQUIRED')
        self.assertEqual(result['performance_acceptance'], 'PENDING_MANUAL_REVIEW')
        self.assertEqual(result['physical_lcd_acceptance'], 'NOT_ASSESSED')
        self.assertEqual(result['candidate_acceptance'], 'NOT_ASSESSED')
        self.assertEqual(result['probe_revision'], self.report['probe_revision'])

    def test_unknown_probe_is_not_accepted_by_prefix(self):
        self.report['probe_revision'] = 'r99-fast-identity-full-hash-every-10-cycles-or-10-soak-samples'
        self.assertIn('probe revision differs; review measurement scope', self.result()['issues'])

    def test_r23_cannot_bypass_duration_and_candidate_guards(self):
        self.report['probe_revision'] = 'r23-fast-identity-full-hash-every-10-cycles-or-10-soak-samples'
        self.report['hmi_sha256'] = 'wrong'
        self.soak = self.soak[:18]
        self.report['soak_samples'] = 18
        issues = self.result()['issues']
        self.assertIn('candidate SHA mismatch', issues)
        self.assertIn('raw soak does not reach 86400 seconds', issues)

    def test_completed_flag_cannot_replace_actual_time(self):
        self.soak=self.soak[:18];self.report['soak_samples']=18
        self.assertIn('raw soak does not reach 86400 seconds', self.result()['issues'])

    def test_duplicate_or_missing_transition_is_rejected(self):
        self.switches[1]=copy.deepcopy(self.switches[0])
        self.assertIn('switch order/title mismatch',self.result()['issues'])
        self.switches.pop();self.assertIn('not exactly 1000 raw transitions',self.result()['issues'])

    def test_restart_and_stale_heartbeat_are_rejected(self):
        self.soak[30]['health']['processes']['hmi']=99
        self.soak[31]['heartbeat_monotonic']-=20
        self.soak[32]['cpu_ticks']=0
        issues=self.result()['issues']
        for expected in ['native health failed or HMI PID changed','stale or future GUI heartbeat','CPU counters reset or frequency changed']:
            self.assertIn(expected,issues)

    def test_wrong_boot_live_runner_and_changed_layout_rejected(self):
        self.observation.update(current_boot_id='new-boot',runner_alive=True)
        self.report['layout_sha256_after_soak']='changed'
        issues=self.result()['issues']
        for expected in ['boot identity mismatch or missing','runner termination not proven','layout preservation not proven']:
            self.assertIn(expected,issues)

    def test_nonfinite_reset_and_missing_health_rejected(self):
        self.soak[40].pop('health');self.soak[41]['rss_kib']=float('nan');self.soak[42]['elapsed_seconds']=0
        issues=self.result()['issues']
        for expected in ['scheduled native health sample missing','missing/nonfinite numeric field: rss_kib','elapsed time reset/inconsistent with monotonic time']:
            self.assertIn(expected,issues)

    def test_recorded_failure_and_wrong_candidate_rejected(self):
        self.report.update(state='failed',error='fixture failure',hmi_sha256='wrong')
        issues=self.result()['issues']
        for expected in ['run is not completed','runner recorded failure','candidate SHA mismatch']:
            self.assertIn(expected,issues)


if __name__ == '__main__':
    unittest.main()
