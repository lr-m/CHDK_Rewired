#!/bin/bash
# Flash an SD card with CHDK for the PowerShot A410 (firmware 100e), on macOS.
#
# This is a release script: it lives beside CHDK-A410-100e-card.zip and, given no second
# argument, flashes exactly that zip. It ERASES the card.
#
# Makes ONE FAT16 partition with the string "BOOTDISK" at offset 0x40 of its
# boot sector, which is what these pre-2011 PowerShots look for.
#
#   sudo ./flash-card-macos.sh disk4
#
# Find the disk identifier with:  diskutil list
# Use the WHOLE disk (disk4), not a partition (disk4s1).

set -euo pipefail

MODEL="a410"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

DISKARG="${1:-}"
SRC="${2:-}"

if [[ -z "$DISKARG" ]]; then
    echo "usage: sudo $0 <diskN> [source-dir-or-zip]" >&2
    echo "   eg: sudo $0 disk4" >&2
    echo >&2
    echo "Run 'diskutil list' first and be certain which disk is the card." >&2
    exit 1
fi

[[ $EUID -eq 0 ]] || { echo "error: must run as root (sudo $0 $*)" >&2; exit 1; }

DISKARG="${DISKARG#/dev/}"
DISK="/dev/$DISKARG"

command -v diskutil   >/dev/null || { echo "error: diskutil not found - is this macOS?" >&2; exit 1; }
command -v newfs_msdos >/dev/null || { echo "error: newfs_msdos not found" >&2; exit 1; }

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
    TMPSRC=$(mktemp -d /tmp/chdkcard.XXXXXX)
    echo "==> unpacking $(basename "$SRC")"
    /usr/bin/unzip -q "$SRC" -d "$TMPSRC"
    SRC="$TMPSRC"
else
    SRC="$(cd "$SRC" && pwd)"
fi

[[ -d "$SRC" ]]              || { echo "error: source dir $SRC not found" >&2; exit 1; }
[[ -f "$SRC/DISKBOOT.BIN" ]] || { echo "error: $SRC/DISKBOOT.BIN missing - wrong source?" >&2; exit 1; }

# ---- identify and vet the disk -------------------------------------------
PLIST=$(mktemp /tmp/chdkcard.XXXXXX.plist)
trap 'rm -f "$PLIST"; cleanup' EXIT
diskutil info -plist "$DISK" > "$PLIST" 2>/dev/null || {
    echo "error: $DISK is not a disk diskutil knows about. Run 'diskutil list'." >&2
    exit 1
}

get() { plutil -extract "$1" raw -o - "$PLIST" 2>/dev/null || echo ""; }

WHOLE=$(get WholeDisk)
INTERNAL=$(get Internal)
EJECTABLE=$(get Ejectable)
BYTES=$(get Size)
MODELNAME=$(get MediaName)

[[ "$WHOLE" == "true" ]] || {
    echo "error: $DISK is a partition or slice. Pass the whole disk, eg disk4 not disk4s1." >&2
    exit 1
}
if [[ "$INTERNAL" == "true" && "$EJECTABLE" != "true" ]]; then
    echo "error: $DISK is an internal, non-ejectable disk. Refusing." >&2
    exit 1
fi
[[ -n "$BYTES" ]] || { echo "error: could not read the size of $DISK" >&2; exit 1; }

GB=$(( BYTES / 1000000000 ))

# ---- cluster size --------------------------------------------------------
# FAT16 caps at 65524 clusters. Pick the smallest sectors-per-cluster that fits
# under that, and refuse anything needing more than 32KiB clusters: 64KiB is a
# FAT16 extension these old boot ROMs reject, and the camera then will not power
# on from the card at all.
PART_SECTORS=$(( (BYTES - 1048576) / 512 ))
SPC=0
for try in 4 8 16 32 64; do
    if [ $(( PART_SECTORS / try )) -lt 65524 ]; then SPC=$try; break; fi
done
if [ "$SPC" -eq 0 ] || [ "$SPC" -gt 64 ]; then
    echo "error: a ${GB}GB card needs clusters larger than 32KiB to fit FAT16," >&2
    echo "       which these cameras reject. Use a 2GB card or smaller." >&2
    exit 1
fi

# ---- volume label --------------------------------------------------------
# It must NOT be "CHDK". On FAT the volume label is an entry in the root
# directory, so a card labelled CHDK has two root entries spelled CHDK - the
# label and the A/CHDK directory. Some of these cameras' FAT drivers match the
# label first, and then every module load and the config file fail silently
# while the card looks perfect on the Mac. Cost the best part of a day to find.
LABEL="RWD_$(echo "$MODEL" | tr '[:lower:]' '[:upper:]')"
LABEL="$(echo "$LABEL" | cut -c1-11)"

# ---- confirm -------------------------------------------------------------
echo "About to ERASE $DISK  (${GB}GB, ${MODELNAME:-unknown})"
diskutil list "$DISK"
echo
echo "  source:       $SRC"
echo "  volume label: $LABEL"
echo "  cluster size: $(( SPC * 512 / 1024 ))KiB"
echo
read -rp "Type ERASE to continue: " CONFIRM
[[ "$CONFIRM" == "ERASE" ]] || { echo "aborted"; exit 1; }

# ---- partition and format ------------------------------------------------
echo "==> unmounting"
diskutil unmountDisk force "$DISK" >/dev/null

echo "==> writing MBR partition table"
diskutil partitionDisk "$DISK" MBRFormat "MS-DOS FAT16" "$LABEL" 100% >/dev/null

PART="${DISK}s1"
[[ -b "$PART" || -c "$PART" ]] || { echo "error: expected partition $PART does not exist" >&2; exit 1; }
RPART="/dev/r${DISKARG}s1"

echo "==> reformatting FAT16 with $(( SPC * 512 / 1024 ))KiB clusters"
diskutil unmount force "$PART" >/dev/null 2>&1 || true
newfs_msdos -F 16 -c "$SPC" -v "$LABEL" "$RPART" >/dev/null

# ---- MBR: partition type 0x06, marked active -----------------------------
# diskutil usually gets this right; set it explicitly so the result does not
# depend on the macOS version.
echo "==> setting partition type 0x06, active"
MBR=$(mktemp /tmp/chdkmbr.XXXXXX)
dd if="/dev/r$DISKARG" of="$MBR" bs=512 count=1 status=none
printf '\x80' | dd of="$MBR" bs=1 seek=446 conv=notrunc status=none   # 0x1BE status
printf '\x06' | dd of="$MBR" bs=1 seek=450 conv=notrunc status=none   # 0x1C2 type
dd if="$MBR" of="/dev/r$DISKARG" bs=512 count=1 conv=notrunc status=none
rm -f "$MBR"

# ---- boot signature: "BOOTDISK" at offset 0x40 of the partition ----------
# Must come AFTER the format, which rewrites the boot sector. 0x40 lands in the
# VBR's boot-code area, past the BPB, so overwriting it costs nothing.
# The sector is patched whole: raw devices on macOS reject unaligned writes.
echo "==> writing BOOTDISK signature at 0x40"
VBR=$(mktemp /tmp/chdkvbr.XXXXXX)
dd if="$RPART" of="$VBR" bs=512 count=1 status=none
printf 'BOOTDISK' | dd of="$VBR" bs=1 seek=64 conv=notrunc status=none
dd if="$VBR" of="$RPART" bs=512 count=1 conv=notrunc status=none
rm -f "$VBR"
sync

SIG=$(dd if="$RPART" bs=512 count=1 status=none | dd bs=1 skip=64 count=8 status=none)
[[ "$SIG" == "BOOTDISK" ]] || { echo "error: boot signature not written, read back: '$SIG'" >&2; exit 1; }

# ---- copy CHDK -----------------------------------------------------------
echo "==> mounting and copying CHDK"
diskutil mount "$PART" >/dev/null
MNT=$(diskutil info -plist "$PART" | plutil -extract MountPoint raw -o - - 2>/dev/null)
[[ -d "$MNT" ]] || { echo "error: could not find the mount point for $PART" >&2; exit 1; }

# -X keeps macOS extended attributes and resource forks off the card. Without
# it every file gets a ._shadow twin, which wastes FAT16 root entries and
# confuses some camera-side file listings.
cp -RX "$SRC"/. "$MNT"/

# Stop Spotlight and the trash from writing their own directories to the card.
mdutil -i off "$MNT" >/dev/null 2>&1 || true
rm -rf "$MNT/.Spotlight-V100" "$MNT/.fseventsd" "$MNT/.Trashes" 2>/dev/null || true
command -v dot_clean >/dev/null && dot_clean -m "$MNT" 2>/dev/null || true
sync

echo
echo "Card contents:"
ls -1 "$MNT"
echo

diskutil eject "$DISK" >/dev/null || diskutil unmountDisk "$DISK" >/dev/null

echo "OK - card is bootable (BOOTDISK signature verified) and safe to remove."
echo
echo "Now slide the card's physical LOCK switch to locked before putting it in"
echo "the camera. The camera only looks for DISKBOOT.BIN on a locked card; it can"
echo "still write photos, because the firmware ignores the switch once running."
