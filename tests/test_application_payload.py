"""Application-local component packaging, without secrets or system mutations."""
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from application_payload import payload, component_inputs, COMPONENT_PATHS


class ComponentPayloadTests(unittest.TestCase):
    def artifacts(self, root):
        files = []
        for name in ['hmi', 'desktop', 'media', 'images.so']:
            path = root / name
            path.write_bytes(b'owned build fixture ' + name.encode())
            path.chmod(0o755)
            files.append(path)
        return files

    def test_complete_bundle_keeps_native_components_and_licenses_inside_application(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            hmi, desktop, media, images = self.artifacts(root)
            destination = root / 'payload'
            payload(ROOT, destination, hmi, desktop, media, images)
            for name, source in zip(COMPONENT_PATHS.values(), [desktop, media, images]):
                self.assertEqual(source.read_bytes(), (destination / name).read_bytes())
            self.assertEqual(0o644, (destination / COMPONENT_PATHS['imageformats']).stat().st_mode & 0o777)
            self.assertFalse((destination / 'etc').exists())
            self.assertFalse(list(destination.rglob('desktop.json')))
            self.assertFalse(list(destination.rglob('*runtime*')))
            license_dir = destination / 'share/licenses/qt-imageformats-5.11.3'
            self.assertIn('5.11.3', (license_dir / 'NOTICE').read_text())
            self.assertEqual('5.11.3', json.loads((license_dir / 'SOURCE.json').read_text())['version'])

    def test_incomplete_or_linked_bundle_refuses_before_writing_payload(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            hmi, desktop, media, images = self.artifacts(root)
            destination = root / 'payload'
            for inputs in [(desktop, None, images), (None, media, None)]:
                with self.assertRaises(ValueError):
                    payload(ROOT, destination, hmi, *inputs)
                self.assertFalse(destination.exists())
            link = root / 'linked'; link.symlink_to(desktop)
            with self.assertRaises(ValueError):
                payload(ROOT, destination, hmi, link, media, images)
            self.assertFalse(destination.exists())
            desktop.chmod(0o600)
            with self.assertRaises(ValueError):
                component_inputs(desktop, media, images)

    def test_missing_codec_license_refuses_before_creating_payload(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            hmi, desktop, media, images = self.artifacts(root)
            destination = root / 'payload'
            with self.assertRaisesRegex(ValueError, 'license input'):
                payload(root / 'incomplete-source', destination, hmi, desktop, media, images)
            self.assertFalse(destination.exists())

    def test_legacy_payload_does_not_enable_unrequested_provider(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); hmi, _, _, _ = self.artifacts(root)
            destination = root / 'payload'; payload(ROOT, destination, hmi)
            self.assertEqual(['omnigate-hmi'], sorted(p.name for p in (destination / 'bin').iterdir()))
