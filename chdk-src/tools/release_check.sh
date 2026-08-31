#!/usr/bin/env bash
# Host-side release checks for the custom camera features. Firmware builds are
# deliberately separate: run ../full_rebuild.sh after this passes.

set -euo pipefail

TOOLS=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$TOOLS/.." && pwd)
OUT=$(mktemp -d /tmp/chdk-release-check.XXXXXX)
trap 'rm -rf "$OUT"' EXIT

build_run() {
    local name=$1
    shift
    echo "== $name"
    gcc -O2 -Wall -Wextra "$@" -o "$OUT/$name"
    "$OUT/$name"
}

cd "$TOOLS"

build_run bend_selftest \
    -I../include bend_selftest.c ../core/bend.c
build_run bendx_selftest \
    -I../include -I../platform/a460 bendx_selftest.c ../core/bendx.c
build_run mexp_selftest \
    -I../include mexp_selftest.c ../core/mexp.c
build_run bend_shot_selftest \
    -Iteststub -I../include -I../platform/a460 bend_shot_selftest.c \
    ../core/bend_shot.c ../core/bend.c ../core/bendx.c
build_run bend_tag_selftest \
    -Iteststub_tag -Iteststub -I../include -I../platform/a460 \
    bend_tag_selftest.c ../core/bend_tag.c ../core/bend_shot.c \
    ../core/bend.c ../core/bendx.c
build_run gbcam_selftest \
    -I../modules/gbc -DGBC_HOST_TEST \
    gbcam_selftest.c ../modules/gbc/gbc_cam.c
build_run gbc_save_selftest \
    -I../modules/gbc -I../modules/gbc/core -DGBC_HOST_TEST -DGBC_CHDK_PORT \
    -include ../modules/gbc/gbc_port.h \
    gbc_save_selftest.c ../modules/gbc/gbc_save.c
build_run gbc_mmu_selftest \
    -I../modules/gbc -I../modules/gbc/core -DGBC_HOST_TEST -DGBC_CHDK_PORT \
    -include ../modules/gbc/gbc_port.h \
    gbc_mmu_selftest.c ../modules/gbc/gbc_rom.c

if command -v lizard >/dev/null 2>&1; then
    echo "== complexity"
    # The emulator core is imported switch-machine code and is intentionally
    # excluded. This gate covers the code maintained as part of this fork.
    lizard -l c -w -C 45 -L 280 \
        "$ROOT/core/bend.c" "$ROOT/core/bend_seg.c" \
        "$ROOT/core/bend_shot.c" "$ROOT/core/bend_tag.c" \
        "$ROOT/core/bendx.c" "$ROOT/core/gui_bend.c" \
        "$ROOT/core/gui_recui.c" "$ROOT/core/mexp.c" \
        "$ROOT/core/mexp_ghost.c" "$ROOT/core/raw.c" \
        "$ROOT/modules/gbc/gbc_cam.c" "$ROOT/modules/gbc/gbc_emu.c" \
        "$ROOT/modules/gbc/gbc_feed.c" "$ROOT/modules/gbc/gbc_rom.c" \
        "$ROOT/modules/gbc/gbc_save.c"
else
    echo "== complexity (skipped: install lizard to enable)"
fi

echo "all release checks passed"
