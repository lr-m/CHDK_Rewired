#!/usr/bin/env bash
# Flash an SD card with CHDK for the PowerShot A460 (firmware 100d), on Linux.
#
# This is a release script: it lives beside CHDK-A460-100d-card.zip and, given no second
# argument, flashes exactly that zip. It ERASES the card.
#
# These cameras' boot ROM only reads FAT16, so the card gets a FAT16 partition
# with the string "BOOTDISK" written at offset 0x40 of its boot sector.
#
# Two layouts, picked from the card size:
#
#   single  one FAT16 partition holding everything. Used whenever the card fits
#           FAT16 with clusters of 32KiB or less - roughly 2GB and under.
#
#   split   a 16MiB FAT16 boot partition holding only DISKBOOT.BIN, then the
#           rest of the card as FAT32 holding the whole CHDK tree. Only for
#           bodies whose port mounts a FAT32 partition in preference to the
#           first one (CAM_MULTIPART plus CHDK's FAT32 autodetect in boot.c):
#           the boot ROM loads DISKBOOT.BIN from the FAT16 partition, then the
#           firmware mounts the FAT32 one as A/, so that is where CHDK looks for
#           its modules and where photos go. Put the CHDK tree on the FAT16 side
#           instead and the camera boots but reports missing modules.
#
# Usage:  sudo ./flash-card-linux.sh /dev/sdX [source-dir-or-zip]
#
# Find the card with:  lsblk -o NAME,SIZE,RM,MODEL
# Pass the WHOLE disk (/dev/sde), not a partition (/dev/sde1).

set -euo pipefail

MODEL="a460"
HERE="$(dirname "$(readlink -f "$0")")"

# Whether this body can use the split layout: tested, untested, or no.
LARGE_CARD="no"

# 32GiB. Above this is SDXC, which every body here predates.
MAX_SPLIT_BYTES=34359738368

# --allow-64k-clusters: let the FAT16 volume use 64KiB clusters, which is the
# only way a card between roughly 2GB and 4GB fits in one FAT16 partition.
#
# Off by default and deliberately awkward to reach, because 64KiB clusters are
# a FAT16 extension and the older boot ROMs reject them outright - a PowerShot
# A470 will not power on at all from a card formatted this way, while an A480
# reads it fine. Which side of that line any given body falls on is only
# knowable by trying it, and the failure looks like a dead camera rather than
# an error message. Use a 2GB card unless you are deliberately testing this.
ALLOW_64K=0
ARGS=()
for a in "$@"; do
    case "$a" in
        --allow-64k-clusters) ALLOW_64K=1 ;;
        *) ARGS+=("$a") ;;
    esac
done
set -- ${ARGS+"${ARGS[@]}"}

DEV="${1:-}"
SRC="${2:-}"

if [[ -z "$DEV" ]]; then
    echo "usage: sudo $0 /dev/sdX [source-dir-or-zip] [--allow-64k-clusters]" >&2
    echo "   eg: sudo $0 /dev/sde" >&2
    echo >&2
    echo "Run 'lsblk -o NAME,SIZE,RM,MODEL' first and be certain which one is the card." >&2
    exit 1
fi
if [[ $EUID -ne 0 ]]; then
    echo "error: must run as root (sudo $0 $*)" >&2
    exit 1
fi

BASE="$(basename "$DEV")"

# ---- resolve the source --------------------------------------------------
# Default: the card zip shipped in this folder, unpacked to a temp directory
# that is removed on exit.
TMPSRC=""
cleanup() { [[ -n "$TMPSRC" ]] && rm -rf "$TMPSRC"; }
trap cleanup EXIT

if [[ -z "$SRC" ]]; then
    SRC="$HERE/CHDK-A460-100d-card.zip"
    [[ -f "$SRC" ]] || { echo "error: CHDK-A460-100d-card.zip is missing from $HERE" >&2; exit 1; }
fi
if [[ -f "$SRC" && "$SRC" == *.zip ]]; then
    command -v unzip >/dev/null || { echo "error: unzip not installed (apt install unzip)" >&2; exit 1; }
    TMPSRC=$(mktemp -d)
    echo "==> unpacking $(basename "$SRC")"
    unzip -q "$SRC" -d "$TMPSRC"
    SRC="$TMPSRC"
fi

# ---- safety checks -------------------------------------------------------
[[ -b "$DEV" ]]                || { echo "error: $DEV is not a block device" >&2; exit 1; }
[[ -d "/sys/block/$BASE" ]]    || { echo "error: $DEV is a partition; pass the whole disk (e.g. /dev/sde)" >&2; exit 1; }
[[ -d "$SRC" ]]                || { echo "error: source dir $SRC not found" >&2; exit 1; }
[[ -f "$SRC/DISKBOOT.BIN" ]]   || { echo "error: $SRC/DISKBOOT.BIN missing" >&2; exit 1; }

if [[ "$(cat "/sys/block/$BASE/removable")" != "1" ]]; then
    echo "error: $DEV is not a removable device. Refusing." >&2
    exit 1
fi

ROOTDEV=$(findmnt -no SOURCE / | sed 's/[0-9]*$//')
[[ "$DEV" == "$ROOTDEV" ]] && { echo "error: $DEV is the root disk. Refusing." >&2; exit 1; }

BYTES=$(blockdev --getsize64 "$DEV")
GB=$(( BYTES / 1000000000 ))

# ---- choose the layout ---------------------------------------------------
# Pick the smallest cluster size that keeps a single FAT16 volume under the
# 65524-cluster cap, capped at 32KiB (-s 64). 64KiB clusters are a FAT16
# extension that older boot ROMs reject - a PowerShot A470 will not power on at
# all from a card formatted that way, while an A480 reads it fine.
PART_SECTORS=$(( (BYTES - 1048576) / 512 ))
TRIES="4 8 16 32 64"
[[ "$ALLOW_64K" -eq 1 ]] && TRIES="$TRIES 128"
SPC=0
for try in $TRIES; do
    if [ $(( PART_SECTORS / try )) -lt 65524 ]; then
        SPC=$try
        break
    fi
done

LAYOUT=single
if [ "$SPC" -eq 0 ]; then
    if [[ "$LARGE_CARD" == "no" ]]; then
        if (( BYTES > 4294967296 )); then
            echo "error: $DEV is ${GB}GB. The A460 can only boot CHDK from a single" >&2
            echo "       FAT16 partition, and FAT16 tops out at 4GB. Use a 2GB card or smaller." >&2
        else
            echo "error: ${GB}GB needs >32KiB clusters to fit FAT16, which old cameras" >&2
            echo "       reject. Use a card of 2GB or less, or pass --allow-64k-clusters" >&2
            echo "       to try 64KiB clusters anyway (see the note at the top)." >&2
        fi
        exit 1
    fi
    if (( BYTES > MAX_SPLIT_BYTES )); then
        echo "error: $DEV is ${GB}GB. Cards over 32GB are SDXC, which the A460" >&2
        echo "       predates. Use a card of 32GB or less." >&2
        exit 1
    fi
    LAYOUT=split
fi

# Volume label: RWD_<model>.
#
# It must NOT be "CHDK", and this is not cosmetic. On FAT the volume label is
# stored as an entry in the root directory, with the volume-id attribute (0x08)
# in place of the directory attribute. Label the card CHDK and the root then
# holds two entries spelled CHDK: the label, and the A/CHDK directory CHDK
# itself lives in. The a460's FAT driver matches the label entry first, finds it
# is not a directory, and stops - so opendir("A/CHDK") returns NULL, nothing
# beneath it can be opened, and every module load, the config file and the
# module probe all fail, while the card looks perfect on a PC.
# Cost the best part of a day to find; do not "tidy" this back to CHDK.
#
# 11 characters is the FAT label limit. "RWD_A460" is 8, so there is room.
LABEL="RWD_$(echo "$MODEL" | tr '[:lower:]' '[:upper:]')"
LABEL="${LABEL:0:11}"

echo "About to ERASE $DEV (${GB}GB, model: $(cat "/sys/block/$BASE/device/model" 2>/dev/null | xargs))"
lsblk -o NAME,SIZE,FSTYPE,LABEL,MOUNTPOINT "$DEV"
echo
if [[ "$LAYOUT" == "split" ]]; then
    echo "  layout: 16MiB FAT16 boot partition + $(( GB ))GB FAT32 partition ($LABEL)"
    if [[ "$LARGE_CARD" == "untested" ]]; then
        echo
        echo "  NOTE: this two-partition layout has been tested on the A480, not yet on"
        echo "        the A460. If the camera does not see the card, or CHDK reports"
        echo "        missing modules, use a 2GB card instead - and please say so."
    fi
else
    echo "  layout: one FAT16 partition ($LABEL, $(( SPC * 512 / 1024 ))KiB clusters)"
fi
echo
read -rp "Type ERASE to continue: " CONFIRM
[[ "$CONFIRM" == "ERASE" ]] || { echo "aborted"; exit 1; }

# ---- unmount anything on the card ---------------------------------------
for part in $(lsblk -lno NAME "$DEV" | tail -n +2); do
    umount "/dev/$part" 2>/dev/null || true
done

# ---- partition -----------------------------------------------------------
# Partition 1 is always the FAT16 boot partition, primary and marked active.
echo "==> writing partition table"
wipefs -a "$DEV" >/dev/null
if [[ "$LAYOUT" == "split" ]]; then
    sfdisk "$DEV" >/dev/null <<'EOF'
label: dos
start=2048, size=16MiB, type=06, bootable
type=0c
EOF
else
    sfdisk "$DEV" >/dev/null <<'EOF'
label: dos
start=2048, type=06, bootable
EOF
fi
partprobe "$DEV"; sleep 2

PART="${DEV}1"; DATA="${DEV}2"
[[ -b "${DEV}p1" ]] && { PART="${DEV}p1"; DATA="${DEV}p2"; }

# ---- format --------------------------------------------------------------
if [[ "$LAYOUT" == "split" ]]; then
    # 2KiB clusters: 16MiB / 2KiB = 8192 clusters, enough to be FAT16 not FAT12.
    echo "==> formatting $PART as FAT16 boot partition, label RWD_BOOT"
    mkfs.fat -F 16 -s 4 -n RWD_BOOT "$PART" >/dev/null
    echo "==> formatting $DATA as FAT32, label $LABEL"
    mkfs.fat -F 32 -n "$LABEL" "$DATA" >/dev/null
else
    if [ "$SPC" -eq 128 ]; then
        echo
        echo "WARNING: using 64KiB clusters. This is a FAT16 extension that some of" >&2
        echo "         these boot ROMs refuse - if the camera will not power on from" >&2
        echo "         this card, that is why, and a 2GB card is the fix. The card" >&2
        echo "         itself is fine; take it out and the camera is stock again." >&2
        echo
    fi
    echo "==> formatting $PART as FAT16 ($(( SPC * 512 / 1024 ))KiB clusters, $(( PART_SECTORS / SPC )) clusters), label $LABEL"
    mkfs.fat -F 16 -s "$SPC" -n "$LABEL" "$PART" >/dev/null
fi

# ---- boot signature: "BOOTDISK" at offset 0x40 ---------------------------
# Must come AFTER mkfs, which rewrites the boot sector.
echo "==> writing BOOTDISK signature at 0x40"
printf 'BOOTDISK' | dd of="$PART" bs=1 seek=64 conv=notrunc status=none
sync

# ---- copy CHDK ------------------------------------------------------------
# Split: the boot ROM needs only DISKBOOT.BIN on the FAT16 side; everything,
# DISKBOOT.BIN included, goes on the FAT32 side the firmware actually mounts.
MNT=$(mktemp -d)
if [[ "$LAYOUT" == "split" ]]; then
    mount "$PART" "$MNT"
    cp "$SRC/DISKBOOT.BIN" "$MNT"/
    sync; umount "$MNT"
    mount "$DATA" "$MNT"
else
    mount "$PART" "$MNT"
fi
echo "==> copying CHDK"
cp -r "$SRC"/. "$MNT"/
sync
echo
echo "Card contents:"
ls -1 "$MNT"
umount "$MNT"; rmdir "$MNT"

# ---- verify ---------------------------------------------------------------
SIG=$(dd if="$PART" bs=1 skip=64 count=8 status=none)
echo
if [[ "$SIG" == "BOOTDISK" ]]; then
    echo "OK - card is bootable (signature verified)."
    echo
    echo "Now slide the card's physical LOCK switch to locked before putting it in"
    echo "the camera. The camera only looks for DISKBOOT.BIN on a locked card; it can"
    echo "still write photos, because the firmware ignores the switch once running."
else
    echo "WARNING - boot signature not found, got: '$SIG'"
    exit 1
fi
