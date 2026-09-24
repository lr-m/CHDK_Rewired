<#
.SYNOPSIS
    Flash an SD card with CHDK for the PowerShot A430 (firmware 100b), on Windows.

.DESCRIPTION
    This is a release script: it lives beside CHDK-A430-100b-card.zip and, with no -Source
    given, flashes exactly that zip. It ERASES the card.

    Makes a FAT16 partition with the string "BOOTDISK" at offset 0x40 of its
    boot sector, which is what these pre-2011 PowerShots look for.

    Two layouts, picked from the card size:

      single  one FAT16 partition holding everything. Used whenever the card
              fits FAT16 with clusters of 32KiB or less - roughly 2GB and under.

      split   a 16MiB FAT16 boot partition holding only DISKBOOT.BIN, then the
              rest of the card as FAT32 holding the whole CHDK tree. Only for
              bodies whose port mounts a FAT32 partition in preference to the
              first one (CAM_MULTIPART plus CHDK's FAT32 autodetect): the boot
              ROM loads DISKBOOT.BIN from the FAT16 side, then the firmware
              mounts the FAT32 side as A/, so CHDK and its modules live there.
              Needs Windows 10 1703 or later, which is what first allowed more
              than one partition on removable media.

    Run from an ADMINISTRATOR PowerShell prompt. Find the disk number with
    Get-Disk (or `diskpart` -> `list disk`) and be certain which one is the
    card: this erases it completely.

    Windows refuses to run downloaded scripts by default (PSSecurityException),
    so start it with -ExecutionPolicy Bypass, which applies to this run only.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\flash-card-windows.ps1 -DiskNumber 2
#>

#Requires -RunAsAdministrator

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][int]$DiskNumber,
    [string]$Source
)

$ErrorActionPreference = 'Stop'

$model   = 'a430'

# Whether this body can use the split layout: tested, untested, or no.
$largeCard = 'no'

# 32GiB. Above this is SDXC, which every body here predates - and Format-Volume
# will not make FAT32 that large anyway.
$maxSplitBytes = 34359738368
$here    = Split-Path -Parent $MyInvocation.MyCommand.Path
$cardZip = Join-Path $here 'CHDK-A430-100b-card.zip'

# ---- source ---------------------------------------------------------------
# Default: the card zip shipped in this folder, expanded to a temp directory
# that is removed at the end.
$tempSource = $null
if (-not $Source) {
    if (-not (Test-Path $cardZip)) { throw "CHDK-A430-100b-card.zip is missing from $here" }
    $Source = $cardZip
}
$Source = (Resolve-Path $Source).Path
if ($Source -like '*.zip') {
    $tempSource = Join-Path $env:TEMP ("chdkcard-" + [System.Guid]::NewGuid().ToString("N"))
    Write-Host "==> unpacking $(Split-Path $Source -Leaf)"
    Expand-Archive -LiteralPath $Source -DestinationPath $tempSource -Force
    $Source = $tempSource
}
if (-not (Test-Path (Join-Path $Source 'DISKBOOT.BIN'))) {
    throw "DISKBOOT.BIN not found in $Source - wrong source?"
}

try {

# ---- vet the disk ---------------------------------------------------------
$disk = Get-Disk -Number $DiskNumber
if ($disk.IsBoot -or $disk.IsSystem) { throw "Disk $DiskNumber is the system/boot disk. Refusing." }
if ($disk.BusType -notin @('USB', 'SD', 'MMC')) {
    # Some built-in card readers report SCSI or RAID rather than SD. That is
    # normal, but so is an internal hard disk, so make the user say it out loud.
    Write-Warning "Disk $DiskNumber reports bus type '$($disk.BusType)', which is not USB/SD/MMC."
    Write-Warning "That can be a built-in card reader - or it can be a hard disk. Check Get-Disk carefully."
    $ack = Read-Host "Type the bus type '$($disk.BusType)' to confirm this really is the card"
    if ($ack -ne $disk.BusType) { Write-Host "aborted"; exit 1 }
}

$bytes = $disk.Size
$gb    = [math]::Round($bytes / 1GB, 1)

# ---- cluster size and layout ----------------------------------------------
# FAT16 caps at 65524 clusters. Pick the smallest cluster that fits under that,
# and never more than 32KiB: 64KiB clusters are a FAT16 extension these old boot
# ROMs reject, and the camera then will not power on from the card at all. A
# card too big for that gets the split layout if this body supports it, and is
# refused otherwise.
$partSectors = [math]::Floor(($bytes - 1MB) / 512)
$spc = 0
foreach ($try in 4, 8, 16, 32, 64) {
    if (($partSectors / $try) -lt 65524) { $spc = $try; break }
}
$layout = 'single'
if ($spc -eq 0) {
    if ($largeCard -eq 'no') {
        throw "A $gb GB card needs clusters larger than 32KiB to fit FAT16, which these cameras reject. Use a 2GB card or smaller."
    }
    if ($bytes -gt $maxSplitBytes) {
        throw "A $gb GB card is SDXC, which the A430 predates. Use a card of 32GB or less."
    }
    $layout = 'split'
    $spc = 4   # boot partition: 16MiB / 2KiB = 8192 clusters, FAT16 not FAT12
}
$allocUnit = $spc * 512

# ---- volume label ---------------------------------------------------------
# It must NOT be "CHDK". On FAT the volume label is an entry in the root
# directory, so a card labelled CHDK has two root entries spelled CHDK - the
# label and the CHDK directory itself. Some of these cameras' FAT drivers match
# the label first, and then every module load and the config file fail silently
# while the card looks perfect in Explorer.
$label = 'RWD_' + $model.ToUpper()
if ($label.Length -gt 11) { $label = $label.Substring(0, 11) }

# ---- confirm --------------------------------------------------------------
Write-Host ""
Write-Host "About to ERASE disk $DiskNumber : $($disk.FriendlyName), $gb GB, bus $($disk.BusType)"
Get-Disk -Number $DiskNumber | Format-Table Number, FriendlyName, Size, PartitionStyle -AutoSize
Write-Host "  source:       $Source"
Write-Host "  volume label: $label"
if ($layout -eq 'split') {
    Write-Host "  layout:       16MiB FAT16 boot partition + FAT32 for the rest"
    if ($largeCard -eq 'untested') {
        Write-Host ""
        Write-Warning "This two-partition layout has been tested on the A480, not yet on the A430."
        Write-Warning "If the camera does not see the card, or CHDK reports missing modules, use a 2GB card instead - and please say so."
    }
} else {
    Write-Host "  layout:       one FAT16 partition, $($allocUnit / 1024) KiB clusters"
}
Write-Host ""
$confirm = Read-Host "Type ERASE to continue"
if ($confirm -cne 'ERASE') { Write-Host "aborted"; exit 1 }

# ---- partition and format -------------------------------------------------
Write-Host "==> clearing disk"
Clear-Disk -Number $DiskNumber -RemoveData -RemoveOEM -Confirm:$false -ErrorAction SilentlyContinue
Initialize-Disk -Number $DiskNumber -PartitionStyle MBR -ErrorAction SilentlyContinue | Out-Null
Set-Disk -Number $DiskNumber -PartitionStyle MBR -ErrorAction SilentlyContinue

# The drive letter is assigned asynchronously, so the object New-Partition
# returned may not carry it yet. Re-read until it does.
function Wait-DriveLetter([int]$partNumber) {
    foreach ($attempt in 1..15) {
        Start-Sleep -Seconds 1
        $p = Get-Partition -DiskNumber $DiskNumber -PartitionNumber $partNumber
        if ($p.DriveLetter -and $p.DriveLetter -ne [char]0) { return $p.DriveLetter }
    }
    throw "Windows did not assign a drive letter to partition $partNumber."
}

Write-Host "==> creating FAT16 boot partition"
if ($layout -eq 'split') {
    $part = New-Partition -DiskNumber $DiskNumber -Size 16MB -IsActive -AssignDriveLetter
    $dataPart = New-Partition -DiskNumber $DiskNumber -UseMaximumSize -AssignDriveLetter
} else {
    $part = New-Partition -DiskNumber $DiskNumber -UseMaximumSize -IsActive -AssignDriveLetter
}
$partNumber = $part.PartitionNumber
$drive = Wait-DriveLetter $partNumber

$bootLabel = if ($layout -eq 'split') { 'RWD_BOOT' } else { $label }
Write-Host "==> formatting FAT16 ($($allocUnit / 1024) KiB clusters), label $bootLabel"
Format-Volume -DriveLetter $drive -FileSystem FAT -NewFileSystemLabel $bootLabel `
              -AllocationUnitSize $allocUnit -Force -Confirm:$false | Out-Null

$dataDrive = $null
if ($layout -eq 'split') {
    $dataDrive = Wait-DriveLetter $dataPart.PartitionNumber
    Write-Host "==> formatting FAT32 data partition, label $label"
    Format-Volume -DriveLetter $dataDrive -FileSystem FAT32 -NewFileSystemLabel $label `
                  -Force -Confirm:$false | Out-Null
}
Start-Sleep -Seconds 2

# ---- boot signature: "BOOTDISK" at offset 0x40 of the partition -----------
# Must come AFTER the format, which rewrites the boot sector. 0x40 lands in the
# VBR's boot-code area, past the BPB, so overwriting it costs nothing.
#
# Windows refuses writes to a mounted volume's own sectors unless the volume is
# locked first, so this takes an exclusive lock via FSCTL_LOCK_VOLUME, patches
# the sector, and releases it by closing the handle.
Write-Host "==> writing BOOTDISK signature at 0x40"

if (-not ('ChdkRawVolume' -as [type])) {
Add-Type -Language CSharp @'
using System;
using System.IO;
using System.Text;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

public static class ChdkRawVolume
{
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern SafeFileHandle CreateFileW(string name, uint access, uint share,
        IntPtr sec, uint creation, uint flags, IntPtr template);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool DeviceIoControl(SafeFileHandle h, uint code,
        IntPtr inBuf, uint inSize, IntPtr outBuf, uint outSize, out uint returned, IntPtr ov);

    const uint GENERIC_READ = 0x80000000, GENERIC_WRITE = 0x40000000;
    const uint FILE_SHARE_READ = 1, FILE_SHARE_WRITE = 2;
    const uint OPEN_EXISTING = 3;
    const uint FSCTL_LOCK_VOLUME = 0x00090018;

    // Sets the type byte of the MBR entry starting at startLba, on the raw disk,
    // and reads it back. Sector 0 belongs to no volume, so Windows allows this
    // with the volumes still mounted. Returns the type that was there before.
    public static byte SetMbrType(int diskNumber, int sectorSize, long startLba, byte type)
    {
        string path = @"\\.\PhysicalDrive" + diskNumber;
        SafeFileHandle h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, IntPtr.Zero, OPEN_EXISTING, 0, IntPtr.Zero);
        if (h.IsInvalid)
            throw new IOException("cannot open " + path + " (error " + Marshal.GetLastWin32Error() + ")");

        using (FileStream fs = new FileStream(h, FileAccess.ReadWrite, sectorSize))
        {
            byte[] mbr = new byte[sectorSize];
            fs.Seek(0, SeekOrigin.Begin);
            if (fs.Read(mbr, 0, sectorSize) != sectorSize) throw new IOException("short read of MBR");
            if (mbr[510] != 0x55 || mbr[511] != 0xAA) throw new IOException("sector 0 is not an MBR");

            int entry = -1;
            for (int i = 0; i < 4; i++)
                if (BitConverter.ToUInt32(mbr, 0x1BE + 16 * i + 8) == startLba) entry = 0x1BE + 16 * i;
            if (entry < 0) throw new IOException("no MBR entry starts at LBA " + startLba);

            byte before = mbr[entry + 4];
            mbr[entry + 4] = type;
            fs.Seek(0, SeekOrigin.Begin);
            fs.Write(mbr, 0, sectorSize);
            fs.Flush();

            byte[] check = new byte[sectorSize];
            fs.Seek(0, SeekOrigin.Begin);
            fs.Read(check, 0, sectorSize);
            if (check[entry + 4] != type)
                throw new IOException(String.Format("MBR type reads back 0x{0:X2} after writing 0x{1:X2}", check[entry + 4], type));
            return before;
        }
    }

    public static void WriteBootdisk(char driveLetter)
    {
        string path = @"\\.\" + driveLetter + ":";
        SafeFileHandle h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, IntPtr.Zero, OPEN_EXISTING, 0, IntPtr.Zero);
        if (h.IsInvalid)
            throw new IOException("cannot open " + path + " (error " + Marshal.GetLastWin32Error() + ")");

        uint ret;
        if (!DeviceIoControl(h, FSCTL_LOCK_VOLUME, IntPtr.Zero, 0, IntPtr.Zero, 0, out ret, IntPtr.Zero))
        {
            int err = Marshal.GetLastWin32Error();
            h.Dispose();
            throw new IOException("cannot lock volume " + driveLetter +
                ": (error " + err + "). Close Explorer windows and any program using the card, then retry.");
        }

        // FileStream takes ownership of the handle; closing it releases the lock.
        using (FileStream fs = new FileStream(h, FileAccess.ReadWrite, 512))
        {
            byte[] sector = new byte[512];
            fs.Seek(0, SeekOrigin.Begin);
            if (fs.Read(sector, 0, 512) != 512) throw new IOException("short read of boot sector");

            byte[] sig = Encoding.ASCII.GetBytes("BOOTDISK");
            Array.Copy(sig, 0, sector, 0x40, 8);

            fs.Seek(0, SeekOrigin.Begin);
            fs.Write(sector, 0, 512);
            fs.Flush();

            byte[] check = new byte[512];
            fs.Seek(0, SeekOrigin.Begin);
            fs.Read(check, 0, 512);
            if (Encoding.ASCII.GetString(check, 0x40, 8) != "BOOTDISK")
                throw new IOException("boot signature did not verify after writing");
        }
    }
}
'@
}

$written = $false
foreach ($attempt in 1..10) {
    try {
        [ChdkRawVolume]::WriteBootdisk([char]$drive)
        $written = $true
        break
    } catch {
        if ($attempt -eq 10) { throw }
        Write-Host "    volume busy, retrying ($attempt/10)..."
        Start-Sleep -Seconds 2
    }
}
if (-not $written) { throw "could not write the boot signature" }

# ---- copy CHDK ------------------------------------------------------------
# Split: the boot ROM needs only DISKBOOT.BIN on the FAT16 side; everything,
# DISKBOOT.BIN included, goes on the FAT32 side the firmware actually mounts.
Start-Sleep -Seconds 2
$target = $drive
if ($layout -eq 'split') {
    Write-Host "==> copying DISKBOOT.BIN to the boot partition"
    Copy-Item -Path (Join-Path $Source 'DISKBOOT.BIN') -Destination "${drive}:\" -Force
    Write-VolumeCache -DriveLetter $drive -ErrorAction SilentlyContinue
    $target = $dataDrive
}
Write-Host "==> copying CHDK"
Copy-Item -Path (Join-Path $Source '*') -Destination "${target}:\" -Recurse -Force
Write-VolumeCache -DriveLetter $target -ErrorAction SilentlyContinue

# ---- MBR partition types --------------------------------------------------
# The boot partition must be type 0x06 (FAT16). Windows makes it 0x0E (FAT16
# LBA), and the A410's boot ROM answers a 0x0E card with "Memory card error"
# even though the filesystem and BOOTDISK signature are perfect. The split
# layout's data partition must be 0x0C (FAT32 LBA), the type CHDK's FAT32
# autodetect looks for.
#
# Set-Partition -MbrType did not stick on a real card: the card still came out
# 0x0E. So the byte is written straight into the MBR and read back from the
# disk, and this is the last write the script makes, so nothing after it can
# change the type again.
$sectorSize = [int]$disk.LogicalSectorSize
if ($sectorSize -le 0) { $sectorSize = 512 }
function Set-MbrType($p, [byte]$type) {
    $lba = [long]($p.Offset / $sectorSize)
    $before = [ChdkRawVolume]::SetMbrType($DiskNumber, $sectorSize, $lba, $type)
    Write-Host ("    partition {0}: MBR type 0x{1:X2} -> 0x{2:X2} (verified on disk)" -f $p.PartitionNumber, $before, $type)
}
Write-Host "==> writing MBR partition types"
Set-MbrType $part 0x06
if ($layout -eq 'split') { Set-MbrType $dataPart 0x0C }
Update-Disk -Number $DiskNumber -ErrorAction SilentlyContinue
Get-Partition -DiskNumber $DiskNumber | ForEach-Object {
    Write-Host ("    Windows now reports partition {0} as MBR type 0x{1:X2}" -f $_.PartitionNumber, [int]$_.MbrType)
}

Write-Host ""
Write-Host "Card contents:"
Get-ChildItem "${target}:\" | Select-Object -ExpandProperty Name
Write-Host ""
Write-Host "OK - card is bootable (BOOTDISK signature verified)." -ForegroundColor Green
Write-Host ""
Write-Host "Eject the card with Safely Remove Hardware, then slide its physical LOCK"
Write-Host "switch to locked before putting it in the camera. The camera only looks for"
Write-Host "DISKBOOT.BIN on a locked card; it can still write photos, because the"
Write-Host "firmware ignores the switch once CHDK is running."

}
finally {
    if ($tempSource -and (Test-Path $tempSource)) { Remove-Item -Recurse -Force $tempSource }
}
