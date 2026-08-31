Custom sounds (A480/100b)
=========================

Place any of these files in this directory:

  startup.wav    replaces the startup sound on supported cameras
  shutter.wav    replaces Canon's My Camera shutter sound
  button.wav     replaces Canon's My Camera operation sound
  selftimer.wav  replaces Canon's My Camera self-timer sound

Files must be standard PCM WAV: 11025 Hz, unsigned 8-bit, mono. Convert an
ordinary PCM WAV on your computer with:

  python3 tools/mksound.py input.wav button.wav

The actual startup sound happens before the card filesystem is mounted and is
therefore compiled into the A480 build with tools/mksound.py --header; see
docs/CUSTOMIZATION.md. Missing or invalid card files leave the corresponding
Canon sound unchanged.
Keep clips short; the loader rejects files larger than 256 KiB.
