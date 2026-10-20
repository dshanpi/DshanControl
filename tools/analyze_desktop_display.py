#!/usr/bin/env python3
"""Compare unchanged artwork regions without treating live data as a fixture.

This compares PNG pixels only. It cannot verify LCD color, physical aspect,
touch, or runtime display-layer settings.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', required=True, type=Path)
    parser.add_argument('--framebuffer', required=True, type=Path)
    parser.add_argument('--font', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    reference = Image.open(args.reference).convert('RGB')
    board = Image.open(args.framebuffer).convert('RGB')
    if reference.size != (1024, 600) or board.size != reference.size:
        raise SystemExit('Expected two unscaled 1024x600 images')
    regions = {
        'brand_text': (55, 11, 216, 43),
        'workspace_heading': (20, 72, 190, 105),
        'first_card_heading': (23, 119, 489, 166),
        'between_card_rows': (15, 344, 1005, 356),
        'bottom_background': (0, 530, 1024, 542),
    }
    results = {}
    for name, bounds in regions.items():
        first = list(reference.crop(bounds).getdata())
        second = list(board.crop(bounds).getdata())
        error = sum(sum((x - y) ** 2 for x, y in zip(a, b)) for a, b in zip(first, second))
        results[name] = dict(bounds=list(bounds), pixels=len(first),
            identical_pixels=sum(a == b for a, b in zip(first, second)),
            rmse_rgb=math.sqrt(error / (len(first) * 3)))
    positions = [(10, 10), (30, 130), (120, 120), (20, 100), (512, 55), (500, 530)]
    samples = [dict(x=x, y=y, reference=list(reference.getpixel((x, y))),
                    framebuffer=list(board.getpixel((x, y)))) for x, y in positions]
    args.output.mkdir(parents=True, exist_ok=True)
    report = dict(reference_sha256=digest(args.reference),
                  framebuffer_sha256=digest(args.framebuffer),
                  image_size=list(reference.size), static_regions=results, color_samples=samples,
                  scope='prior framebuffer PNG versus approved example, with live-content regions excluded',
                  physical_display_acceptance='OPEN; user reports color/proportion mismatch')
    (args.output / 'framebuffer-comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    sheet = Image.new('RGB', (2080, 664), '#e6edf6')
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.truetype(str(args.font), 22)
    draw.text((8, 10), '批准原稿（示例数据）', font=font, fill='#142c4b')
    draw.text((1048, 10), '板端 framebuffer（真实空态；不是屏幕实拍）', font=font, fill='#142c4b')
    sheet.paste(reference, (8, 48))
    sheet.paste(board, (1048, 48))
    sheet.save(args.output / 'UI原稿与板端帧缓冲对照.png')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
