#!/usr/bin/env bash
# Flash an SD card with CHDK for the PowerShot A410 (firmware 100e), on Linux.
#
# This is a release script: it lives beside CHDK-A410-100e-card.zip and, given no second
# argument, flashes exactly that zip. It ERASES the card.
#
# These cameras' boot ROM only reads FAT16, so the card gets ONE FAT16 partition
# with the string "BOOTDISK" written at offset 0x40 of its boot sector.
#
# Usage:  sudo ./flash-card-linux.sh /dev/sdX [source-dir-or-zip]
#
# Find the card with:  lsblk -o NAME,SIZE,RM,MODEL
# Pass the WHOLE disk (/dev/sde), not a partition (/dev/sde1).

set -euo pipefail

MODEL="a410"
HERE="$(dirname "$(readlink -f "$0")")"

DEV="${1:-}"
SRC="${2:-}"

if [[ -z "$DEV" ]]; then
    echo "usage: sudo $0 /dev/sdX [source-dir-or-zip]" >&2
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
    SRC="$HERE/CHDK-A410-100e-card.zip"
    [[ -f "$SRC" ]] || { echo "error: CHDK-A410-100e-card.zip is missing from $HERE" >&2; exit 1; }
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

BYTES=$(blockdev --getsize64 "$DEV")
GB=$(( BYTES / 1000000000 ))
if (( BYTES > 4294967296 )); then
    echo "error: $DEV is ${GB}GB. FAT16 tops out at 4GB, and this camera cannot read" >&2
    echo "       SDHC at all. Use a 2GB card or smaller." >&2
    exit 1
fi

ROOTDEV=$(findmnt -no SOURCE / | sed 's/[0-9]*$//')
[[ "$DEV" == "$ROOTDEV" ]] && { echo "error: $DEV is the root disk. Refusing." >&2; exit 1; }

echo "About to ERASE $DEV (${GB}GB, model: $(cat "/sys/block/$BASE/device/model" 2>/dev/null | xargs))"
lsblk -o NAME,SIZE,FSTYPE,LABEL,MOUNTPOINT "$DEV"
echo
read -rp "Type ERASE to continue: " CONFIRM
[[ "$CONFIRM" == "ERASE" ]] || { echo "aborted"; exit 1; }

# ---- unmount anything on the card ---------------------------------------
for part in $(lsblk -lno NAME "$DEV" | tail -n +2); do
    umount "/dev/$part" 2>/dev/null || true
done

# ---- partition: single primary FAT16, marked active ----------------------
echo "==> writing partition table"
wipefs -a "$DEV" >/dev/null
sfdisk "$DEV" >/dev/null <<'EOF'
label: dos
start=2048, type=06, bootable
EOF
partprobe "$DEV"; sleep 2

PART="${DEV}1"
[[ -b "${DEV}p1" ]] && PART="${DEV}p1"

# ---- format FAT16 --------------------------------------------------------
# Pick the smallest cluster size that keeps the volume under the 65524-cluster
# FAT16 cap, capped at 32KiB (-s 64). 64KiB clusters are a FAT16 extension that
# older boot ROMs reject - a PowerShot A470 will not power on at all from a card
# formatted that way, while an A480 reads it fine.
PART_SECTORS=$(( (BYTES - 1048576) / 512 ))
SPC=0
for try in 4 8 16 32 64; do
    if [ $(( PART_SECTORS / try )) -lt 65524 ]; then
        SPC=$try
        break
    fi
done
if [ "$SPC" -eq 0 ]; then
    echo "error: ${GB}GB needs >32KiB clusters to fit FAT16, which old cameras" >&2
    echo "       reject. Use a card of 2GB or less." >&2
    exit 1
fi
echo "==> formatting $PART as FAT16 ($(( SPC * 512 / 1024 ))KiB clusters, $(( PART_SECTORS / SPC )) clusters)"
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
echo "==> volume label: $LABEL"
mkfs.fat -F 16 -s "$SPC" -n "$LABEL" "$PART" >/dev/null

# ---- boot signature: "BOOTDISK" at offset 0x40 ---------------------------
# Must come AFTER mkfs, which rewrites the boot sector.
echo "==> writing BOOTDISK signature at 0x40"
printf 'BOOTDISK' | dd of="$PART" bs=1 seek=64 conv=notrunc status=none
sync

# ---- copy CHDK ------------------------------------------------------------
MNT=$(mktemp -d)
mount "$PART" "$MNT"
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
