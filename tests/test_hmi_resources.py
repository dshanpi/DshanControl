"""Resource checks exercise the shared factory/application payload path."""
import json
from pathlib import Path
import shutil
import sys
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from check_hmi_resources import verify
from install_s4_runtime import payload


class HmiResourceTests(unittest.TestCase):
    def test_current_resources_verified(self):
        self.assertEqual('Dshan Sans', verify(ROOT)['family'])

    def test_installer_rejects_corruption_without_removing_previous_factory(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            source, target = base / 'source', base / 'target'
            hmi = source / 'components/hmi/src'
            shutil.copytree(str(ROOT / 'components/hmi/src/fonts'), str(hmi / 'fonts'))
            shutil.copy2(str(ROOT / 'components/hmi/src/icons.qrc'), str(hmi / 'icons.qrc'))
            (hmi / 'fonts/DshanSans-Bold.otf').write_bytes(b'corrupt')
            previous = target / 'usr/share/dshancontrol/factory/installed.json'
            previous.parent.mkdir(parents=True)
            previous.write_bytes(b'previous factory fixture')
            result = subprocess.run([sys.executable, str(ROOT / 'scripts/install_s4_runtime.py'),
                                     '--source', str(source), '--target', str(target),
                                     '--hmi', str(base / 'not-yet-read-hmi')],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertNotEqual(0, result.returncode)
            self.assertIn(b'font hash mismatch', result.stderr)
            self.assertEqual(b'previous factory fixture', previous.read_bytes())

    def test_corrupt_or_missing_resource_rejected_before_payload_writes(self):
        for case in ('font', 'license', 'manifest', 'qrc', 'missing'):
            with self.subTest(case=case), tempfile.TemporaryDirectory() as tmp:
                base = Path(tmp)
                source = base / 'source'
                hmi = source / 'components/hmi/src'
                shutil.copytree(str(ROOT / 'components/hmi/src/fonts'), str(hmi / 'fonts'))
                shutil.copy2(str(ROOT / 'components/hmi/src/icons.qrc'), str(hmi / 'icons.qrc'))
                if case == 'font':
                    font = hmi / 'fonts/DshanSans-Regular.otf'
                    font.write_bytes(font.read_bytes()[:-1] + b'!')
                elif case == 'license':
                    (hmi / 'fonts/OFL.txt').write_text('missing license')
                elif case == 'manifest':
                    path = hmi / 'fonts/manifest.json'
                    value = json.loads(path.read_text())
                    value['fonts'][0]['output'] = '../outside.otf'
                    path.write_text(json.dumps(value))
                elif case == 'qrc':
                    (hmi / 'icons.qrc').write_text('<RCC/>')
                else:
                    (hmi / 'fonts/DshanSans-Bold.otf').unlink()
                destination = base / 'payload'
                with self.assertRaises((ValueError, FileNotFoundError)):
                    payload(source, destination, base / 'not-yet-read-hmi')
                self.assertFalse(destination.exists())

    def test_search_font_inputs_rejected_before_shared_payload_writes(self):
        from application_payload import payload as application_payload
        for case in ('font', 'policy', 'missing_font', 'missing_record',
                     'qrc_font', 'qrc_policy', 'unsafe_name', 'symlink'):
            for assemble in (application_payload,):
                with self.subTest(case=case, assembler=assemble.__module__), tempfile.TemporaryDirectory() as tmp:
                    base = Path(tmp)
                    source = base / 'source'
                    hmi = source / 'components/hmi/src'
                    shutil.copytree(str(ROOT / 'components/hmi/src/fonts'), str(hmi / 'fonts'))
                    shutil.copy2(str(ROOT / 'components/hmi/src/icons.qrc'), str(hmi / 'icons.qrc'))
                    font = hmi / 'fonts/DshanSearch-Regular.otf'
                    if case == 'font':
                        font.write_bytes(b'wrong search font')
                    elif case == 'policy':
                        (hmi / 'fonts/search-font.conf').write_bytes(b'wrong policy')
                    elif case == 'missing_font':
                        font.unlink()
                    elif case in ('missing_record', 'unsafe_name'):
                        path = hmi / 'fonts/manifest.json'
                        value = json.loads(path.read_text())
                        if case == 'missing_record':
                            value.pop('search_font')
                        else:
                            value['search_font']['output'] = '../outside.otf'
                        path.write_text(json.dumps(value))
                    elif case in ('qrc_font', 'qrc_policy'):
                        path = hmi / 'icons.qrc'
                        name = 'DshanSearch-Regular.otf' if case == 'qrc_font' else 'search-font.conf'
                        path.write_text(path.read_text().replace('<file>fonts/' + name + '</file>', ''))
                    else:
                        font.unlink()
                        font.symlink_to(ROOT / 'components/hmi/src/fonts/DshanSearch-Regular.otf')
                    destination = base / 'payload'
                    with self.assertRaises((ValueError, FileNotFoundError)):
                        assemble(source, destination, base / 'not-yet-read-hmi')
                    self.assertFalse(destination.exists())

    def test_installer_rejects_search_font_corruption_before_factory_replacement(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            source, target = base / 'source', base / 'target'
            hmi = source / 'components/hmi/src'
            shutil.copytree(str(ROOT / 'components/hmi/src/fonts'), str(hmi / 'fonts'))
            shutil.copy2(str(ROOT / 'components/hmi/src/icons.qrc'), str(hmi / 'icons.qrc'))
            (hmi / 'fonts/DshanSearch-Regular.otf').write_bytes(b'corrupt')
            previous = target / 'usr/share/dshancontrol/factory/installed.json'
            previous.parent.mkdir(parents=True)
            previous.write_bytes(b'previous factory fixture')
            result = subprocess.run([sys.executable, str(ROOT / 'scripts/install_s4_runtime.py'),
                                     '--source', str(source), '--target', str(target),
                                     '--hmi', str(base / 'not-yet-read-hmi')],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertNotEqual(0, result.returncode)
            self.assertIn(b'search font resource hash mismatch', result.stderr)
            self.assertEqual(b'previous factory fixture', previous.read_bytes())
