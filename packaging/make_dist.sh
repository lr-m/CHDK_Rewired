#!/usr/bin/env bash
#
# make_dist.sh - build the release package for one or more cameras.
#
# A package is what someone actually installs. One directory per camera under
# dist/, holding:
#
#   CHDK-<MODEL>-<fw>-card.zip   the whole card tree, zipped at its root, so
#                                unzipping it onto a prepared card is enough
#   flash-card-linux.sh          write that zip to an SD card, from scratch:
#   flash-card-macos.sh          partition FAT16, BOOTDISK signature, copy
#   flash-card-windows.ps1
#   READ_ME_FIRST.txt            what to do, start to finish
#   vers.req / ver.req           loose copies, for the firmware-version check
#   SHA256SUMS.txt               over everything above
#
# The source of truth is cameras/<model>/card/, which build-all.sh refreshes
# from the build. This script never builds anything - it packages what is there,
# and build-all.sh calls it after each camera it rebuilds.
#
# Usage:
#   ./packaging/make_dist.sh                 package every camera with a card tree
#   ./packaging/make_dist.sh a480 a470       only these
#   ./packaging/make_dist.sh --with-roms     include commercial Game Boy ROMs
#                                            (off by default - see below)
#   ./packaging/make_dist.sh -n              dry run
#
# Game Boy ROMs: CHDK/GBC/ on the card is where the Game Boy player looks for
# .gb/.gbc files. No ROMs are shipped - the package writes a note there telling
# you to drop your own in. --with-roms packages whatever is in the card tree.

set -uo pipefail

ROOT="$(dirname "$(dirname "$(readlink -f "$0")")")"
FLASHDIR="$(dirname "$(readlink -f "$0")")/flash"
DIST="$ROOT/dist"

WITH_ROMS=0
DRYRUN=0
MODELS=()
for arg in "$@"; do
    case "$arg" in
        --with-roms)  WITH_ROMS=1 ;;
        -n|--dry-run) DRYRUN=1 ;;
        -h|--help)    sed -n '2,40p' "$0"; exit 0 ;;
        -*)           echo "unknown option: $arg" >&2; exit 1 ;;
        *)            MODELS+=("$arg") ;;
    esac
done

command -v zip >/dev/null || { echo "error: zip not installed" >&2; exit 1; }

if [[ ${#MODELS[@]} -eq 0 ]]; then
    for d in "$ROOT"/cameras/*/card; do
        [[ -f "$d/DISKBOOT.BIN" ]] && MODELS+=("$(basename "$(dirname "$d")")")
    done
fi

# ROM images that are not ours to redistribute.
ROM_GLOBS=( "CHDK/GBC/*.gb" "CHDK/GBC/*.GB" "CHDK/GBC/*.gbc" "CHDK/GBC/*.GBC" )

# 100b -> 1.00B, 102c -> 1.02C. This is the string the camera shows under
# FUNC/SET+DISP, and a mismatch means the build will not load at all.
fw_display() {
    local fw=$1
    echo "${fw:0:1}.${fw:1:2}$(echo "${fw:3}" | tr '[:lower:]' '[:upper:]')"
}

# The firmware this card was built against, from the <MODEL>.TXT note in the
# card root. Each build is compiled against one exact firmware revision, so this
# is what the package tells the user to check the camera against.
card_firmware() {
    local card="$ROOT/cameras/$1/card"
    grep -hoE '^firmware:[[:space:]]*[0-9a-z]+' "$card"/*.TXT 2>/dev/null \
        | head -1 | awk '{print $2}'
}

made=()
failed=()

for model in "${MODELS[@]}"; do
    card="$ROOT/cameras/$model/card"
    if [[ ! -f "$card/DISKBOOT.BIN" ]]; then
        echo "$model: no card tree at cameras/$model/card - skipped"
        failed+=("$model (no card tree)")
        continue
    fi

    fw=$(card_firmware "$model")
    if [[ -z "$fw" ]]; then
        echo "$model: no 'firmware:' line in cameras/$model/card/*.TXT - skipped" >&2
        failed+=("$model (no firmware note)")
        continue
    fi

    MODEL_UC=$(echo "$model" | tr '[:lower:]' '[:upper:]')
    FWVER=$(fw_display "$fw")
    PKG="$DIST/$MODEL_UC"
    CARDZIP="CHDK-$MODEL_UC-$fw-card.zip"

    # PS.FIR on the card means this body can be booted through PLAY -> MENU ->
    # "Firm Update...", which needs no special card at all. Without it the only
    # route is a bootable card, which is what the flash scripts make.
    HAS_FIR=0; [[ -f "$card/PS.FIR" ]] && HAS_FIR=1

    BUILT=$(find "$card" -maxdepth 1 -iname '*.txt' -exec grep -hoE '^date:.*' {} + 2>/dev/null | head -1 | sed 's/^date://; s/^ *//')
    [[ -n "$BUILT" ]] || BUILT=$(date -r "$card/DISKBOOT.BIN" '+%a, %d %b %Y %H:%M:%S %z')
    MD5=$(md5sum "$card/DISKBOOT.BIN" | cut -d' ' -f1)

    echo "=============================================================="
    echo "$MODEL_UC  fw $fw  built $BUILT"

    if [[ "$DRYRUN" -eq 1 ]]; then
        echo "  would write $PKG/{$CARDZIP,flash-card-*,READ_ME_FIRST.txt,SHA256SUMS.txt}"
        continue
    fi

    # ---- stage the card tree ---------------------------------------------
    stage=$(mktemp -d)
    cp -r "$card"/. "$stage"/
    # Placeholders that only exist to keep empty directories in git.
    find "$stage" -name .gitkeep -delete

    if [[ "$WITH_ROMS" -eq 0 ]]; then
        dropped=0
        for g in "${ROM_GLOBS[@]}"; do
            for f in "$stage"/$g; do
                [[ -f "$f" ]] || continue
                rm -f "$f"; dropped=1
            done
        done
        if [[ -d "$stage/CHDK/GBC" ]]; then
            cat > "$stage/CHDK/GBC/README.TXT" <<'EOF'
Put Game Boy ROMs here.

The emulator is under Miscellaneous Stuff -> Games -> Game Boy. It reads
.gb and .gbc files from this folder, straight off the card - copy one in
and it appears in the list.

No ROMs ship with this release: the ones that were used to develop it are
commercial games and are not ours to hand out.
EOF
        fi
        [[ "$dropped" -eq 1 ]] && echo "  (commercial ROMs left out - pass --with-roms to include them)"
    fi

    # ---- build the package ------------------------------------------------
    rm -rf "$PKG"
    mkdir -p "$PKG"

    # -X drops uid/gid and extra timestamps: the card is FAT, none of it survives
    # the copy anyway, and leaving it out keeps the zip stable between machines.
    ( cd "$stage" && zip -qr -X "$PKG/$CARDZIP" . ) || {
        echo "  FAILED to zip" >&2; failed+=("$model (zip)"); rm -rf "$stage"; continue
    }
    # Both names, always. Which one a body looks for varies by generation, the
    # two files are byte-identical marker files whose content is never read, and
    # READ_ME_FIRST tells the user to copy "both" - so a package carrying only
    # one sends them hunting for a file that isn't there, on step 2. The A470
    # and A480 card trees carry only vers.req, hence the fallback.
    for r in vers.req ver.req; do
        if [[ -f "$stage/$r" ]]; then
            cp "$stage/$r" "$PKG/$r"
        elif [[ -f "$stage/vers.req" ]]; then
            cp "$stage/vers.req" "$PKG/$r"
        fi
    done
    rm -rf "$stage"

    # ---- flash scripts ----------------------------------------------------
    for s in flash-card-linux.sh flash-card-macos.sh flash-card-windows.ps1; do
        sed -e "s/@MODEL@/$model/g" \
            -e "s/@MODEL_UC@/$MODEL_UC/g" \
            -e "s/@FW@/$fw/g" \
            -e "s/@CARDZIP@/$CARDZIP/g" \
            "$FLASHDIR/$s" > "$PKG/$s"
    done
    chmod +x "$PKG"/flash-card-linux.sh "$PKG"/flash-card-macos.sh

    # ---- READ_ME_FIRST.txt ------------------------------------------------
    {
        title="CHDK Rewired for the PowerShot $MODEL_UC  (firmware $FWVER)"
        echo "$title"
        printf '%*s\n' "${#title}" '' | tr ' ' '='
        echo
        cat <<EOF
CHDK Rewired is a custom build of CHDK. It adds a circuit-bending mode to
the image pipeline, a persistent overlay, multiple exposure and a Game Boy
player, on top of everything stock CHDK does.
EOF
        cat <<EOF

Nothing here can damage the camera. It all runs in memory and never
touches the camera's firmware - take the card out and it's stock again.
If it ever freezes, pull the battery.


1. USE A 2GB OR SMALLER SD CARD
   Anything bigger is SDHC and this camera cannot read it at all.

2. CHECK THE FIRMWARE VERSION
   Copy vers.req and ver.req (both in this folder) onto the card.
   Camera on in PLAY mode. Hold FUNC/SET, then press DISP.

   It must say $FWVER. Each build is compiled against one exact firmware
   revision; on anything else CHDK will not load at all.

3. WRITE THE CARD

   !! THIS ERASES AN ENTIRE DISK, PERMANENTLY. If you name the wrong
   !! one, everything on it is gone - your hard drive, your backup
   !! drive, whatever it was. There is no undo and no recycle bin.
   !! Disk names change between sessions, so never reuse one you
   !! remember. Work it out fresh, every time, like this:

   Put the card in your reader and list the disks:

       Linux:    lsblk -o NAME,SIZE,RM,TYPE,MOUNTPOINT
       Mac:      diskutil list
       Windows:  Get-Disk        (Administrator PowerShell)

   Now UNPLUG the card and run the same command again. The entry that
   disappeared is the card. Plug it back in, run it a third time, and
   check the same entry comes back. That name is the card, and nothing
   else can be. Check the size matches the card you are holding too.

   Pass the WHOLE disk, not a partition: /dev/sde not /dev/sde1,
   disk4 not disk4s1.

   Then run the script for your system, from this folder:

       Linux:    sudo ./flash-card-linux.sh /dev/sdX
       Mac:      sudo ./flash-card-macos.sh diskN
       Windows:  .\\flash-card-windows.ps1 -DiskNumber N
                 from an Administrator PowerShell prompt

   Each one prints the disk it is about to erase and makes you type
   ERASE before it touches anything. READ THAT LINE. It is your last
   chance to notice it names your hard drive rather than the card.

   The repository README walks through all of this in more detail.

4. PUT IT IN THE CAMERA
   Slide the card's physical LOCK switch to locked first. That is not a
   mistake - the camera only looks for CHDK on a locked card, and still
   saves photos normally.

   Switch on. CHDK loads by itself.
EOF
        if [[ "$HAS_FIR" -eq 1 ]]; then
            cat <<EOF

   If you would rather not repartition a card: unzip $CARDZIP
   onto a card the camera formatted, then PLAY mode -> MENU -> scroll to
   the bottom -> "Firm Update...". Same result, but it has to be done
   again after every power-off.
EOF
        fi
        cat <<EOF

5. USING IT
   PRINT/SHARE (the printer icon) toggles "<ALT>" mode.
   In <ALT>: MENU opens CHDK's own menu.
   Bend mode is on the shooting screen - FUNC/SET opens the patchbay.

   Presets are saved to CHDK/BENDS on the card, and every photo gets a
   sidecar file recording what it was bent with.

   The manual in the repository covers all of it.
EOF
        cat <<EOF


Build: $model $fw, $BUILT
DISKBOOT.BIN md5 $MD5

Frozen camera: pull the battery, no harm done. Nothing here is written
to the camera itself, so taking the card out always gets you a stock
camera back.
EOF
    } > "$PKG/READ_ME_FIRST.txt"

    # ---- checksums --------------------------------------------------------
    ( cd "$PKG" && find . -type f ! -name SHA256SUMS.txt -printf '%P\n' | sort \
        | xargs -d '\n' sha256sum > SHA256SUMS.txt )

    echo "  -> dist/$MODEL_UC/  ($(du -sh "$PKG" | cut -f1))"
    made+=("$MODEL_UC $fw")
done

# ---- dist index ------------------------------------------------------------
if [[ "$DRYRUN" -eq 0 && -d "$DIST" ]]; then
    {
        echo "# dist - release packages"
        echo
        echo "Generated by \`packaging/make_dist.sh\`; \`build-all.sh\` refreshes a"
        echo "camera's package as soon as it rebuilds it. Everything here is output -"
        echo "delete the lot and it comes back."
        echo
        echo "One directory per camera. Each holds the card tree as a zip, the three"
        echo "card-flashing scripts, a READ_ME_FIRST, loose"
        echo "\`vers.req\`/\`ver.req\` for the firmware check, and SHA256SUMS.txt."
        echo
        echo "| Package | Firmware | Built | Card zip |"
        echo "|---|---|---|---|"
        for p in "$DIST"/*/; do
            [[ -f "$p/READ_ME_FIRST.txt" ]] || continue
            n=$(basename "$p")
            z=$(cd "$p" && ls CHDK-*-card.zip 2>/dev/null | head -1)
            b=$(grep -oE '^Build: .*' "$p/READ_ME_FIRST.txt" | sed 's/^Build: //')
            f=$(echo "$b" | awk '{print $2}')
            f=${f%,}
            d=$(echo "$b" | cut -d, -f2- | sed 's/^ *//')
            echo "| [$n]($n/) | $f | $d | \`$z\` |"
        done
        echo
        echo "**No Game Boy ROMs are included.** \`CHDK/GBC/\` ships empty with a note in"
        echo "it - put your own \`.gb\`/\`.gbc\` files there."
    } > "$DIST/README.md"
fi

echo "=============================================================="
echo "packaged: ${#made[@]}"
for m in ${made+"${made[@]}"}; do echo "  $m"; done
if [[ ${#failed[@]} -gt 0 ]]; then
    echo "not packaged: ${#failed[@]}"
    for f in "${failed[@]}"; do echo "  $f"; done
fi
[[ ${#failed[@]} -eq 0 ]]
