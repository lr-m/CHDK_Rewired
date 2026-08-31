#ifndef PLATFORM_KBD_H
#define PLATFORM_KBD_H
// plaform specific keyboard defines, see core/kbd_common.c for documentation

#define KEYS_MASK0 (0x00000000)
// KEYS_MASK1 is a variable, needed by the extra button hack
#define KEYS_MASK1 keys_mask1
#define KEYS_MASK2 (0x153F)

#define SD_READONLY_FLAG (0x20000)
#define SD_READONLY_IDX 2

#define USB_MASK (0x40000)
#define USB_IDX 2

extern int keys_mask1;

// non-standard key state update
#define KBD_CUSTOM_UPDATE_KEY_STATE 1
#endif
