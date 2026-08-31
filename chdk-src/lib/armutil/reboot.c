#include "stdlib.h"
#include "stddef.h"
#include "stdio.h"
#include "string.h"
#define LOW_LEVEL
#include "lolevel.h"
/*
 shut down the display and reboot the camera. 
 bootfile is the name of the file to boot.
  If the filename ends in .FI*, load with _reboot_fw_update if implemented
  Otherwise must be an unencoded ARM binary, will be loaded at 0x1900
  For cameras which use encoded diskboot, loader/<camera>/main.bin may be used
  For cameras which do not use encoded diskboot, DISKBOOT.BIN may be used
  No sanity checking is performed on the binary, except that the size is >= 4 bytes
 If bootfile is NULL, camera firmware is rebooted. DISKBOOT.BIN will be loaded or not according to normal rules
 returns 0 on failure, does not return on success
 does NOT save camera settings to flash
 does NOT retract lens before rebooting
 calling from playback mode is recommended
 
 DIGIC 6: reboot not supported yet, unsuitable code #ifdef'd out
*/
int reboot(const char *bootfile) {
#ifndef THUMB_FW
    if(bootfile == NULL)
#endif
    {
        _TurnOffDisplay();
        _Restart(0);
    }
#ifndef THUMB_FW
	int namelen=strlen(bootfile);
	if(namelen > 3 && (strncmp(bootfile + namelen - 4,".FI",3) == 0)) {
		_reboot_fw_update(bootfile);
		// if _reboot_fw_update returns, it failed or is not implemented
		return 0;
	}

	int fd;
	int size;
	int rcnt;
	char *buf;
	unsigned data_tcm;
    void __attribute__((noreturn)) (*canon_copy_and_restart)(char *dst, char *src, unsigned length,char *start);

	// use open/read etc since it takes less mem, boot image might be large
	fd = open(bootfile,O_RDONLY,0);
	if(fd == -1) {
		return 0;
	}
	size = lseek(fd,0,SEEK_END);
	// error or less than 1 instruction
	if(size < 4) {
		return 0;
	}
	buf = umalloc(size);
	if(!buf) {
		close(fd);
		return 0;
	}
	lseek(fd,0,SEEK_SET);
	rcnt = read(fd, buf, size);
	close(fd);
	if(rcnt != size) {
		ufree(buf);
		return 0;
	}
	asm volatile(
		"MRC		p15, 0, %0, c9, c1, 0\n"
		: "=r" (data_tcm)
	); 
	data_tcm &= 0xFFFFF000;
	canon_copy_and_restart = (void *)(*(unsigned *)data_tcm);
	if( ((unsigned)canon_copy_and_restart & 0xFFFF0000) != 0xFFFF0000) {
		ufree(buf);
		return 0;
	}
    _TurnOffDisplay();
	_Restart(7);
	canon_copy_and_restart((void *)0x1900,buf,size,(void *)0x1900);
#else
    (void)bootfile;
    return 0;
#endif // THUMB_FW
}


