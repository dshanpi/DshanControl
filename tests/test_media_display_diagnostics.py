"""Regression for the actual E failure: a depth footer prefix is not Qt config."""
import importlib.util
from pathlib import Path
import unittest

spec=importlib.util.spec_from_file_location("display_evidence",
    Path(__file__).resolve().parents[1]/"scripts/desktop_display_evidence.py")
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
record=module.qt_layer1_record
QT=("BUF    enable ch[1] lyr[0] z[16] prem[N] a[pixel 255] fmt[  0] "
    "fb[1024, 600;1024, 600;1024, 600] crop[   0,   0,1024, 600] "
    "frame[   0,   0,1024, 600] addr[       0,       0,       0] "
    "flags[0x       0] trd[0,0]")


class DisplayDiagnosticTests(unittest.TestCase):
    def test_actual_E_depth_prefix_does_not_change_following_Qt_record(self):
        self.assertEqual(record("depth[ 0]    "+QT+"\ndepth[ 0] "),
                         record("   "+QT+"\ndepth[ 0] "))

    def test_configuration_changes_are_still_observable(self):
        for old,new in [("z[16]","z[15]"),("fmt[  0]","fmt[  1]"),
                        ("frame[   0,   0,1024, 600]","frame[   0,   0,1280, 720]"),
                        ("addr[       0,","addr[    8192,"),
                        ("a[pixel 255]","a[pixel 128]"),("flags[0x       0]","flags[0x       1]")]:
            with self.subTest(field=old):
                self.assertNotEqual(record(QT),record(QT.replace(old,new)))

    def test_missing_disabled_or_duplicate_Qt_record_is_refused(self):
        for raw in ["",QT.replace("enable","disable"),QT+"\n"+QT,QT.replace("lyr[0]","lyr[1]")]:
            with self.subTest(raw=raw):
                with self.assertRaises(ValueError):record(raw)


if __name__=="__main__":unittest.main()
