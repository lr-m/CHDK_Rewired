/* This file should be used in loader for boot compatibility checks */

#ifndef OPT_DISABLE_COMPAT_CHECK

typedef struct {
    short *fw_pid;
    short pid;
} pid_sig_t;

typedef struct {
    const char *fw_str;
    const char *str;
} ver_sig_t;

/*
Any changes to the following struct have to be reflected in the tools/compatbuilder.sh script
*/
typedef struct {
    unsigned short pid;     // camera P-ID
    short method;           // LED control method (BLINK_LED_CONTROL= in platform/(cam)/makefile.inc)
    unsigned int led;       // LED MMIO address (BLINK_LED_GPIO= in platform/(cam)/makefile.inc)
    unsigned int addr;      // location of P-ID
} pid_led_t;

/*
LED control methods
*/
#define LEDCNTRL_UNK  0     // LED control unknown (no blink possible)
#define LEDCNTRL_OLD  1     // DIGIC II...4; 0x46 to on, 0x44 to off
#define LEDCNTRL_NEW1 2     // DIGIC 5; bit5, inverted
#define LEDCNTRL_NEW2 3     // DIGIC 4+, 5; 0x93d800 to on, 0x83dc00 to off
#define LEDCNTRL_NEW3 4     // DIGIC 6; 0x4d0002 to on, 0x4c0003 to off
#define LEDCNTRL_NEW4 5     // DIGIC 7; 0x024d0002 to on, 0x024c0003 to off

#ifndef NEED_ENCODED_DISKBOOT
#define NEED_ENCODED_DISKBOOT 0
#endif

// following must be ordered by P-ID, order to be maintained inside NEED_ENCODED_DISKBOOT blocks only
pid_led_t pid_leds[]={
// insert generated data
#include "compat_table.h"
};

// compatibility definitions from platform/sub
#include "bin_compat.h"

#ifndef DEBUG_DELAY
    #define DEBUG_DELAY 10000000
#endif

void set_led(int led, int state, int method) {
    volatile long *p = (void*)led;

    if (method == LEDCNTRL_NEW1) {
        // DIGIC 5
        *p = (*p & 0xFFFFFFCF) | ((state) ? 0x00 : 0x20);
    }
    else if (method == LEDCNTRL_NEW2) {
        // DIGIC 4+, 5
        *p = ((state) ? 0x93d800 : 0x83dc00);
    }
    else if (method == LEDCNTRL_NEW3) {
        // DIGIC 6
        *p = ((state) ? 0x4d0002 : 0x4c0003);
    }
    else if (method == LEDCNTRL_NEW4) {
        // DIGIC 7
        *p = ((state) ? 0x024d0002 : 0x024c0003);
    }
    else if (method == LEDCNTRL_OLD) {
        *p = ((state) ? 0x46 : 0x44);
    }
}

const int num_pid_sigs = sizeof(pid_sigs)/sizeof(pid_sig_t);
const int num_ver_sigs = sizeof(ver_sigs)/sizeof(ver_sig_t);
const int led_addresses = sizeof(pid_leds)/sizeof(pid_led_t);

unsigned short get_pid(void) {
    int i;
    for(i=0;i<led_addresses;i++) {
        if( (*(unsigned short*)pid_leds[i].addr >= pid_leds[0].pid) &&
            (*(unsigned short*)pid_leds[i].addr <= pid_leds[led_addresses-1].pid) ) {
            // return the first possible number which looks like a valid P-ID
            return *(unsigned short*)pid_leds[i].addr;
        }
    }
    return 0;
}

void blink(unsigned short cam_pid) {
    unsigned int led = 0;
    int method = LEDCNTRL_UNK;
    int i;
    // check if there's a known LED
    for(i=0;i<led_addresses;i++) {
        if(cam_pid == pid_leds[i].pid) {
            led = pid_leds[i].led;
            method = pid_leds[i].method;
            break;
        }
    }
    // no known LED, no blink
    if (!led)
        while(1);
    // LED found, blink (control method chosen according to the type of GPIO)
    while(1) {
        set_led(led, 1, method);
        for(i=0;i<DEBUG_DELAY;i++) {
            asm("nop\n nop\n");
        };
        set_led(led, 0, method);
        for(i=0;i<DEBUG_DELAY;i++) {
            asm("nop\n nop\n");
        };
    }
}

// my_ver assumed to be a null terminated string like 1.00A
int ver_cmp(const char *my_ver, const char *fw_ver) {
    int i=0;
    while(my_ver[i] == fw_ver[i] && my_ver[i] != 0)
        i++;
    return my_ver[i] == 0; // hit the null in our string
}

unsigned short check_pid(void) {
    int i;
    // skip check if pid information not available
    if (num_pid_sigs == 0)
        return 0;
    for(i=0;i<num_pid_sigs;i++) {
        if(*pid_sigs[i].fw_pid == pid_sigs[i].pid) {
            return pid_sigs[i].pid;
        }
    }
    i = get_pid();
    blink(i);
    return 0; // to make the compiler happy
}

#endif // OPT_DISABLE_COMPAT_CHECK

void check_compat(void) {
#ifndef OPT_DISABLE_COMPAT_CHECK
    int i;
    unsigned short cam_pid;
    cam_pid = check_pid();
    for(i=0;i<num_ver_sigs;i++) {
        if(ver_cmp(ver_sigs[i].str,ver_sigs[i].fw_str)) {
            return;
       }
    }
    blink(cam_pid);
#endif
}

void core_copy(const long *src, long *dst, long length) {
    if (src < dst && dst < src + length) {
        /* Have to copy backwards */
        src += length;
        dst += length;
        while (length--)  *--dst = *--src;
    }
    else
        while (length--)  *dst++ = *src++;
}

