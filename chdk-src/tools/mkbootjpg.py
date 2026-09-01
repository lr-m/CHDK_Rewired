#!/usr/bin/env python3
"""Convert any image into a boot screen the camera can install without work.

The on-camera importer used to do all of this itself: decode an arbitrary
JPEG/PNG, resize it, and re-encode it until it fit the slot. That is megabytes
of buffers and tens of thousands of file operations on a body with a few hundred
KB free, and it is what made "Import JPG/PNG" unreliable.

Doing it here instead leaves the camera with nothing to do but copy bytes: a
file produced by this tool is already the exact size, colour space, marker
layout and byte budget the firmware decoder wants.

What "boot ready" means, and why each part matters:

  320x240        the LCD, and what Canon's own startup asset is. The firmware
                 draws the decoded image at its own size; anything else is not
                 what the slot was measured for.
  baseline       SOF0, not progressive. The ROM decoder handles baseline only.
  4:2:0, 3 comp  the stock asset's sampling.
  standard
  Huffman        optimize=off. Optimised tables render as a BLACK SCREEN - the
                 firmware only carries the standard ones. This is the single
                 easiest way to produce a boot screen that bricks the splash,
                 and it is why this tool exists rather than "just use any JPEG".
  <= capacity    each camera reserves a different number of bytes. See below.

Usage:
  mkbootjpg.py --cam a480 photo.png -o WP.JPG
  mkbootjpg.py --cam a480 photo.png -o WP.JPG --fit contain   # letterbox
"""

import argparse
import io
import sys

try:
    from PIL import Image, ImageFilter
except ImportError:
    sys.exit("needs Pillow:  pip install Pillow")

WIDTH, HEIGHT = 320, 240

# Must match boot_slot_cfgs[] in modules/boot_image.c and the capacity each
# camera's sub/<fw>/boot_image.h (or loader) actually reserved.
CAPACITY = {
    "a430": 18426,
    "a460": 18426,
    "a470": 17500,
    "a480": 7168,
}


def fit_image(im, mode):
    w, h = im.size
    if mode == "contain":
        # whole image, letterboxed on black - matches the camera's old behaviour
        if w * HEIGHT > h * WIDTH:
            nw, nh = WIDTH, max(1, h * WIDTH // w)
        else:
            nh, nw = HEIGHT, max(1, w * HEIGHT // h)
        out = Image.new("RGB", (WIDTH, HEIGHT), (0, 0, 0))
        out.paste(im.resize((nw, nh), Image.LANCZOS),
                  ((WIDTH - nw) // 2, (HEIGHT - nh) // 2))
        return out
    # cover: fill the screen, trimming the long axis
    if w * HEIGHT > h * WIDTH:
        nw = h * WIDTH // HEIGHT
        im = im.crop(((w - nw) // 2, 0, (w - nw) // 2 + nw, h))
    elif w * HEIGHT < h * WIDTH:
        nh = w * HEIGHT // WIDTH
        im = im.crop((0, (h - nh) // 2, w, (h - nh) // 2 + nh))
    return im.resize((WIDTH, HEIGHT), Image.LANCZOS)


def encode(im, limit, denoise):
    """Highest quality that fits `limit`, denoising only if it has to."""
    for blur in ((0, 0), (3, 0.0), (5, 0.6), (7, 1.2)) if denoise else ((0, 0),):
        src = im
        if blur[0]:
            src = src.filter(ImageFilter.MedianFilter(blur[0]))
            if blur[1]:
                src = src.filter(ImageFilter.GaussianBlur(blur[1]))
        for q in range(92, 19, -2):
            b = io.BytesIO()
            src.save(b, "JPEG", quality=q, subsampling=2,
                     progressive=False, optimize=False)   # optimize=off is required
            if b.tell() <= limit:
                return q, blur[0], b.getvalue()
    return None, None, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--cam", required=True, choices=sorted(CAPACITY),
                    help="which camera's slot to fit")
    ap.add_argument("--fit", default="cover", choices=("cover", "contain"),
                    help="cover fills the screen (default); contain letterboxes")
    ap.add_argument("--no-denoise", action="store_true",
                    help="never smooth to make it fit; fail instead")
    args = ap.parse_args()

    limit = CAPACITY[args.cam]
    try:
        im = Image.open(args.input).convert("RGB")
    except Exception as exc:
        sys.exit("could not read %s: %s" % (args.input, exc))

    q, blur, data = encode(fit_image(im, args.fit), limit, not args.no_denoise)
    if data is None:
        sys.exit("could not fit %s into the %s's %d byte slot. Try a simpler "
                 "image, or drop --no-denoise." % (args.input, args.cam, limit))

    open(args.out, "wb").write(data)
    note = ", denoise %d" % blur if blur else ""
    print("wrote %s: %dx%d baseline, quality %d%s, %d/%d bytes (%s)"
          % (args.out, WIDTH, HEIGHT, q, note, len(data), limit, args.cam))


if __name__ == "__main__":
    main()
