import importlib.util
import json
import os
import subprocess
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import audit_s4_system as audit
import capture_sdk_inputs as inputs
import finalize_s4_rootfs as rootfs
import build_s4

spec = importlib.util.spec_from_file_location('s4_storage', ROOT / 'boards/t113s4-sdnand/runtime/storage.py')
storage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(storage)


class S4SystemTests(unittest.TestCase):
    def test_preflight_selects_buildroot_partition_and_rejects_oversized_rootfs(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            board = base / 'hardware/device/config/chips/t113_s4/configs/100ask'
            for folder in ('linux-5.4', 'buildroot'):
                (board / folder).mkdir(parents=True)
            (board / 'sys_config.fex').write_text('storage_type=2\n')
            (board / 'linux-5.4/sys_partition.fex').write_text('[partition]\nname=rootfs\nsize=104M\n')
            partition = board / 'buildroot/sys_partition.fex'
            partition.write_text('[partition]\nname=rootfs\nsize=1G\n[partition]\nname=UDISK\n')
            (board / 'linux-5.4/env.cfg').write_text('bootcmd=run setargs_mmc boot_normal\n'
                                                     'setargs_mmc=setenv bootargs root=${mmc_root}\nmmc_root=/dev/mmcblk0p1\n')
            br = base / 'buildroot'
            (br / 'target/etc/init.d').mkdir(parents=True)
            (br / 'target/etc/init.d/S60mount_udisk').write_text('storage.py --mount')
            (br / '.config').write_text('BR2_TARGET_ROOTFS_EXT2_4=y\nBR2_TARGET_ROOTFS_EXT2_SIZE="512M"\n')
            value = audit.audit(base / 'hardware', br, capacity_bytes=2 * 1024**3)
            self.assertEqual(str(partition), value['partition_file'])
            self.assertEqual([], value['configuration_errors'])
            self.assertFalse(value['publishable'])
            (br / '.config').write_text('BR2_TARGET_ROOTFS_EXT2_4=y\nBR2_TARGET_ROOTFS_EXT2_SIZE="2G"\n')
            value = audit.audit(base / 'hardware', br, capacity_bytes=1024**3)
            self.assertIn('configured rootfs exceeds selected partition', value['configuration_errors'])
            self.assertTrue(any('capacity below' in e for e in value['configuration_errors']))

    def test_rootfs_checks_raw_ext4_superblock_and_truncation(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'rootfs.ext4'
            data = bytearray(4096)
            data[1024 + 56:1024 + 58] = b'\x53\xef'
            struct.pack_into('<I', data, 1024 + 4, 4)
            path.write_bytes(data)
            self.assertEqual(4096, audit.ext4_image_bytes(path))
            struct.pack_into('<I', data, 1024 + 4, 128)
            path.write_bytes(data)
            with self.assertRaisesRegex(ValueError, 'truncated'):
                audit.ext4_image_bytes(path)
            path.write_bytes(b'not a filesystem')
            with self.assertRaisesRegex(ValueError, 'raw ext4'):
                audit.ext4_image_bytes(path)

    def test_build_runner_pins_board_and_rejects_modified_snapshot(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)
            source = out / 'external/app-source/file'
            source.parent.mkdir(parents=True)
            source.write_text('original')
            (out / 'board').mkdir()
            (out / 'build-provenance.json').write_text(json.dumps({
                'board':'t113_s4/100ask', 'storage':'sdnand', 'buildroot':'201902', 'sdk_path':'/fixture/sdk',
                'application_snapshot_files':{'file': inputs.sha_file(source)}, 'hardware_board_files':{}}))
            with patch.dict(os.environ, {'LICHEE_IC':'t113', 'TINA_TARGET':'wrong-board'}):
                argv, env, _ = build_s4.command(out, 4, 'rootfs')
            self.assertEqual('all', argv[-1])
            self.assertIn('LICHEE_IC=t113_s4', argv)
            self.assertEqual('t113_s4', env['LICHEE_IC'])
            self.assertNotIn('TINA_TARGET', env)
            source.write_text('changed')
            with self.assertRaises(ValueError):
                build_s4.command(out, 4, 'application')

    def test_partition_units_and_commented_alternatives(self):
        layout = audit.partitions('[mbr]\nsize=16384\n[partition]\nname=rootfs\nsize=1G\n'
                                  ';[partition]\n;name=wrong\n[partition]\nname=UDISK\n')
        self.assertEqual(1024**3, layout[0]['size_bytes'])
        self.assertIsNone(layout[1]['size_bytes'])
        for text in ('[partition]\nname=x\nsize=1B', '[partition]\nname=x\n',
                     '[partition]\nname=x\nname=y\nsize=10',
                     '[partition]\nname=x\nsize=10\n[partition]\nname=x\nsize=10'):
            with self.assertRaises(ValueError):
                audit.partitions(text)

    def test_storage_resolves_boot_map_and_rejects_guessing_or_ambiguous_identity(self):
        self.assertEqual(Path('/dev/mmcblk0p9'), storage.device_from_cmdline(
            'console=ttyS3 partitions=rootfs@mmcblk0p5:UDISK@mmcblk0p9'))
        for text in ('', 'partitions=UDISK@mtdblock9', 'partitions=UDISK@../../sda',
                     'partitions=UDISK@mmcblk0p9:UDISK@mmcblk1p9',
                     'partitions=UDISK@mmcblk0p9 partitions=UDISK@mmcblk1p9'):
            with self.assertRaises(ValueError):
                storage.device_from_cmdline(text)

    def test_mount_requires_exact_device_ext4_and_rw(self):
        line = '36 25 179:9 / /mnt/UDISK rw,relatime - ext4 /dev/mmcblk0p9 rw\n'
        self.assertTrue(storage.check_mount(line, '179:9'))
        self.assertFalse(storage.check_mount('', '179:9'))
        for bad in (line.replace('ext4', 'vfat'), line.replace('179:9', '179:5'),
                    line.replace('rw,relatime', 'ro,relatime'), line.replace(' / /mnt', ' /sub /mnt'),
                    line + line, line[:-3] + 'ro\n'):
            with self.assertRaises(ValueError):
                storage.check_mount(bad, '179:9')

    def test_vfat_or_unknown_storage_is_never_formatted(self):
        for filesystem in ('vfat', ''):
            with patch.object(storage, 'storage_device', return_value=(Path('/dev/mmcblk0p9'), '179:9')), \
                    patch.object(Path, 'read_text', return_value=''), \
                    patch.object(Path, 'is_symlink', return_value=False), \
                    patch.object(storage.subprocess, 'run') as run:
                run.return_value.stdout = '/dev/mmcblk0p9: TYPE="' + filesystem + '"'
                with self.assertRaises(ValueError):
                    storage.ensure_storage(mount=True)
                self.assertEqual(1, run.call_count)
                self.assertEqual(['blkid', '/dev/mmcblk0p9'], run.call_args[0][0])

    def test_missing_mount_never_initializes_rootfs(self):
        with patch.object(storage, 'storage_device', return_value=(Path('/dev/mmcblk0p9'), '179:9')), \
                patch.object(Path, 'read_text', return_value=''), \
                patch.object(Path, 'is_symlink', return_value=False), \
                patch.object(storage.subprocess, 'run') as run:
            with self.assertRaises(ValueError):
                storage.ensure_storage()
            run.assert_not_called()

    def test_postbuild_replaces_late_vendor_formatter_and_rejects_wrong_board(self):
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp)
            contract = target / 'etc/dshancontrol/system-release.json'
            contract.parent.mkdir(parents=True)
            contract.write_text(json.dumps({'app_id':'dshancontrol', 'board_id':'100ask-t113s4-sdnand'}))
            init = target / 'etc/init.d/S60mount_udisk'
            init.parent.mkdir(parents=True)
            init.write_text('mkfs.vfat /dev/by-name/UDISK')
            rootfs.finalize(target)
            self.assertIn('storage.py --mount', init.read_text())
            self.assertNotIn('mkfs', init.read_text())
            self.assertTrue(os.access(str(init), os.X_OK))
            contract.write_text('{"app_id":"dshancontrol","board_id":"t153"}')
            with self.assertRaises(ValueError):
                rootfs.finalize(target)

    def test_sdk_capture_records_dirty_inputs_and_missing_component(self):
        with tempfile.TemporaryDirectory() as temp:
            sdk = Path(temp)
            (sdk / '.repo').mkdir()
            (sdk / '.repo/manifest.xml').write_text('<manifest><project name="one" path="one"/>'
                                                   '<project name="missing" path="missing"/></manifest>')
            component = sdk / 'one'
            subprocess.run(['git', 'init', '-q', str(component)], check=True)
            (component / 'input').write_text('initial')
            subprocess.run(['git', '-C', str(component), 'add', 'input'], check=True)
            subprocess.run(['git', '-C', str(component), '-c', 'user.name=Test', '-c',
                            'user.email=test@example.invalid', 'commit', '-qm', 'baseline'], check=True)
            (component / 'input').write_text('modified')
            (component / 'new').write_text('untracked')
            value = inputs.capture(sdk)
            self.assertEqual(2, value['component_count'])
            self.assertFalse(value['commits_complete'])
            self.assertFalse(value['complete_reproduction_bundle'])
            item = next(p for p in value['projects'] if p['path'] == 'one')
            self.assertTrue(item['tracked_dirty'])
            self.assertIn('new', item['untracked'])
            self.assertEqual('modified', (component / 'input').read_text())

    def test_sdk_manifest_include_and_path_rejection(self):
        with tempfile.TemporaryDirectory() as temp:
            sdk = Path(temp)
            (sdk / '.repo').mkdir()
            manifest = sdk / '.repo/manifest.xml'
            manifest.write_text('<manifest><include name="other.xml"/></manifest>')
            (sdk / '.repo/other.xml').write_text('<manifest><project name="one" path="one"/></manifest>')
            self.assertEqual({'one':'one'}, inputs.project_paths(sdk)[0])
            for body in ('<project name="bad" path="../outside"/>', '<include name="manifest.xml"/>',
                         '<extend-project name="one"/>'):
                manifest.write_text('<manifest>' + body + '</manifest>')
                with self.assertRaises(ValueError):
                    inputs.project_paths(sdk)


if __name__ == '__main__':
    unittest.main()
