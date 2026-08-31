#!/usr/bin/env bash
# Refresh an existing cameras/<model>/card/ tree with the latest local build,
# plus this fork's custom reticles and scripts.
#
# `make ... fir` only writes bin/DISKBOOT.BIN and CHDK/MODULES/*.flt inside
# this tree - it never touches cameras/<model>/card/, which is what
# setup-card.sh actually copies onto an SD card. Separately, CHDK/GRIDS/ and
# CHDK/SCRIPTS/ carry this fork's own additions (see docs/CUSTOMIZATION.md)
# that no build step ever generates or copies anywhere - they just sit in the
# source tree unless something moves them. This closes both gaps by copying
# bin/DISKBOOT.BIN, CHDK/MODULES/*.flt, CHDK/GRIDS/, and CHDK/SCRIPTS/ over an
# already-existing card tree, adding/overwriting by name only. It never
# deletes anything - a file already on the card tree that isn't in the source
# (a hand-added preset, a per-camera script) is left alone.
#
# Usage: ./update-card.sh <camdir>   e.g. ./update-card.sh a470

set -euo pipefail

CAMDIR="${1:-}"
if [[ -z "$CAMDIR" ]]; then
    echo "usage: $0 <camdir>   (e.g. $0 a470, matching cameras/<camdir>/)" >&2
    exit 1
fi

HERE="$(dirname "$(readlink -f "$0")")"
BIN="$HERE/bin"
SRC_MODULES="$HERE/CHDK/MODULES"
SRC_GRIDS="$HERE/CHDK/GRIDS"
SRC_SCRIPTS="$HERE/CHDK/SCRIPTS"
SRC_SOUNDS="$HERE/CHDK/SOUNDS"
CARD="$HERE/../cameras/$CAMDIR/card"

[[ -f "$BIN/DISKBOOT.BIN" ]]  || { echo "error: $BIN/DISKBOOT.BIN missing - run make fir first" >&2; exit 1; }
[[ -d "$SRC_MODULES" ]]       || { echo "error: $SRC_MODULES missing" >&2; exit 1; }
[[ -d "$CARD" ]]              || { echo "error: $CARD does not exist - this refreshes an existing card tree, it does not create one" >&2; exit 1; }
[[ -f "$CARD/DISKBOOT.BIN" ]] || { echo "error: $CARD/DISKBOOT.BIN missing - this doesn't look like a card tree" >&2; exit 1; }
[[ -d "$CARD/CHDK/MODULES" ]] || { echo "error: $CARD/CHDK/MODULES missing - this doesn't look like a card tree" >&2; exit 1; }

echo "==> $BIN/DISKBOOT.BIN -> $CARD/DISKBOOT.BIN"
cp "$BIN/DISKBOOT.BIN" "$CARD/DISKBOOT.BIN"

echo "==> $SRC_MODULES/*.flt -> $CARD/CHDK/MODULES/"
rm -f "$CARD/CHDK/MODULES/"*.flt
cp "$SRC_MODULES/"*.flt "$CARD/CHDK/MODULES/"

echo "==> $SRC_GRIDS/ -> $CARD/CHDK/GRIDS/"
mkdir -p "$CARD/CHDK/GRIDS"
cp -r "$SRC_GRIDS/." "$CARD/CHDK/GRIDS/"

echo "==> $SRC_SCRIPTS/ -> $CARD/CHDK/SCRIPTS/"
mkdir -p "$CARD/CHDK/SCRIPTS"
cp -r "$SRC_SCRIPTS/." "$CARD/CHDK/SCRIPTS/"

echo "==> $SRC_SOUNDS/ -> $CARD/CHDK/SOUNDS/"
mkdir -p "$CARD/CHDK/SOUNDS"
cp -r "$SRC_SOUNDS/." "$CARD/CHDK/SOUNDS/"

# bin/PS.FIR is shared across every VxWorks camera's build (the Makefile names
# it the same regardless of PLATFORM) and gets clobbered on each build, so it
# is not safe to copy automatically - it may belong to a different camera than
# the one just built.
if [[ -f "$BIN/PS.FIR" && -f "$CARD/PS.FIR" && "$BIN/PS.FIR" -nt "$CARD/PS.FIR" ]]; then
    echo "note: $BIN/PS.FIR is newer than $CARD/PS.FIR but was NOT copied -" >&2
    echo "      bin/PS.FIR is shared across every VxWorks camera build. If" >&2
    echo "      $CAMDIR needs it, verify it came from this build before copying by hand." >&2
fi

echo
echo "Card tree updated: $CARD"
echo "Run setup-card.sh against it to write a fresh physical card, or"
echo "'setup-card.sh --update' to refresh an already-formatted one in place."
