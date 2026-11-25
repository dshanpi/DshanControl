"""Release closure rejects corruption and internally inconsistent provenance."""
import gzip
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('delivery', ROOT / 'scripts/verify_imx6ull_delivery.py')
delivery = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(delivery)


class DeliveryVerificationTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.commits = ['a' * 40, 'b' * 40, 'c' * 40]
        self.source = 'DshanControl-v0.1-source.tar.gz'
        self.archive(self.source, self.commits[0], 'LICENSE', b'fixture')
        self.recipe = {'application_commit': self.commits[0], 'bsp_commit': self.commits[2],
                       'application_source_sha256': self.sha(self.source)}
        self.hardware()
        self.archive('bsp-source.tar.gz', self.commits[2], 'LICENSE', b'fixture')
        raw = b'test image bytes'
        (self.root / 'image.img.gz').write_bytes(gzip.compress(raw))
        self.lock = {'release_kind': 'system', 'auto_update_eligible': False,
                     'application_commit': self.commits[0], 'integration_commit': self.commits[1],
                     'bsp_commit': self.commits[2], 'image': {'filename': 'image.img.gz',
                     'sha256': self.sha('image.img.gz'), 'uncompressed_sha256': hashlib.sha256(raw).hexdigest(),
                     'uncompressed_bytes': len(raw)}}
        (self.root / 'validation-imx6ull-pro.json').write_text(json.dumps({
            'image_sha256': self.lock['image']['uncompressed_sha256'], 'stage': 'draft'}))
        self.seal()

    def sha(self, name):
        return hashlib.sha256((self.root / name).read_bytes()).hexdigest()

    def archive(self, name, commit, member, data):
        with tarfile.open(self.root / name, 'w:gz', format=tarfile.PAX_FORMAT,
                          pax_headers={'comment': commit}) as tar:
            entry = tarfile.TarInfo(member)
            entry.size = len(data)
            tar.addfile(entry, io.BytesIO(data))

    def hardware(self):
        self.archive('hardware-source.tar.gz', self.commits[1], 'dshancontrol/release-inputs.json',
                     json.dumps(self.recipe).encode())

    def seal(self):
        self.lock['artifacts'] = [{'filename': p.name, 'sha256': self.sha(p.name), 'bytes': p.stat().st_size}
                                  for p in self.root.iterdir() if p.name not in ('SHA256SUMS', 'release-lock.json')]
        (self.root / 'release-lock.json').write_text(json.dumps(self.lock))
        (self.root / 'SHA256SUMS').write_text(''.join(self.sha(p.name) + '  ' + p.name + '\n'
            for p in self.root.iterdir() if p.name != 'SHA256SUMS'))

    def test_complete_delivery(self):
        self.assertTrue(delivery.verify(self.root)['passed'])

    def test_corrupt_download_is_rejected(self):
        (self.root / self.source).write_bytes(b'corrupt')
        with self.assertRaisesRegex(ValueError, 'Checksum mismatch'):
            delivery.verify(self.root)

    def test_rehashed_wrong_hardware_recipe_is_rejected(self):
        self.recipe['application_commit'] = 'd' * 40
        self.hardware()
        self.seal()
        with self.assertRaisesRegex(ValueError, 'Hardware recipe mismatch'):
            delivery.verify(self.root)

    def test_wrong_uncompressed_image_is_rejected(self):
        self.lock['image']['uncompressed_sha256'] = '0' * 64
        self.seal()
        with self.assertRaisesRegex(ValueError, 'Uncompressed image mismatch'):
            delivery.verify(self.root)

    def test_rehashed_wrong_git_archive_is_rejected(self):
        self.archive('bsp-source.tar.gz', 'e' * 40, 'LICENSE', b'fixture')
        self.seal()
        with self.assertRaisesRegex(ValueError, 'Git source identity mismatch'):
            delivery.verify(self.root)

    def test_extra_asset_is_rejected(self):
        (self.root / 'extra.bin').write_bytes(b'extra')
        with self.assertRaisesRegex(ValueError, 'Extra or missing'):
            delivery.verify(self.root)


if __name__ == '__main__':
    unittest.main()
