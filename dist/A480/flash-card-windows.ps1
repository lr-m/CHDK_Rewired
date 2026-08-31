<#
.SYNOPSIS
    Flash an SD card with CHDK for the PowerShot A480 (firmware 100b), on Windows.

.DESCRIPTION
    This is a release script: it lives beside CHDK-A480-100b-card.zip and, with no -Source
    given, flashes exactly that zip. It ERASES the card.

    Makes ONE FAT16 partition with the string "BOOTDISK" at offset 0x40 of its
    boot sector, which is what these pre-2011 PowerShots look for.

    Run from an ADMINISTRATOR PowerShell prompt. Find the disk number with
    Get-Disk (or `diskpart` -> `list disk`) and be certain which one is the
    card: this erases it completely.

.EXAMPLE
    .\flash-card-windows.ps1 -DiskNumber 2
#>

#Requires -RunAsAdministrator

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][int]$DiskNumber,
    [string]$Source
)

$ErrorActionPreference = 'Stop'

$model   = 'a480'
$here    = Split-Path -Parent $MyInvocation.MyCommand.Path
$cardZip = Join-Path $here 'CHDK-A480-100b-card.zip'

# ---- source ---------------------------------------------------------------
# Default: the card zip shipped in this folder, expanded to a temp directory
# that is removed at the end.
$tempSource = $null
if (-not $Source) {
    if (-not (Test-Path $cardZip)) { throw "CHDK-A480-100b-card.zip is missing from $here" }
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

# ---- cluster size ---------------------------------------------------------
# FAT16 caps at 65524 clusters. Pick the smallest cluster that fits under that,
# and refuse anything needing more than 32KiB: 64KiB clusters are a FAT16
# extension these old boot ROMs reject, and the camera then will not power on
# from the card at all.
$partSectors = [math]::Floor(($bytes - 1MB) / 512)
$spc = 0
foreach ($try in 4, 8, 16, 32, 64) {
    if (($partSectors / $try) -lt 65524) { $spc = $try; break }
}
if ($spc -eq 0 -or $spc -gt 64) {
    throw "A $gb GB card needs clusters larger than 32KiB to fit FAT16, which these cameras reject. Use a 2GB card or smaller."
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
Write-Host "  cluster size: $($allocUnit / 1024) KiB"
Write-Host ""
$confirm = Read-Host "Type ERASE to continue"
if ($confirm -cne 'ERASE') { Write-Host "aborted"; exit 1 }

# ---- partition and format -------------------------------------------------
Write-Host "==> clearing disk"
Clear-Disk -Number $DiskNumber -RemoveData -RemoveOEM -Confirm:$false -ErrorAction SilentlyContinue
Initialize-Disk -Number $DiskNumber -PartitionStyle MBR -ErrorAction SilentlyContinue | Out-Null
Set-Disk -Number $DiskNumber -PartitionStyle MBR -ErrorAction SilentlyContinue

Write-Host "==> creating FAT16 partition"
$part = New-Partition -DiskNumber $DiskNumber -UseMaximumSize -IsActive -AssignDriveLetter
$partNumber = $part.PartitionNumber

# The drive letter is assigned asynchronously, so the object New-Partition
# returned may not carry it yet. Re-read until it does.
$drive = $null
foreach ($attempt in 1..15) {
    Start-Sleep -Seconds 1
    $part = Get-Partition -DiskNumber $DiskNumber -PartitionNumber $partNumber
    if ($part.DriveLetter -and $part.DriveLetter -ne [char]0) { $drive = $part.DriveLetter; break }
}
if (-not $drive) { throw "Windows did not assign a drive letter to the new partition." }

# MBR partition type 0x06 = FAT16. New-Partition guesses from the size; set it
# explicitly so the result does not depend on the Windows version.
try { Set-Partition -DiskNumber $DiskNumber -PartitionNumber $partNumber -MbrType FAT16 } catch {}

Write-Host "==> formatting FAT16 ($($allocUnit / 1024) KiB clusters), label $label"
Format-Volume -DriveLetter $drive -FileSystem FAT -NewFileSystemLabel $label `
              -AllocationUnitSize $allocUnit -Force -Confirm:$false | Out-Null
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
Write-Host "==> copying CHDK"
Start-Sleep -Seconds 2
Copy-Item -Path (Join-Path $Source '*') -Destination "${drive}:\" -Recurse -Force
Write-VolumeCache -DriveLetter $drive -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "Card contents:"
Get-ChildItem "${drive}:\" | Select-Object -ExpandProperty Name
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
