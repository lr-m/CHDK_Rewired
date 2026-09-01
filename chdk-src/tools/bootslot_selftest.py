#!/usr/bin/env python3
"""Off-camera proof that the boot screen slot and its importer agree.

Reimplements modules/boot_image.c's install_early_jpeg() byte for byte - the
same marker scan, the same dancing-bits transform, the same capacity/size
layout - and runs it against the DISKBOOT.BIN each camera actually builds.

What this catches, none of which needs a body:
  * a slot that is missing from the built DISKBOOT.BIN, or unfindable because
    the encoding version in the importer's table is wrong for that camera
  * a capacity in the importer's table that disagrees with the one the
    generated header reserved (which would let the importer write past the
    slot)
  * a compiled-in default image that is not a valid JPEG, or whose declared
    size does not match its content
  * an import that does not read back as the image that went in

Usage: bootslot_selftest.py [cameras_dir]
"""
import re
import struct
import sys
import os

PERMS = {1: [4, 6, 1, 0, 7, 2, 5, 3], 2: [5, 3, 6, 1, 2, 7, 0, 4]}

# Must mirror boot_slot_cfgs[] in modules/boot_image.c.
CFGS = [
    ("a430", "A430", 0, 18426),
    ("a460", "A460", 0, 18426),
    ("a470", "A470", 1, 17500),
    ("a480", "A480", 2, 7168),
]


def dance(v, pos):
    pos &= 0x3ff
    if pos % 3 != 0:
        return v ^ 0xff
    if (pos & 1) == 0:
        return v ^ 0xa0
    return ((v >> 4) | (v << 4)) & 0xff


def transcode(data, version, patch=None):
    """Decode DISKBOOT to raw bytes, optionally applying a patch callback.

    Returns (raw_bytes, reencoded_file). With version 0 the transform is the
    identity and the file is the raw bytes, which is exactly how the importer
    treats the VxWorks bodies.
    """
    if version:
        prefix, body = data[:1], data[1:]
        perm = PERMS[version]
    else:
        prefix, body = b"", data
        perm = None

    raw = bytearray()
    groups = []
    pos = 0
    while pos + 8 <= len(body):
        g = body[pos:pos + 8]
        if perm:
            r = bytes(dance(g[perm[i]], pos + i) for i in range(8))
        else:
            r = bytes(g)
        raw += r
        groups.append(r)
        pos += 8
    tail = body[pos:]

    if patch is not None:
        raw2 = bytearray(patch(bytes(raw)))
        assert len(raw2) == len(raw)
        groups = [bytes(raw2[i:i + 8]) for i in range(0, len(raw2), 8)]
        raw = raw2

    out = bytearray(prefix)
    for gi, r in enumerate(groups):
        base = gi * 8
        if perm:
            e = bytearray(8)
            for i in range(8):
                e[perm[i]] = dance(r[i], base + i)
            out += e
        else:
            out += r
    out += tail
    return bytes(raw), bytes(out)


def check(cam, tag, version, cap, path):
    data = open(path, "rb").read()
    raw, roundtrip = transcode(data, version)
    if roundtrip != data:
        return "transcode is not lossless (encoding version %d wrong?)" % version

    marker = (b"BOOTSLOT" + tag.encode() + b"IMG1")
    i = raw.find(marker)
    if i < 0:
        return "marker %s not found in %s" % (tag, os.path.basename(path))

    slot_cap, size = struct.unpack("<II", raw[i + 16:i + 24])
    if slot_cap != cap:
        return "capacity mismatch: slot says %d, importer table says %d" % (slot_cap, cap)
    jpeg = raw[i + 24:i + 24 + slot_cap]
    if not 4 <= size <= slot_cap:
        return "declared size %d outside slot capacity %d" % (size, slot_cap)
    if jpeg[:2] != b"\xff\xd8":
        return "compiled-in default is not a JPEG (no SOI)"
    if jpeg[size - 2:size] != b"\xff\xd9":
        return "declared size %d does not end on EOI" % size
    if set(jpeg[size:]) - {0xff}:
        return "padding after the image is not 0xff"

    # Now the round trip: patch a different image in and read it back, exactly
    # as install_early_jpeg() would.
    new = bytes([0xff, 0xd8]) + bytes(range(256)) * 3 + bytes([0xff, 0xd9])

    def do_patch(r):
        b = bytearray(r)
        j = b.find(marker)
        # capacity is read, never rewritten; size then image follow it
        b[j + 20:j + 24] = struct.pack("<I", len(new))
        b[j + 24:j + 24 + slot_cap] = new + b"\xff" * (slot_cap - len(new))
        return bytes(b)

    _, patched_file = transcode(data, version, patch=do_patch)
    raw2, _ = transcode(patched_file, version)
    j = raw2.find(marker)
    cap2, size2 = struct.unpack("<II", raw2[j + 16:j + 24])
    got = raw2[j + 24:j + 24 + size2]
    if cap2 != slot_cap:
        return "import corrupted the capacity field"
    if size2 != len(new) or got != new:
        return "import did not read back the image that went in"
    if len(patched_file) != len(data):
        return "import changed the file length"
    return None


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "../cameras"
    fails = 0
    checked = 0
    for cam, tag, version, cap in CFGS:
        path = os.path.join(root, cam, "card", "DISKBOOT.BIN")
        if not os.path.exists(path):
            print("  %-5s SKIP  no %s" % (cam, path))
            continue
        checked += 1
        err = check(cam, tag, version, cap, path)
        if err:
            print("  %-5s FAIL  %s" % (cam, err))
            fails += 1
        else:
            print("  %-5s ok    slot %s, enc v%d, capacity %d" % (cam, tag, version, cap))
    if not checked:
        print("nothing checked")
        return 1
    print("%d/%d boot slots verified" % (checked - fails, checked))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
