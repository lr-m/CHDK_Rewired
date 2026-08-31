#!/bin/bash
# Build CHDK from source on macOS.
#
#   ./build-macos.sh                 # defaults to a530 / 100a
#   ./build-macos.sh a640 100b
#   ./build-macos.sh a530 100a /path/to/chdk-src
#
# Produces, in <source>/bin/ :
#   <cam>-<fw>-1.7.0-0.zip        DISKBOOT.BIN, PS.FIR, modules, docs
#   <cam>-<fw>-1.7.0-0-full.zip   the above plus the complete CHDK card tree
#
# CHDK's build system already knows about macOS (it picks `sed -E` over
# `sed -r`), so this script is mostly a prerequisite check plus the two
# macOS-specific overrides.

set -euo pipefail

CAM="${1:-a530}"
FW="${2:-100a}"
SRC="${3:-}"

# ---- locate the source tree ----------------------------------------------
if [[ -z "$SRC" ]]; then
    d="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    while [[ "$d" != "/" ]]; do
        if [[ -f "$d/makefile_env.inc" ]]; then SRC="$d"; break; fi
        if [[ -f "$d/chdk-src/makefile_env.inc" ]]; then SRC="$d/chdk-src"; break; fi
        d="$(dirname "$d")"
    done
fi
[[ -n "$SRC" && -f "$SRC/makefile_env.inc" ]] || {
    echo "error: could not find the CHDK source tree." >&2
    echo "       Pass it: $0 $CAM $FW /path/to/chdk-src" >&2
    exit 1
}
SRC="$(cd "$SRC" && pwd)"

[[ -d "$SRC/platform/$CAM/sub/$FW" ]] || {
    echo "error: $SRC/platform/$CAM/sub/$FW does not exist." >&2
    echo "       Supported firmware for $CAM:" >&2
    ls "$SRC/platform/$CAM/sub" 2>/dev/null | grep -E '^[0-9]' | sed 's/^/         /' >&2
    exit 1
}

# ---- prerequisites --------------------------------------------------------
missing=0

if ! command -v arm-none-eabi-gcc >/dev/null; then
    echo "MISSING: arm-none-eabi-gcc (the ARM cross-compiler)" >&2
    echo "  brew install --cask gcc-arm-embedded" >&2
    missing=1
fi

# macOS ships GNU make 3.81, which is old but has built CHDK for years. Prefer
# a newer gmake if Homebrew has installed one.
if command -v gmake >/dev/null; then MAKE=gmake; else MAKE=make; fi
command -v "$MAKE" >/dev/null || { echo "MISSING: make  (xcode-select --install)" >&2; missing=1; }

command -v zip >/dev/null || { echo "MISSING: zip" >&2; missing=1; }
command -v cc  >/dev/null || { echo "MISSING: a host C compiler  (xcode-select --install)" >&2; missing=1; }

[[ $missing -eq 0 ]] || { echo >&2; echo "Install the above and run this again." >&2; exit 1; }

# ---- macOS override 1: the zip timestamp ---------------------------------
# The Makefile stamps zips with `date -R`. BSD date has not always supported
# -R, so use GNU date if coreutils is installed and fall back to a literal
# RFC-2822 format string otherwise.
if command -v gdate >/dev/null; then
    ZIPDATE="gdate -R"
elif date -R >/dev/null 2>&1; then
    ZIPDATE="date -R"
else
    ZIPDATE="date +%a,\\ %d\\ %b\\ %Y\\ %H:%M:%S\\ %z"
fi

# ---- macOS override 2: the compiler whitelist ----------------------------
# Upstream CHDK only whitelists GCC 4/5/8/9/10/11. Homebrew's cask currently
# installs 13.x, which builds these cameras cleanly, so wave the check through.
# GCC 10+ automatically gets -fno-tree-loop-distribute-patterns, the flag that
# fixes the known "builds from new GCC do not boot" problem.
GCCMAJ=$(arm-none-eabi-gcc -dumpversion | cut -d. -f1)
EXTRA=""
case "$GCCMAJ" in
    4|5|8|9|10|11) ;;
    *) EXTRA="DISABLE_GCC_VERSION_CHECK=1"
       echo "note: arm-none-eabi-gcc is version $GCCMAJ, outside CHDK's whitelist."
       echo "      Building anyway. If the result misbehaves on the camera in a way"
       echo "      that looks like a code-generation fault, retry with GCC 11."
       echo ;;
esac

# ---- build ----------------------------------------------------------------
echo "==> building CHDK for $CAM firmware $FW"
echo "    source:   $SRC"
echo "    make:     $MAKE ($("$MAKE" --version | head -1))"
echo "    compiler: $(arm-none-eabi-gcc -dumpversion)"
echo

cd "$SRC"
"$MAKE" PLATFORM="$CAM" PLATFORMSUB="$FW" ZIPDATE="$ZIPDATE" $EXTRA firzipsubcomplete

echo
echo "Built:"
ls -la "$SRC/bin/$CAM-$FW"-*.zip
echo
echo "Unzip the -full.zip onto a prepared card, or use ./setup-card-macos.sh"
echo "to prepare one from scratch."
