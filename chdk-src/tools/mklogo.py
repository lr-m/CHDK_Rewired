#!/usr/bin/env python3
"""
Build a CHDK boot splash (CHDK/DATA/logo.dat) for the A470 / A480.

Format, from gui_draw_splash() in core/gui.c:
  one byte per run -- bits 7..5 = palette index, bits 4..0 = (run length - 1)
  pixels stream left-to-right, wrapping every LOGO_WIDTH+1 = 150 px, 84 rows.
  Palette index 6 maps to COLOR_TRANSPARENT (0x00) and is skipped when drawing.

Usage:
  mklogo.py --text "A480" [--sub "CHDK"] -o logo.dat
  mklogo.py --image pic.png -o logo.dat
  mklogo.py --decode logo.dat        # dump an existing logo as ASCII
"""

import argparse
import sys

WIDTH = 150       # LOGO_WIDTH + 1, the effective row stride
HEIGHT = 84
MAX_RUN = 32      # (data & 0x1F) + 1

TRANSPARENT = 6

# logo_colors[] in core/gui.c, approximated in RGB for quantisation
PALETTE = {
    0: (0, 0, 0),          # COLOR_BLACK
    1: (128, 0, 0),        # COLOR_RED_DK
    2: (255, 0, 0),        # COLOR_RED
    3: (128, 128, 128),    # COLOR_GREY
    4: (192, 192, 192),    # COLOR_GREY_LT
    5: (255, 128, 128),    # COLOR_RED_LT
    7: (255, 255, 255),    # COLOR_WHITE
}


def nearest(rgb):
    """Nearest opaque palette index for an RGB triple."""
    best, best_d = 7, None
    for idx, ref in PALETTE.items():
        d = sum((a - b) ** 2 for a, b in zip(rgb, ref))
        if best_d is None or d < best_d:
            best, best_d = idx, d
    return best


def encode(pixels):
    """RLE-encode a flat list of palette indices, trimming trailing transparency."""
    while pixels and pixels[-1] == TRANSPARENT:
        pixels.pop()

    out = bytearray()
    i = 0
    while i < len(pixels):
        c = pixels[i]
        n = 1
        while i + n < len(pixels) and pixels[i + n] == c and n < MAX_RUN:
            n += 1
        out.append(((c & 0x07) << 5) | ((n - 1) & 0x1F))
        i += n
    return bytes(out)


def decode(data):
    """Inverse of encode(), for inspecting an existing logo.dat."""
    px = []
    for b in data:
        px.extend([(b >> 5) & 0x07] * ((b & 0x1F) + 1))
    return px


def from_pil_image(source):
    from PIL import Image
    img = source.convert("RGBA")
    img.thumbnail((WIDTH, HEIGHT), Image.LANCZOS)

    canvas = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    canvas.paste(img, ((WIDTH - img.width) // 2, (HEIGHT - img.height) // 2))

    px = []
    for y in range(HEIGHT):
        for x in range(WIDTH):
            r, g, b, a = canvas.getpixel((x, y))
            px.append(TRANSPARENT if a < 128 else nearest((r, g, b)))
    return px


def from_image(path):
    from PIL import Image
    return from_pil_image(Image.open(path))


def from_text(text, sub=None):
    from PIL import Image, ImageDraw, ImageFont

    def load(size):
        for p in ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
                  "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
            try:
                return ImageFont.truetype(p, size)
            except OSError:
                continue
        return ImageFont.load_default()

    img = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # main word: grow until it fills the width
    size = 8
    font = load(size)
    while size < 80:
        nxt = load(size + 2)
        w = d.textbbox((0, 0), text, font=nxt)[2]
        if w > WIDTH - 8:
            break
        size += 2
        font = nxt

    bb = d.textbbox((0, 0), text, font=font)
    tw, th = bb[2] - bb[0], bb[3] - bb[1]
    ty = 6
    d.text(((WIDTH - tw) // 2 - bb[0], ty - bb[1]), text, font=font,
           fill=(255, 255, 255, 255))

    # accent bar under the word
    bar_y = ty + th + 6
    d.rectangle([14, bar_y, WIDTH - 15, bar_y + 4], fill=(255, 0, 0, 255))

    if sub:
        sfont = load(16)
        sb = d.textbbox((0, 0), sub, font=sfont)
        d.text(((WIDTH - (sb[2] - sb[0])) // 2 - sb[0], bar_y + 10 - sb[1]),
               sub, font=sfont, fill=(192, 192, 192, 255))

    px = []
    for y in range(HEIGHT):
        for x in range(WIDTH):
            r, g, b, a = img.getpixel((x, y))
            px.append(TRANSPARENT if a < 128 else nearest((r, g, b)))
    return px


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--text")
    ap.add_argument("--sub")
    ap.add_argument("--image")
    ap.add_argument("--decode")
    ap.add_argument("-o", "--output", default="logo.dat")
    a = ap.parse_args()

    if a.decode:
        px = decode(open(a.decode, "rb").read())
        chars = {TRANSPARENT: " ", 0: "#", 7: "@"}
        for y in range(HEIGHT):
            row = px[y * WIDTH:(y + 1) * WIDTH]
            if not row:
                break
            print("".join(chars.get(p, "+") for p in row))
        return

    if a.image:
        px = from_image(a.image)
    elif a.text:
        px = from_text(a.text, a.sub)
    else:
        ap.error("need --text, --image or --decode")

    data = encode(px)
    if len(data) > 8192:
        print("warning: %d bytes is large for a splash" % len(data), file=sys.stderr)
    open(a.output, "wb").write(data)
    print("wrote %s: %d bytes, %d runs" % (a.output, len(data), len(data)))


if __name__ == "__main__":
    main()
