#!/usr/bin/env python3
import math
import os
import struct
import zlib


SIZE = 1024
SCALE = 3
W = SIZE * SCALE
H = SIZE * SCALE


def rgba(hex_value, alpha=255):
    hex_value = hex_value.lstrip("#")
    return (
        int(hex_value[0:2], 16),
        int(hex_value[2:4], 16),
        int(hex_value[4:6], 16),
        alpha,
    )


bg = rgba("#171b1b")
panel = rgba("#222727")
panel_light = rgba("#303b35")
mint = rgba("#94bba4")
mint_bright = rgba("#b6dfc5")
blue = rgba("#46acd3")
warm = rgba("#ff8f7a")
text = rgba("#e9eeeb")

pixels = bytearray(bg * (W * H))


def blend(dst, src):
    a = src[3] / 255.0
    return tuple(int(src[i] * a + dst[i] * (1 - a)) for i in range(3)) + (255,)


def set_pixel(x, y, color):
    if 0 <= x < W and 0 <= y < H:
        idx = (y * W + x) * 4
        pixels[idx : idx + 4] = bytes(blend(tuple(pixels[idx : idx + 4]), color))


def fill_rounded_rect(x, y, w, h, r, color):
    x = int(x * SCALE)
    y = int(y * SCALE)
    w = int(w * SCALE)
    h = int(h * SCALE)
    r = int(r * SCALE)
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            dx = max(x + r - xx, 0, xx - (x + w - r - 1))
            dy = max(y + r - yy, 0, yy - (y + h - r - 1))
            if dx * dx + dy * dy <= r * r:
                set_pixel(xx, yy, color)


def fill_circle(cx, cy, radius, color):
    cx = int(cx * SCALE)
    cy = int(cy * SCALE)
    radius = int(radius * SCALE)
    rr = radius * radius
    for yy in range(cy - radius, cy + radius + 1):
        for xx in range(cx - radius, cx + radius + 1):
            if (xx - cx) * (xx - cx) + (yy - cy) * (yy - cy) <= rr:
                set_pixel(xx, yy, color)


def draw_line(x1, y1, x2, y2, width, color):
    x1 *= SCALE
    y1 *= SCALE
    x2 *= SCALE
    y2 *= SCALE
    width *= SCALE
    steps = int(max(abs(x2 - x1), abs(y2 - y1))) + 1
    for i in range(steps):
        t = i / max(1, steps - 1)
        x = x1 + (x2 - x1) * t
        y = y1 + (y2 - y1) * t
        fill_circle(x / SCALE, y / SCALE, width / (2 * SCALE), color)


def draw_text_like_pm():
    # Small block-letter P/M, drawn from rectangles so the generator has no font dependency.
    x = 214
    y = 174
    stroke = 30
    fill_rounded_rect(x, y, stroke, 170, 10, text)
    fill_rounded_rect(x, y, 108, stroke, 10, text)
    fill_rounded_rect(x + 80, y, stroke, 96, 10, text)
    fill_rounded_rect(x, y + 72, 108, stroke, 10, text)
    x = 354
    fill_rounded_rect(x, y, stroke, 170, 10, text)
    draw_line(x + 18, y + 20, x + 78, y + 118, 28, text)
    draw_line(x + 138, y + 20, x + 78, y + 118, 28, text)
    fill_rounded_rect(x + 138, y, stroke, 170, 10, text)


fill_rounded_rect(84, 84, 856, 856, 170, panel)
fill_rounded_rect(132, 132, 760, 760, 120, bg)

for i in range(16):
    x = 174 + i * 43
    shade = panel_light if i % 4 == 0 else panel
    fill_rounded_rect(x, 505, 30, 228, 11, shade)

notes = [614, 590, 625, 568, 543, 644, 603, 523, 651, 580, 561, 629, 548, 618, 590, 674]
for i, y in enumerate(notes):
    x = 178 + i * 43
    fill_rounded_rect(x, y, 22, 54, 10, mint if i % 5 else mint_bright)

for i in range(4):
    y = 398 + i * 42
    draw_line(230, y + math.sin(i) * 18, 806, y + math.cos(i * 1.7) * 18, 12, rgba("#20373e", 210))
    fill_circle(292 + i * 140, y, 18, blue)

fill_circle(744, 262, 74, warm)
fill_circle(744, 262, 38, rgba("#171b1b", 255))
fill_circle(744, 262, 22, warm)

draw_text_like_pm()
fill_rounded_rect(246, 790, 532, 34, 16, mint_bright)
fill_rounded_rect(326, 842, 372, 18, 9, rgba("#a5b2ad", 210))


def downsample():
    out = bytearray()
    area = SCALE * SCALE
    for y in range(SIZE):
        for x in range(SIZE):
            total = [0, 0, 0, 0]
            for yy in range(SCALE):
                for xx in range(SCALE):
                    idx = (((y * SCALE + yy) * W) + (x * SCALE + xx)) * 4
                    for c in range(4):
                        total[c] += pixels[idx + c]
            out.extend(bytes(min(255, total[c] // area) for c in range(4)))
    return out


def write_png(path, width, height, data):
    def chunk(kind, payload):
        return (
            struct.pack(">I", len(payload))
            + kind
            + payload
            + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
        )

    raw = bytearray()
    stride = width * 4
    for y in range(height):
        raw.append(0)
        raw.extend(data[y * stride : (y + 1) * stride])
    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png.extend(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)))
    png.extend(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
    png.extend(chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


out_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "Assets")
os.makedirs(out_dir, exist_ok=True)
write_png(os.path.join(out_dir, "AppIcon.png"), SIZE, SIZE, downsample())
