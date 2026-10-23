import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from check_s4_media import verify
from s4_media_fixture import media_elf


class S4MediaTests(unittest.TestCase):
    def test_real_readelf_requires_linked_backend_not_appended_names(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'worker'
            path.write_bytes(media_elf()); path.chmod(0o755)
            result = verify(path)
            self.assertTrue(result['cedar_linkage'])
            self.assertEqual('not-run', result['board_validation'])
            path.write_bytes(media_elf(cedar=False) + b'TPlayerCreate libtplayer.so libcdx_base.so')
            with self.assertRaisesRegex(ValueError, 'lacks Cedar playback'):
                verify(path)

    def test_rejects_wrong_abi_truncation_nonexecutable_and_links(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'worker'
            for contents in (media_elf(machine=62), media_elf(flags=0x5000400),
                             media_elf(flags=0x4000200), b'ELF', b'#!/bin/sh\nexit 0\n'):
                path.write_bytes(contents); path.chmod(0o755)
                with self.subTest(contents=contents[:52]):
                    with self.assertRaises(ValueError): verify(path)
            path.write_bytes(media_elf()); path.chmod(0o644)
            with self.assertRaises(ValueError): verify(path)
            path.chmod(0o755)
            link = Path(temp) / 'linked'; link.symlink_to(path)
            with self.assertRaises(ValueError): verify(link)

    def test_missing_inspector_and_artifact_replacement_fail_closed(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'worker'; path.write_bytes(media_elf()); path.chmod(0o755)
            with self.assertRaisesRegex(ValueError, 'cannot inspect'):
                verify(path, str(Path(temp) / 'no-readelf'))
            import subprocess
            real = subprocess.run
            def replace_after_inspection(*args, **kwargs):
                result = real(*args, **kwargs)
                if '--dyn-syms' in args[0]: path.write_bytes(media_elf(cedar=False))
                return result
            with patch('check_s4_media.subprocess.run', side_effect=replace_after_inspection):
                with self.assertRaisesRegex(ValueError, 'changed during inspection'): verify(path)
