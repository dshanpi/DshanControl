from types import SimpleNamespace
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
    def boot_fixture(self, target):
        busybox = target / 'bin/busybox'
        busybox.parent.mkdir(parents=True, exist_ok=True)
        busybox.write_text('#!/bin/sh\nexit 0\n')
        busybox.chmod(0o755)
        firmware = target / 'lib/firmware/sdio/aic8800d80'
        firmware.mkdir(parents=True, exist_ok=True)
        for name in rootfs.AIC_D80_FILES:
            (firmware / name).write_bytes(b'fixture firmware: ' + name.encode())

    def fixture(self, base):
        hardware, br = base / 'hardware', base / 'buildroot'
        board = hardware / 'device/config/chips/t113_s4/configs/100ask'
        (board / 'buildroot').mkdir(parents=True)
        (board / 'linux-5.4').mkdir()
        (board / 'sys_config.fex').write_text('storage_type=2\n')
        (board / 'buildroot/sys_partition.fex').write_text(
            '[partition]\nname=rootfs\nsize=1G\n[partition]\nname=UDISK\n')
        (board / 'linux-5.4/env.cfg').write_text(
            'bootcmd=run setargs_mmc boot_normal\n'
            'setargs_mmc=setenv bootargs root=${mmc_root}\nmmc_root=/dev/mmcblk0p1\n')
        (br / 'target/etc/dshancontrol').mkdir(parents=True)
        (br / 'target/etc/dshancontrol/system-release.json').write_text(json.dumps(
            {'app_id': 'dshancontrol', 'board_id': '100ask-t113s4-sdnand'}))
        self.boot_fixture(br / 'target')
        rootfs.finalize(br / 'target')
        (br / '.config').write_text('BR2_TARGET_ROOTFS_EXT2_4=y\nBR2_TARGET_ROOTFS_EXT2_SIZE="512M"\n')
        return hardware, board, br

    def test_preflight_rejects_tampered_storage_policy_and_recovers(self):
        with tempfile.TemporaryDirectory() as temp:
            hardware, _, br = self.fixture(Path(temp))
            for relative in ('etc/init.d/S60mount_udisk', 'usr/lib/dshancontrol-update/storage.py'):
                script = br / 'target' / relative
                original = script.read_bytes()
                script.write_bytes(original + b'\n# storage.py --mount\nmkfs.vfat /dev/by-name/UDISK\n')
                self.assertTrue(any('storage policy' in e for e in audit.audit(hardware, br)['configuration_errors']))
                rootfs.finalize(br / 'target')
                self.assertEqual([], audit.audit(hardware, br)['configuration_errors'])

    def test_preflight_matches_versioned_env_and_final_platform_override(self):
        with tempfile.TemporaryDirectory() as temp:
            hardware, board, br = self.fixture(Path(temp))
            versioned = board / 'linux-5.4/env-5.4.cfg'
            versioned.write_text((board / 'linux-5.4/env.cfg').read_text().replace('p1', 'p9'))
            report = audit.audit(hardware, br)
            self.assertEqual(str(versioned), report['env_file'])
            self.assertIn('MMC root device index differs from selected partition table', report['configuration_errors'])
            # pack copies buildroot/* after selecting a versioned environment.
            (board / 'buildroot/env-5.4.cfg').write_text(versioned.read_text())
            plain = board / 'buildroot/env.cfg'
            plain.write_text((board / 'linux-5.4/env.cfg').read_text())
            self.assertEqual(str(plain), audit.audit(hardware, br)['env_file'])
            self.assertEqual([], audit.audit(hardware, br)['configuration_errors'])

    def test_preflight_fixed_udisk_budget_and_unknown_capacity_remain_draft(self):
        with tempfile.TemporaryDirectory() as temp:
            hardware, board, br = self.fixture(Path(temp))
            partition = board / 'buildroot/sys_partition.fex'
            original = partition.read_text()
            partition.write_text(original + 'size=16M\n')
            report = audit.audit(hardware, br, capacity_bytes=8 * 1024**3)
            self.assertTrue(any('UDISK partition below' in e for e in report['configuration_errors']))
            partition.write_text(original + 'size=256M\n')
            report = audit.audit(hardware, br, capacity_bytes=1024**3 + 288 * 1024**2)
            self.assertEqual([], report['configuration_errors'])
            report = audit.audit(hardware, br)
            self.assertIn('actual SD NAND capacity required', report['pending'])
            self.assertFalse(report['publishable'])

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
            (br / 'target/etc/dshancontrol').mkdir(parents=True)
            (br / 'target/etc/dshancontrol/system-release.json').write_text(json.dumps(
                {'app_id': 'dshancontrol', 'board_id': '100ask-t113s4-sdnand'}))
            self.boot_fixture(br / 'target')
            rootfs.finalize(br / 'target')
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

    def test_incremental_rootfs_requires_real_installed_baseline_and_bounds_space(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)
            with self.assertRaisesRegex(ValueError, 'only to rootfs'):
                build_s4.build_policy(out, 'application', True)
            with self.assertRaisesRegex(ValueError, 'installed Qt'):
                build_s4.build_policy(out, 'rootfs', True)
            br = out / 'buildroot'
            for relative in ['host/bin/qmake', '.config', 'target/usr/lib/libQt5Core.so.5.11.3',
                             'target/lib/libc-2.23.so', 'build/qt5base-5.11.3/.stamp_target_installed']:
                path = br / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('fixture baseline')
            (br / 'host/bin/qmake').chmod(0o755)
            image = br / 'images/rootfs.ext4'
            image.parent.mkdir()
            image.write_bytes(b'invalid image')
            with self.assertRaisesRegex(ValueError, 'raw ext4'):
                build_s4.build_policy(out, 'rootfs', True)
            data = bytearray(4096)
            struct.pack_into('<I', data, 1024 + 4, 4)
            data[1024 + 56:1024 + 58] = b'\x53\xef'
            image.write_bytes(data)
            with patch.object(build_s4.shutil, 'disk_usage', return_value=SimpleNamespace(free=8 * 1024**3)):
                policy = build_s4.build_policy(out, 'rootfs', True)
                self.assertTrue(policy['reused_target_dependencies'])
                self.assertEqual(4 * 1024**3, policy['minimum_free_bytes'])
                self.assertEqual(inputs.sha_file(image), policy['baseline_rootfs_sha256'])
                self.assertEqual(4, len(policy['reused_baseline_files']))
                with self.assertRaisesRegex(ValueError, '16 GiB'):
                    build_s4.build_policy(out, 'rootfs')
            with patch.object(build_s4.shutil, 'disk_usage', return_value=SimpleNamespace(free=3 * 1024**3)):
                with self.assertRaisesRegex(ValueError, '4 GiB'):
                    build_s4.build_policy(out, 'rootfs', True)
            image.unlink()
            outside = out / 'outside.ext4'
            outside.write_bytes(data)
            image.symlink_to(outside)
            with self.assertRaisesRegex(ValueError, 'escapes'):
                build_s4.build_policy(out, 'rootfs', True)

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

    def test_mount_failure_timeout_and_verification_leave_data_untouched_then_recover(self):
        with tempfile.TemporaryDirectory() as temp:
            mountpoint = Path(temp) / 'UDISK'
            mountpoint.mkdir()
            data = mountpoint / 'user-data'
            data.write_bytes(b'preserve user data')
            good = '36 25 179:9 / {} rw - ext4 /dev/mmcblk0p9 rw\n'.format(mountpoint)
            for error in (subprocess.CalledProcessError(1, 'mount'), subprocess.TimeoutExpired('mount', 30), None):
                calls = []
                def run(argv, **kwargs):
                    calls.append(argv)
                    if argv[0] == 'blkid':
                        return subprocess.CompletedProcess(argv, 0, '/dev/mmcblk0p9: TYPE="ext4"')
                    if error:
                        raise error
                    return subprocess.CompletedProcess(argv, 0)
                with patch.object(storage, 'MOUNT', mountpoint), \
                        patch.object(storage, 'storage_device', return_value=(Path('/dev/mmcblk0p9'), '179:9')), \
                        patch.object(Path, 'read_text', return_value=''), \
                        patch.object(storage.subprocess, 'run', side_effect=run):
                    with self.assertRaises((subprocess.SubprocessError, ValueError)):
                        storage.ensure_storage(mount=True)
                self.assertEqual(['blkid', 'mount'], [a[0] for a in calls])
                self.assertEqual(b'preserve user data', data.read_bytes())
            with patch.object(storage, 'MOUNT', mountpoint), \
                    patch.object(storage, 'storage_device', return_value=(Path('/dev/mmcblk0p9'), '179:9')), \
                    patch.object(Path, 'read_text', side_effect=['', good]), \
                    patch.object(storage.subprocess, 'run', side_effect=run):
                storage.ensure_storage(mount=True)
            self.assertEqual(b'preserve user data', data.read_bytes())

    def test_postbuild_refuses_parent_symlinks_before_any_replacement(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            _, _, br = self.fixture(base)
            target = br / 'target'
            before = (target / 'etc/init.d/S60mount_udisk').read_bytes()
            outside = base / 'outside'
            outside.mkdir()
            policy = target / 'usr/lib/dshancontrol-update/storage.py'
            policy.unlink()
            policy.parent.rmdir()
            policy.parent.symlink_to(outside, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, 'symlink'):
                rootfs.finalize(target)
            self.assertEqual([], list(outside.iterdir()))
            self.assertEqual(before, (target / 'etc/init.d/S60mount_udisk').read_bytes())

    def test_postbuild_null_access_preserves_vendor_rules_and_audit_rejects_regression(self):
        with tempfile.TemporaryDirectory() as temp:
            hardware, _, br = self.fixture(Path(temp))
            config = br / 'target/etc/mdev.conf'
            vendor = b'# vendor hotplug rules\nsd[a-z] 0:0 0660 @/etc/mdev/insert\n'
            config.write_bytes(vendor)
            self.assertTrue(any('/dev/null access' in error
                                for error in audit.audit(hardware, br)['configuration_errors']))
            rootfs.finalize(br / 'target')
            self.assertEqual(b'null 0:0 0666\n' + vendor, config.read_bytes())
            self.assertEqual([], audit.audit(hardware, br)['configuration_errors'])
            first = config.read_bytes()
            rootfs.finalize(br / 'target')
            self.assertEqual(first, config.read_bytes())
            outside = Path(temp) / 'outside'
            outside.write_bytes(vendor)
            config.unlink()
            config.symlink_to(outside)
            boot = br / 'target/etc/init.d/S60mount_udisk'
            boot.write_bytes(b'unchanged before complete preflight')
            with self.assertRaisesRegex(ValueError, 'symlink'):
                rootfs.finalize(br / 'target')
            self.assertEqual(vendor, outside.read_bytes())
            self.assertEqual(b'unchanged before complete preflight', boot.read_bytes())

    def test_postbuild_replaces_late_vendor_formatter_and_rejects_wrong_board(self):
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp)
            contract = target / 'etc/dshancontrol/system-release.json'
            contract.parent.mkdir(parents=True)
            contract.write_text(json.dumps({'app_id':'dshancontrol', 'board_id':'100ask-t113s4-sdnand'}))
            init = target / 'etc/init.d/S60mount_udisk'
            init.parent.mkdir(parents=True)
            init.write_text('mkfs.vfat /dev/by-name/UDISK')
            self.boot_fixture(target)
            rootfs.finalize(target)
            self.assertIn('storage.py --mount', init.read_text())
            self.assertNotIn('mkfs', init.read_text())
            self.assertTrue(os.access(str(init), os.X_OK))
            contract.write_text('{"app_id":"dshancontrol","board_id":"t153"}')
            with self.assertRaises(ValueError):
                rootfs.finalize(target)

    def test_clean_ext4_postbuild_installs_boot_entry_and_audit_rejects_other_entry(self):
        with tempfile.TemporaryDirectory() as temp:
            hardware, _, br = self.fixture(Path(temp))
            init = br / 'target/init'
            self.assertEqual('bin/busybox', os.readlink(str(init)))
            rootfs.finalize(br / 'target')
            self.assertEqual([], audit.audit(hardware, br)['configuration_errors'])
            init.unlink()
            self.assertTrue(any('init=/init' in p for p in audit.audit(hardware, br)['pending']))
            init.symlink_to('/sbin/init')
            self.assertTrue(any('S4 /init' in e for e in audit.audit(hardware, br)['configuration_errors']))

    def test_postbuild_maps_the_actual_sdio_firmware_and_audit_detects_missing_alias(self):
        with tempfile.TemporaryDirectory() as temp:
            hardware, _, br = self.fixture(Path(temp))
            target = br / 'target'
            alias = target / 'lib/firmware/aic8800d80'
            self.assertEqual('sdio/aic8800d80', os.readlink(str(alias)))
            self.assertTrue((alias / 'fw_patch_table_8800d80_u02.bin').is_file())
            rootfs.finalize(target)
            alias.unlink()
            self.assertTrue(any('firmware alias' in e for e in audit.audit(hardware, br)['configuration_errors']))
            rootfs.finalize(target)
            self.assertEqual([], audit.audit(hardware, br)['configuration_errors'])

    def test_bad_firmware_or_alias_refuses_before_replacing_any_policy(self):
        for bad in ('missing-u02', 'empty', 'file-symlink', 'directory-symlink',
                    'parent-symlink', 'wrong-alias', 'existing-directory'):
            with self.subTest(bad=bad), tempfile.TemporaryDirectory() as temp:
                base = Path(temp)
                _, _, br = self.fixture(base)
                target = br / 'target'
                policy = target / 'etc/init.d/S60mount_udisk'
                policy.write_bytes(b'preserve policy until firmware preflight passes')
                alias = target / 'lib/firmware/aic8800d80'
                directory = target / 'lib/firmware/sdio/aic8800d80'
                file = directory / 'fw_patch_table_8800d80_u02.bin'
                outside = base / 'outside'
                outside.mkdir()
                if bad in ('missing-u02', 'file-symlink'):
                    file.unlink()
                    if bad == 'file-symlink':
                        file.symlink_to(outside / 'firmware')
                elif bad == 'empty':
                    file.write_bytes(b'')
                elif bad in ('directory-symlink', 'parent-symlink'):
                    import shutil
                    path = directory if bad == 'directory-symlink' else directory.parent
                    shutil.move(str(path), str(outside / 'original'))
                    path.symlink_to(outside / 'original', target_is_directory=True)
                else:
                    alias.unlink()
                    if bad == 'wrong-alias':
                        alias.symlink_to('/tmp/untrusted-firmware')
                    else:
                        alias.mkdir()
                with self.assertRaises(ValueError):
                    rootfs.finalize(target)
                self.assertEqual(b'preserve policy until firmware preflight passes', policy.read_bytes())

    def test_postbuild_boot_preflight_preserves_other_files(self):
        for bad in ('missing-busybox', 'non-executable', 'busybox-symlink',
                    'bin-symlink', 'init-file', 'init-symlink'):
            with self.subTest(bad=bad), tempfile.TemporaryDirectory() as temp:
                base = Path(temp)
                _, _, br = self.fixture(base)
                target = br / 'target'
                policy = target / 'etc/init.d/S60mount_udisk'
                policy.write_bytes(b'unchanged until all destinations are valid')
                outside = base / 'outside'
                outside.mkdir()
                busybox = target / 'bin/busybox'
                init = target / 'init'
                if bad in ('missing-busybox', 'busybox-symlink'):
                    busybox.unlink()
                    if bad == 'busybox-symlink':
                        busybox.symlink_to('/bin/busybox')
                elif bad == 'non-executable':
                    busybox.chmod(0o644)
                elif bad == 'bin-symlink':
                    busybox.unlink()
                    busybox.parent.rmdir()
                    busybox.parent.symlink_to(outside, target_is_directory=True)
                else:
                    init.unlink()
                    if bad == 'init-file':
                        init.write_bytes(b'preserve existing init')
                    else:
                        init.symlink_to('/sbin/init')
                with self.assertRaises(ValueError):
                    rootfs.finalize(target)
                self.assertEqual(b'unchanged until all destinations are valid', policy.read_bytes())
                self.assertEqual([], list(outside.iterdir()))
                if bad == 'init-file':
                    self.assertEqual(b'preserve existing init', init.read_bytes())

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
