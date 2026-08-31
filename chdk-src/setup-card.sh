#!/usr/bin/env bash
# Prepare an SD card as a CHDK boot card for pre-2011 Canon PowerShots (A470 / A480).
#
# These cameras' boot ROM only reads FAT16, so the card gets ONE FAT16 partition
# with the string "BOOTDISK" written at offset 0x40 of its boot sector.
# (Dual FAT16+FAT32 partitioning is only needed for cards larger than 4GB.)
#
# Usage:  sudo ./setup-card.sh [-u|--update|-f|--format] /dev/sdX [source-dir]
# Default source-dir is ./card-a480
#
# By default the script auto-detects: if /dev/sdX1 already carries the
# BOOTDISK signature (i.e. a previous run of this script set it up), it just
# copies files over; otherwise it partitions and formats from scratch.
#
# -u, --update    force file-copy-only mode; errors out instead of formatting
#                 if the card doesn't already carry the BOOTDISK signature.
# -f, --format    force a full erase/repartition/format even if the card
#                 already looks like a set-up CHDK card.

set -euo pipefail

MODE=auto
if [[ "${1:-}" == "-u" || "${1:-}" == "--update" ]]; then
    MODE=update
    shift
elif [[ "${1:-}" == "-f" || "${1:-}" == "--format" ]]; then
    MODE=format
    shift
fi

DEV="${1:-}"
SRC="${2:-$(dirname "$(readlink -f "$0")")/../cameras/a480/card}"

if [[ -z "$DEV" ]]; then
    echo "usage: sudo $0 [-u|--update|-f|--format] /dev/sdX [source-dir]" >&2
    exit 1
fi
if [[ $EUID -ne 0 ]]; then
    echo "error: must run as root (sudo $0 $DEV)" >&2
    exit 1
fi

BASE="$(basename "$DEV")"

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
    echo "error: $DEV is ${GB}GB. FAT16 tops out at 4GB, so a card this big needs" >&2
    echo "       the dual-partition layout instead. Refusing." >&2
    exit 1
fi

ROOTDEV=$(findmnt -no SOURCE / | sed 's/[0-9]*$//')
[[ "$DEV" == "$ROOTDEV" ]] && { echo "error: $DEV is the root disk. Refusing." >&2; exit 1; }

# ---- auto-detect: does this card already carry our BOOTDISK signature? ----
CANDIDATE_PART="${DEV}1"
[[ -b "${DEV}p1" ]] && CANDIDATE_PART="${DEV}p1"
ALREADY_SET_UP=0
if [[ -b "$CANDIDATE_PART" ]]; then
    SIG=$(dd if="$CANDIDATE_PART" bs=1 skip=64 count=8 status=none 2>/dev/null || true)
    [[ "$SIG" == "BOOTDISK" ]] && ALREADY_SET_UP=1
fi

if [[ "$MODE" == "auto" ]]; then
    if [[ "$ALREADY_SET_UP" -eq 1 ]]; then
        MODE=update
        echo "==> $CANDIDATE_PART already carries the BOOTDISK signature - updating files only."
        echo "    (pass --format to erase and repartition instead)"
    else
        MODE=format
    fi
fi

if [[ "$MODE" == "update" ]]; then
    echo "About to UPDATE $DEV (${GB}GB, model: $(cat "/sys/block/$BASE/device/model" 2>/dev/null | xargs)) - files only, no reformat"
    lsblk -o NAME,SIZE,FSTYPE,LABEL,MOUNTPOINT "$DEV"
    echo
    read -rp "Type UPDATE to continue: " CONFIRM
    [[ "$CONFIRM" == "UPDATE" ]] || { echo "aborted"; exit 1; }
else
    echo "About to ERASE $DEV (${GB}GB, model: $(cat "/sys/block/$BASE/device/model" 2>/dev/null | xargs))"
    lsblk -o NAME,SIZE,FSTYPE,LABEL,MOUNTPOINT "$DEV"
    echo
    read -rp "Type ERASE to continue: " CONFIRM
    [[ "$CONFIRM" == "ERASE" ]] || { echo "aborted"; exit 1; }
fi

# ---- unmount anything on the card ---------------------------------------
for part in $(lsblk -lno NAME "$DEV" | tail -n +2); do
    umount "/dev/$part" 2>/dev/null || true
done

if [[ "$MODE" == "update" ]]; then
    # ---- reuse the existing partition, already formatted and signed --------
    PART="$CANDIDATE_PART"
    [[ -b "$PART" ]] || { echo "error: $PART does not exist - card has not been set up yet, run without --update first" >&2; exit 1; }

    if [[ "$ALREADY_SET_UP" -ne 1 ]]; then
        echo "error: $PART has no BOOTDISK signature - it wasn't set up by this script." >&2
        echo "       Run without --update (or with --format) to partition and format it first." >&2
        exit 1
    fi
else
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
    # Volume label: RWD_<model>, derived from the source directory name.
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
    LABEL_SRC="$(basename "$SRC")"
    if [[ "$LABEL_SRC" == "card" ]]; then
        # cameras/<cam>/card -> use the camera folder above it
        LABEL_SRC="$(basename "$(dirname "$SRC")")"
    fi
    LABEL="RWD_$(echo "$LABEL_SRC" | sed 's/^card-//' | tr '[:lower:]' '[:upper:]')"
    LABEL="${LABEL:0:11}"
    if [[ "${LABEL^^}" == "CHDK" ]]; then
        echo "error: refusing to label the volume CHDK - it collides with the A/CHDK directory" >&2
        exit 1
    fi
    echo "==> volume label: $LABEL"
    mkfs.fat -F 16 -s "$SPC" -n "$LABEL" "$PART" >/dev/null

    # ---- boot signature: "BOOTDISK" at offset 0x40 ---------------------------
    # Must come AFTER mkfs, which rewrites the boot sector.
    echo "==> writing BOOTDISK signature at 0x40"
    printf 'BOOTDISK' | dd of="$PART" bs=1 seek=64 conv=notrunc status=none
    sync
fi

# ---- copy CHDK ------------------------------------------------------------
MNT=$(mktemp -d)
mount "$PART" "$MNT"
echo "==> copying CHDK from $SRC"
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
    echo "Now slide the card's physical LOCK switch to locked before putting it in the camera."
else
    echo "WARNING - boot signature not found, got: '$SIG'"
    exit 1
fi
