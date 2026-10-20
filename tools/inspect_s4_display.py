#!/usr/bin/env python3
"""Read S4 display/Qt state without changing timing, color controls or mounts."""
import argparse
import fcntl
import json
import os
from pathlib import Path
import struct


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expected-cid', required=True)
    args = parser.parse_args()
    compatible = Path('/proc/device-tree/compatible').read_bytes()
    if b'allwinner,t113_s4' not in compatible:
        raise SystemExit('Refused: target is not the S4 board')
    cid = Path('/sys/class/block/mmcblk0/device/cid').read_text().strip()
    if cid != args.expected_cid:
        raise SystemExit('Refused: storage identity changed')
    result = dict(read_only=True, device_compatible=compatible.rstrip(b'\0').decode().split('\0'),
                  cid=cid, boot_id=Path('/proc/sys/kernel/random/boot_id').read_text().strip())
    fd = os.open('/dev/fb0', os.O_RDONLY | os.O_CLOEXEC)
    try:
        raw = bytearray(160)
        fcntl.ioctl(fd, 0x4600, raw, True)
        value = struct.unpack('40I', raw)
        result['framebuffer'] = dict(width=value[0], height=value[1],
            virtual_width=value[2], virtual_height=value[3], xoffset=value[4], yoffset=value[5],
            bits=value[6], red=list(value[8:11]), green=list(value[11:14]),
            blue=list(value[14:17]), alpha=list(value[17:20]), rotate=value[34],
            declared_width_mm=value[23], declared_height_mm=value[22])
    finally:
        os.close(fd)
    result['sysfs'] = {}
    paths = [Path('/sys/class/graphics/fb0') / name
             for name in ['name', 'mode', 'modes', 'virtual_size', 'bits_per_pixel', 'stride']]
    paths += [Path('/sys/class/disp/disp/attr') / name
              for name in ['sys', 'disp', 'xres', 'yres', 'enhance_mode', 'enhance_bright',
                           'enhance_contrast', 'enhance_saturation', 'enhance_edge',
                           'enhance_detail', 'enhance_denoise', 'color_temperature']]
    for path in paths:
        try:
            with path.open() as stream:
                result['sysfs'][str(path)] = stream.read(32769)[:32768].strip()
        except OSError as error:
            result['sysfs'][str(path)] = dict(unavailable_errno=error.errno)
    result['lcd_device_tree'] = []
    for name in Path('/proc/device-tree').rglob('lcd_driver_name'):
        parent = name.parent
        item = dict(path=str(parent), driver=name.read_bytes().rstrip(b'\0').decode())
        for key in ['lcd_if', 'lcd_hv_if', 'lcd_x', 'lcd_y', 'lcd_width', 'lcd_height',
                    'lcd_dclk_freq', 'lcd_hbp', 'lcd_ht', 'lcd_hspw', 'lcd_vbp', 'lcd_vt',
                    'lcd_vspw', 'lcd_frm', 'lcd_io_phase', 'lcd_hv_clk_phase',
                    'lcd_hv_sync_polarity', 'lcd_hv_srgb_seq', 'lcd_rgb_endian',
                    'lcd_gamma_en', 'lcd_cmap_en']:
            prop = parent / key
            if prop.exists():
                raw = prop.read_bytes()
                if len(raw) == 4:
                    item[key] = struct.unpack('>I', raw)[0]
        result['lcd_device_tree'].append(item)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
