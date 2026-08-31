#!/bin/bash
# Build CHDK from source on Windows, inside MSYS2.
#
# Run this from the "MSYS2 MINGW64" shell (the blue icon), NOT the plain
# "MSYS2 MSYS" shell. CHDK's makefiles detect the host from `uname -s`, and only
# the MINGW64/UCRT64 shells report a name they recognise as Windows; from the
# MSYS shell the build guesses wrong about executable extensions.
#
#   ./build-windows-msys2.sh                 # defaults to a640 / 100b
#   ./build-windows-msys2.sh a530 100a
#   ./build-windows-msys2.sh a640 100b /c/path/to/chdk-src
#
# Produces, in <source>/bin/ :
#   <cam>-<fw>-1.7.0-0.zip        DISKBOOT.BIN, PS.FIR, modules, docs
#   <cam>-<fw>-1.7.0-0-full.zip   the above plus the complete CHDK card tree

set -euo pipefail

CAM="${1:-a640}"
FW="${2:-100b}"
SRC="${3:-}"

# ---- shell check ----------------------------------------------------------
UNAME="$(uname -s)"
case "$UNAME" in
    MINGW*|CYGWIN*) ;;
    MSYS*)
        echo "error: this is the MSYS2 MSYS shell (uname: $UNAME)." >&2
        echo "       Close it and open 'MSYS2 MINGW64' from the Start menu instead." >&2
        echo "       CHDK's makefiles only recognise MINGW* and CYGWIN* as Windows;" >&2
        echo "       from MSYS they fall through to a generic host and get the .exe" >&2
        echo "       suffix on the build tools wrong." >&2
        exit 1 ;;
    *)
        echo "error: this does not look like MSYS2 (uname: $UNAME)." >&2
        echo "       On Linux use the Makefile directly; on macOS use build-macos.sh." >&2
        exit 1 ;;
esac

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
    echo "       Pass it: $0 $CAM $FW /c/path/to/chdk-src" >&2
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
    echo "  Try MSYS2 first:" >&2
    echo "    pacman -S mingw-w64-x86_64-arm-none-eabi-gcc mingw-w64-x86_64-arm-none-eabi-binutils" >&2
    echo "  If pacman does not have those packages, install ARM's official toolchain" >&2
    echo "  (\"Arm GNU Toolchain\", arm-none-eabi, Windows installer) from" >&2
    echo "  https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads" >&2
    echo "  and add its bin directory to PATH for this shell, eg:" >&2
    echo "    export PATH=\"/c/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/13.2 rel1/bin:\$PATH\"" >&2
    missing=1
fi

for t in make zip gcc; do
    command -v "$t" >/dev/null || { echo "MISSING: $t   (pacman -S $t)" >&2; missing=1; }
done
# The build compiles host-side helper tools (pakwif, the signature finders) with
# the host compiler, so a working native gcc matters as much as the cross one.
command -v gcc >/dev/null || echo "  host gcc comes from: pacman -S mingw-w64-x86_64-toolchain" >&2

[[ $missing -eq 0 ]] || { echo >&2; echo "Install the above and run this again." >&2; exit 1; }

# ---- compiler whitelist ---------------------------------------------------
# Upstream CHDK only whitelists GCC 4/5/8/9/10/11. Newer ones build these
# cameras cleanly, and GCC 10+ automatically gets -fno-tree-loop-distribute-
# patterns, the flag that fixes the known "builds from new GCC do not boot"
# problem, so wave the check through.
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
echo "    shell:    $UNAME"
echo "    compiler: $(arm-none-eabi-gcc -dumpversion)"
echo

cd "$SRC"
make PLATFORM="$CAM" PLATFORMSUB="$FW" $EXTRA firzipsubcomplete

echo
echo "Built:"
ls -la "$SRC/bin/$CAM-$FW"-*.zip
echo
echo "Unzip the -full.zip onto a prepared card, or use setup-card-windows.ps1"
echo "from an Administrator PowerShell prompt to prepare one from scratch."
