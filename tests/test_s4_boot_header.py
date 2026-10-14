import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    'boot_header', Path(__file__).resolve().parents[1] / 'scripts/check_s4_boot_header.py')
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


def boot0(marker=b'RAW\x9b\xe9'):
    data = bytearray(1024)
    data[4:12] = b'eGON.BT0'
    struct.pack_into('<I', data, 16, len(data))
    data[0xf1:0xf6] = marker
    struct.pack_into('<I', data, 12, 0x5f0a6c39)
    checksum = sum(struct.unpack('<256I', data)) & 0xffffffff
    struct.pack_into('<I', data, 12, checksum)
    return data


class BootHeaderGateTests(unittest.TestCase):
    def test_missing_marker_rejected_even_with_valid_checksum(self):
        with self.assertRaisesRegex(ValueError, 'hardware information'):
            gate.check_boot0(boot0(b'\0' * 5), b'RAW\x9b\xe9')

    def test_other_board_marker_rejected(self):
        with self.assertRaisesRegex(ValueError, 'hardware information'):
            gate.check_boot0(boot0(b'RAW\xdd\xe9'), b'RAW\x9b\xe9')

    def test_corruption_rejected(self):
        data = boot0()
        data[500] ^= 1
        with self.assertRaisesRegex(ValueError, 'checksum'):
            gate.check_boot0(data, b'RAW\x9b\xe9')

    def test_final_container_and_truncation(self):
        data = bytearray(3072)
        data[:8] = b'IMAGEWTY'
        struct.pack_into('<II', data, 8, 0x300, 96)
        struct.pack_into('<Q', data, 24, len(data))
        struct.pack_into('<III', data, 56, 1024, 1, 1024)
        struct.pack_into('<II', data, 1024, 0x100, 1024)
        name = b'boot0_sdcard.fex'
        data[1060:1060 + len(name)] = name
        struct.pack_into('<QQQ', data, 1316, 1024, 1024, 2048)
        data[2048:] = boot0()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'test.img'
            path.write_bytes(data)
            self.assertIn('boot0_sdcard.fex', gate.check_image(path, b'RAW\x9b\xe9'))
            path.write_bytes(data[:-1])
            with self.assertRaisesRegex(ValueError, 'size mismatch'):
                gate.check_image(path, b'RAW\x9b\xe9')
