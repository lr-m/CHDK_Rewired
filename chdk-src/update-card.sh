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

# bin/DISKBOOT.BIN is overwritten by *every* build regardless of target, exactly
# as bin/PS.FIR is (see the note at the foot of this script). Called straight
# after building this model - which is how full_rebuild.sh calls it - that is
# fine. Called by hand after building something else, it silently installs
# another camera's boot file, and a card carrying the wrong DISKBOOT.BIN does
# not boot CHDK at all.
#
# So confirm provenance rather than assume it: if this model has archived builds,
# bin/DISKBOOT.BIN must match one of them. If it matches none, refuse rather than
# install a boot file that demonstrably belongs to a different camera.
shopt -s nullglob
ARCHIVED=( "$HERE/../cameras/$CAMDIR/builds/DISKBOOT_${CAMDIR}_"*.BIN )
shopt -u nullglob
if (( ${#ARCHIVED[@]} )); then
    BINSUM="$(md5sum < "$BIN/DISKBOOT.BIN" | cut -d' ' -f1)"
    MATCHED=""
    for a in "${ARCHIVED[@]}"; do
        if [[ "$(md5sum < "$a" | cut -d' ' -f1)" == "$BINSUM" ]]; then
            MATCHED="$a"; break
        fi
    done
    if [[ -z "$MATCHED" ]]; then
        echo "error: $BIN/DISKBOOT.BIN does not match any archived $CAMDIR build." >&2
        echo "       bin/DISKBOOT.BIN is overwritten by every build, so this is" >&2
        echo "       almost certainly another camera's boot file. Rebuild $CAMDIR" >&2
        echo "       (make PLATFORM=$CAMDIR PLATFORMSUB=<fw> fir) and re-run." >&2
        exit 1
    fi
fi

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

# Sounds, with a per-camera size ceiling.
#
# Most bodies here *repoint* Canon's My Camera slot at our file, so the file can
# be any length. Two do not: the A430 and A540 cache entries carry a
# cached-theme field and their getter rewrites the entry length from flash on
# every call, so a repointed buffer is both useless (the player is told the
# stock length) and dangerous (a theme change memcpy's a flash-sized asset into
# our smaller heap block). Those two overwrite Canon's buffer *in place*
# instead, which makes the stock asset length a hard ceiling - see
# platform/a430/wrappers.c and platform/a540/wrappers.c.
#
# Both bodies carry the same Canon theme asset set, confirmed by name out of the
# flash container at 0xfff70000: 121_RL01 (shutter) is 3244 bytes and 121_OP01
# (button) is 3300. A longer file is refused by the port and the custom sound
# silently does not play.
#
# So a plain copy of one shared set to every card is wrong, and it is wrong
# *quietly*. It is also self-healing in the worst way: hand-trimming the files
# on the card works until the next rebuild copies the full-size ones back over
# them. tools/mksound.py --max-bytes cuts to fit with a fade-out so the truncation
# is not itself a click.
echo "==> $SRC_SOUNDS/ -> $CARD/CHDK/SOUNDS/"
mkdir -p "$CARD/CHDK/SOUNDS"
cp -r "$SRC_SOUNDS/." "$CARD/CHDK/SOUNDS/"

case "$CAMDIR" in
    a430|a540)
        MKSOUND="$HERE/tools/mksound.py"
        if [[ -f "$MKSOUND" ]]; then
            for spec in shutter:3244 button:3300; do
                name="${spec%%:*}"; cap="${spec##*:}"
                src="$SRC_SOUNDS/$name.wav"
                [[ -f "$src" ]] || continue
                if [[ $(stat -c%s "$src") -gt $cap ]]; then
                    echo "    $name.wav -> $cap bytes (in-place slot ceiling on $CAMDIR)"
                    python3 "$MKSOUND" "$src" "$CARD/CHDK/SOUNDS/$name.wav" \
                        --max-bytes "$cap" >/dev/null
                fi
            done
        else
            echo "warning: $MKSOUND missing - $CAMDIR sounds may exceed its slot ceiling" >&2
        fi
        ;;
esac

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
