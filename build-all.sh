#!/usr/bin/env bash
#
# build-all.sh - build every supported camera/firmware and refresh the card
# trees and release packages from the results.
#
# For each entry in the SUPPORTED matrix below:
#   1. build it            (make clean && make fir)
#   2. copy the result to  cameras/<model>/builds/DISKBOOT_<model>_<fw>.BIN
#                          cameras/<model>/builds/PS_<model>_<fw>.FIR
#   3. refresh the card tree at cameras/<model>/card/ - but only from the
#      firmware marked '*', since a card tree holds exactly one DISKBOOT.BIN
#      and it has to be the one the body actually runs.
#   4. rebuild that camera's release package in dist/ from the refreshed card
#      (packaging/make_dist.sh): the card zip, the three card-flashing scripts
#      and the READ_ME_FIRST. dist/ is output only - it is safe to delete the
#      whole thing, this puts it back.
#
# Usage:
#   ./build-all.sh                 build everything
#   ./build-all.sh a480 a470       build only these models
#   ./build-all.sh -n              dry run - print what would happen
#   ./build-all.sh --no-dist       build only, leave dist/ alone
#   ./build-all.sh --with-roms     put Game Boy ROMs on the card zips too
#                                  (left out by default - none are shipped)

set -uo pipefail

ROOT="$(dirname "$(readlink -f "$0")")"
SRC="$ROOT/chdk-src"
BIN="$SRC/bin"

# model  fw    '*' = the firmware that populates cameras/<model>/card/
#               (every supported target refreshes its card - the marker is kept
#                because a card tree holds exactly one DISKBOOT.BIN)
SUPPORTED="
a410 100e *
a430 100b *
a460 100d *
a470 102c *
a480 100b *
a540 100b *
a640 100b *
"

DRYRUN=0
DIST=1
WITH_ROMS=0
MODELS=()
for arg in "$@"; do
    case "$arg" in
        -n|--dry-run) DRYRUN=1 ;;
        --no-dist)    DIST=0 ;;
        --with-roms)  WITH_ROMS=1 ;;
        -h|--help)    sed -n '2,26p' "$0"; exit 0 ;;
        -*)           echo "unknown option: $arg" >&2; exit 1 ;;
        *)            MODELS+=("$arg") ;;
    esac
done

# ---- preflight ------------------------------------------------------------
[[ -d "$SRC" ]]                  || { echo "error: $SRC not found" >&2; exit 1; }
[[ -x "$SRC/update-card.sh" ]]   || { echo "error: $SRC/update-card.sh missing or not executable" >&2; exit 1; }
command -v arm-none-eabi-gcc >/dev/null || { echo "error: arm-none-eabi-gcc not on PATH" >&2; exit 1; }

if [[ ! -f "$SRC/localbuildconf.inc" ]]; then
    echo "==> creating chdk-src/localbuildconf.inc from the example"
    cp "$SRC/localbuildconf.inc.example" "$SRC/localbuildconf.inc"
fi

wanted() {
    [[ ${#MODELS[@]} -eq 0 ]] && return 0
    local m
    for m in "${MODELS[@]}"; do [[ "$m" == "$1" ]] && return 0; done
    return 1
}

# Keep the "<file> md5 <hash>" lines in cameras/<model>/card/<MODEL>.TXT honest.
# Builds are not byte-reproducible - __DATE__/__TIME__ are compiled in - so
# these change on every run even with no source change.
refresh_md5_note() {
    local card=$1 note
    note=$(ls "$card"/*.TXT 2>/dev/null | head -1)
    [[ -n "$note" ]] || return 0
    local f h
    for f in DISKBOOT.BIN PS.FIR; do
        [[ -f "$card/$f" ]] || continue
        grep -qE "^${f}[[:space:]]+md5[[:space:]]" "$note" || continue
        h=$(md5sum "$card/$f" | cut -d' ' -f1)
        sed -i -E "s|^(${f}[[:space:]]+md5[[:space:]]+).*|\1${h}|" "$note"
        echo "    note: $(basename "$note") $f md5 -> $h"
    done
}

PASSED=(); FAILED=(); CARDS=()

# ---- build ----------------------------------------------------------------
while read -r model fw primary; do
    [[ -z "${model:-}" ]] && continue
    wanted "$model" || continue

    builds="$ROOT/cameras/$model/builds"
    card="$ROOT/cameras/$model/card"
    dest_bin="$builds/DISKBOOT_${model}_${fw}.BIN"
    dest_fir="$builds/PS_${model}_${fw}.FIR"

    echo "=============================================================="
    echo "$model-$fw${primary:+  (also refreshes card/)}"

    if [[ ! -d "$SRC/platform/$model/sub/$fw" ]]; then
        echo "  SKIP - platform/$model/sub/$fw does not exist"
        FAILED+=("$model-$fw (no such sub)")
        continue
    fi

    if [[ "$DRYRUN" -eq 1 ]]; then
        echo "  would build, then write:"
        echo "    $dest_bin"
        echo "    $dest_fir  (only if this build emits one)"
        [[ -n "${primary:-}" ]] && echo "    refresh $card"
        continue
    fi

    mkdir -p "$builds"

    # bin/DISKBOOT.BIN and bin/PS.FIR are overwritten by every build regardless
    # of PLATFORM, so each result has to be copied out before the next build.
    ( cd "$SRC" && make PLATFORM="$model" PLATFORMSUB="$fw" clean ) >/dev/null 2>&1

    marker=$(mktemp); sleep 1
    if ! ( cd "$SRC" && make PLATFORM="$model" PLATFORMSUB="$fw" fir ) > "/tmp/build_${model}_${fw}.log" 2>&1; then
        echo "  FAILED - see /tmp/build_${model}_${fw}.log"
        tail -5 "/tmp/build_${model}_${fw}.log" | sed 's/^/    /'
        FAILED+=("$model-$fw")
        rm -f "$marker"
        continue
    fi

    cp "$BIN/DISKBOOT.BIN" "$dest_bin"
    echo "  -> $(basename "$dest_bin")  ($(md5sum "$dest_bin" | cut -d' ' -f1))"

    # Only archive the FIR if THIS build produced it - bin/PS.FIR is shared
    # across every VxWorks target and otherwise still belongs to a prior camera.
    fir_fresh=0
    if [[ -f "$BIN/PS.FIR" && "$BIN/PS.FIR" -nt "$marker" ]]; then
        cp "$BIN/PS.FIR" "$dest_fir"
        fir_fresh=1
        echo "  -> $(basename "$dest_fir")  ($(md5sum "$dest_fir" | cut -d' ' -f1))"
    fi
    rm -f "$marker"

    if [[ -n "${primary:-}" ]]; then
        if ( cd "$SRC" && ./update-card.sh "$model" ) >/dev/null 2>&1; then
            # update-card.sh deliberately refuses to copy PS.FIR because it
            # cannot know which camera built it. Here we can: this build did.
            [[ "$fir_fresh" -eq 1 ]] && cp "$BIN/PS.FIR" "$card/PS.FIR"
            refresh_md5_note "$card"
            echo "  -> refreshed cameras/$model/card/"
            CARDS+=("$model")
        else
            echo "  WARNING - update-card.sh failed for $model; card/ left as-is"
            FAILED+=("$model-$fw (card refresh)")
        fi
    fi

    PASSED+=("$model-$fw")
done <<< "$SUPPORTED"

# ---- package --------------------------------------------------------------
# Only cameras whose card tree this run actually refreshed: a package is a
# snapshot of cameras/<model>/card/, so repackaging one we did not touch would
# only churn its checksums.
if [[ "$DIST" -eq 1 && "$DRYRUN" -eq 0 && ${#CARDS[@]} -gt 0 ]]; then
    echo "=============================================================="
    echo "packaging into dist/"
    args=()
    [[ "$WITH_ROMS" -eq 1 ]] && args+=(--with-roms)
    "$ROOT/packaging/make_dist.sh" "${args[@]}" "${CARDS[@]}"
fi

# ---- summary ---------------------------------------------------------------
echo "=============================================================="
echo "built: ${#PASSED[@]}"
for p in ${PASSED+"${PASSED[@]}"}; do echo "  $p"; done
if [[ ${#FAILED[@]} -gt 0 ]]; then
    echo "failed: ${#FAILED[@]}"
    for f in "${FAILED[@]}"; do echo "  $f"; done
    exit 1
fi
