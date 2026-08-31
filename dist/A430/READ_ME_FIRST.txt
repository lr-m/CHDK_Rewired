CHDK Rewired for the PowerShot A430  (firmware 1.00B)
=====================================================

CHDK Rewired is a custom build of CHDK. It adds a circuit-bending mode to
the image pipeline, a persistent overlay, multiple exposure and a Game Boy
player, on top of everything stock CHDK does.

Nothing here can damage the camera. It all runs in memory and never
touches the camera's firmware - take the card out and it's stock again.
If it ever freezes, pull the battery.


1. USE A 2GB OR SMALLER SD CARD
   Anything bigger is SDHC and this camera cannot read it at all.

2. CHECK THE FIRMWARE VERSION
   Copy vers.req and ver.req (both in this folder) onto the card.
   Camera on in PLAY mode. Hold FUNC/SET, then press DISP.

   It must say 1.00B. Each build is compiled against one exact firmware
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
       Windows:  .\flash-card-windows.ps1 -DiskNumber N
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

   If you would rather not repartition a card: unzip CHDK-A430-100b-card.zip
   onto a card the camera formatted, then PLAY mode -> MENU -> scroll to
   the bottom -> "Firm Update...". Same result, but it has to be done
   again after every power-off.

5. USING IT
   PRINT/SHARE (the printer icon) toggles "<ALT>" mode.
   In <ALT>: MENU opens CHDK's own menu.
   Bend mode is on the shooting screen - FUNC/SET opens the patchbay.

   Presets are saved to CHDK/BENDS on the card, and every photo gets a
   sidecar file recording what it was bent with.

   The manual in the repository covers all of it.


Build: a430 100b, Mon, 31 Aug 2026 13:31:54 +0100
DISKBOOT.BIN md5 d3cd158f157a3a64501ab9ee18dfb72d

Frozen camera: pull the battery, no harm done. Nothing here is written
to the camera itself, so taking the card out always gets you a stock
camera back.
