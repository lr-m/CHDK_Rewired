#!/usr/bin/env python3
"""Convert a PCM WAV into the Canon A480 My Camera sound format.

The camera accepts standard RIFF/WAVE containing unsigned 8-bit mono PCM at
11025 Hz. This tool converts 8- or 16-bit PCM, mono or stereo, without external
dependencies. Runtime output names understood by the port are shutter.wav,
button.wav and selftimer.wav in A/CHDK/SOUNDS. The startup sound is generated
as an embedded C header because Canon plays it before the card is mounted.
"""

import argparse
import struct
import wave

RATE = 11025
MAX_BYTES = 256 * 1024


def read_pcm(path):
    with wave.open(path, "rb") as src:
        channels = src.getnchannels()
        width = src.getsampwidth()
        rate = src.getframerate()
        frames = src.getnframes()
        if src.getcomptype() != "NONE" or channels not in (1, 2) or width not in (1, 2):
            raise ValueError("input must be uncompressed 8/16-bit mono or stereo PCM")
        raw = src.readframes(frames)

    samples = []
    if width == 1:
        values = [((v - 128) << 8) for v in raw]
    else:
        count = len(raw) // 2
        values = struct.unpack("<%dh" % count, raw)
    for i in range(0, len(values), channels):
        if channels == 1:
            samples.append(values[i])
        else:
            samples.append((values[i] + values[i + 1]) // 2)
    return samples, rate


def resample(samples, source_rate):
    if not samples:
        return b""
    out_count = max(1, (len(samples) * RATE) // source_rate)
    out = bytearray(out_count)
    for i in range(out_count):
        pos_num = i * source_rate
        left = pos_num // RATE
        frac = pos_num % RATE
        if left >= len(samples) - 1:
            value = samples[-1]
        else:
            value = (samples[left] * (RATE - frac) + samples[left + 1] * frac) // RATE
        out[i] = max(0, min(255, (value >> 8) + 128))
    return bytes(out)


def fit(pcm, max_bytes, silence_threshold=4, preroll=64):
    """Cut to fit max_bytes of WAV file, keeping the part that makes a sound.

    Needed by the A430 and A540, whose slots are not repointed at our file but
    overwritten in place inside Canon's own buffer - so the ceiling is that
    body's stock asset length, which for the shutter is 3244 bytes (0.29s).
    See platform/a430/wrappers.c and platform/a540/wrappers.c.

    This does NOT simply truncate. The card's button.wav carries 0.53s of
    digital silence before the click actually starts, so cutting the first
    0.30s of it produced a file that was 3300 bytes of pure 0x80 - a sound
    slot that "worked" and played nothing. Head truncation is almost never
    what you want for a sound with a leading gap, and there is no way to tell
    from the file size that it went wrong.

    So: find the audible span, drop the silence in front of it, and keep as
    much from there as fits. When the audible part is itself longer than the
    ceiling the attack is what survives, which is the right half of a shutter
    or button click to keep.

    A hard cut leaves the waveform at whatever level it happened to reach, and
    against the 0x80 silence that follows it that step is a click - which on a
    shutter sound is indistinguishable from the sound itself. So the last 10%
    is ramped to 128 instead.
    """
    limit = max_bytes - 44
    if limit < 1:
        return pcm

    # The audible span, as offsets into pcm. 8-bit unsigned PCM silence is 128.
    loud = [i for i, v in enumerate(pcm) if abs(v - 128) > silence_threshold]
    if not loud:
        return pcm[:limit] if len(pcm) > limit else pcm

    start = max(0, loud[0] - preroll)
    end = min(len(pcm), loud[-1] + 1)
    if end - start <= limit:
        # The whole sound fits once the leading silence is gone. Keep any
        # trailing silence that still fits - it costs nothing and preserves
        # the original spacing if the slot is longer than the sound.
        end = min(len(pcm), start + limit)
        return pcm[start:end]

    out = bytearray(pcm[start:start + limit])
    fade = max(1, limit // 10)
    for i in range(fade):
        pos = limit - fade + i
        gain = (fade - i) / float(fade)
        out[pos] = max(0, min(255, int(round(128 + (out[pos] - 128) * gain))))
    return bytes(out)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("output")
    parser.add_argument("--header", action="store_true",
                        help="write a C header instead of a WAV file")
    parser.add_argument("--var", default="boot_sound_wav")
    parser.add_argument("--max-bytes", type=int, default=MAX_BYTES,
                        help="ceiling for the finished WAV file, in bytes. "
                             "Truncates with a fade-out rather than failing. "
                             "The A540's in-place slots need this; the A470 "
                             "and A480 repoint and do not.")
    args = parser.parse_args()

    try:
        samples, source_rate = read_pcm(args.input)
    except (OSError, EOFError, wave.Error, ValueError) as exc:
        parser.error(str(exc))
    pcm = resample(samples, source_rate)
    if args.max_bytes < MAX_BYTES:
        pcm = fit(pcm, args.max_bytes)
    if len(pcm) + 44 > MAX_BYTES:
        parser.error("converted sound exceeds the camera limit of %d bytes" % MAX_BYTES)

    if args.header:
        # Construct the canonical 44-byte PCM WAV header explicitly so the
        # embedded bytes are identical to the card-loaded format.
        wav = (b"RIFF" + struct.pack("<I", len(pcm) + 36) + b"WAVEfmt " +
               struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE, 1, 8) +
               b"data" + struct.pack("<I", len(pcm)) + pcm)
        lines = [
            "// Generated by tools/mksound.py - DO NOT EDIT BY HAND.",
            "static const unsigned char %s[] __attribute__((aligned(4))) = {" % args.var,
        ]
        for i in range(0, len(wav), 16):
            lines.append("    " + "".join("0x%02x," % b for b in wav[i:i+16]))
        lines.append("};")
        with open(args.output, "w") as dst:
            dst.write("\n".join(lines) + "\n")
    else:
        with wave.open(args.output, "wb") as dst:
            dst.setnchannels(1)
            dst.setsampwidth(1)
            dst.setframerate(RATE)
            dst.writeframes(pcm)
    print("wrote %s: %.2fs, 11025 Hz, 8-bit mono" %
          (args.output, len(pcm) / RATE))


if __name__ == "__main__":
    main()
