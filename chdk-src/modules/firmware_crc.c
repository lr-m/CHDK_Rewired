#include "camera_info.h"
#include "gui.h"
#include "gui_draw.h"
#include "meminfo.h"
#include "module_load.h"
#include "simple_module.h"
#include "clock.h"
#include "console.h"
#include "conf.h"
#include "lang.h"
#include "firmware_crc_types.h"

// =========  MODULE INIT =================

static int running = 0;

//-------------------------------------------------------------------
// From public domain crc32 code
//-------------------------------------------------------------------

unsigned long crc32_for_byte(unsigned long r) {
    unsigned long j;
    for (j = 0; j < 8; j++) {
        r = (r & 1? 0: (unsigned long)0xEDB88320) ^ r >> 1;
    }
    return r ^ (unsigned long)0xFF000000;
}

void crc32(const void *data, unsigned long n_bytes, unsigned long* crc) {
    static unsigned long ct[0x100] = {0};
    unsigned long i;
    if (!ct[0]) {
        for (i = 0; i < 0x100; i++) {
            ct[i] = crc32_for_byte(i);
        }
    }
    for (i = 0; i < n_bytes; i++) {
        *crc = ct[(unsigned char)*crc ^ ((unsigned char*)data)[i]] ^ *crc >> 8;
    }
}

/*************** GUI MODULE *******************/

#include "gui_lang.h"
#include "gui_mbox.h"
#include "gui_fselect.h"
#include "keyboard.h"

void gui_fwc_draw();

gui_handler GUI_MODE_FWC = 
    /*GUI_MODE_FWC*/  { GUI_MODE_MODULE, gui_fwc_draw, 0 /* only keys are in dialogs */, 0 /* no special handling for menu */, 0, 0 };

enum {
    FWC_STATE_INIT,
    FWC_STATE_STARTED,
    FWC_STATE_CHECK,
    FWC_STATE_FAIL,
    FWC_STATE_DIALOG,
    FWC_STATE_DUMP,
    FWC_STATE_DUMP_OK,
    FWC_STATE_DUMP_FAIL,
    FWC_STATE_DONE,
    FWC_STATE_QUIT,
    FWC_STATE_END,
}; 

int fwc_state = FWC_STATE_INIT;

extern const firmware_crc_desc_t firmware_crc_desc;

static char out[512];

int dump_rom(void) {
    // match dumper script that normally skipts last word to avoid potential wraparound issues
    unsigned dump_size = 0xfffffffc - camera_info.rombaseaddr;
    // so far, no ROM bigger that 32MB
    if(dump_size > 0x1fffffc) {
        dump_size = 0x1fffffc;
    }
    FILE *fh = fopen("A/PRIMARY.BIN","wb");
    if(!fh) {
        return 0;
    }
    fwrite((char *)camera_info.rombaseaddr,dump_size,1,fh);
    fclose(fh);
    return 1;
}

static void gui_fwc_done(__attribute__ ((unused))unsigned unused)
{
    fwc_state = FWC_STATE_DONE;
}

static void gui_fwc_dump_dialog(unsigned btn)
{
    if (btn == MBOX_BTN_OK) {
        fwc_state = FWC_STATE_DUMP;
    } else {
        fwc_state = FWC_STATE_DONE;
    }
}

int crc_core(const firmware_crc_desc_t *desc)
{
    running = 1;

    int failed = 0;
    char *s = out;
    unsigned i;
    for(i=0; i < desc->sub_count; i++) {
        if(!strcmp(desc->firmware_ver_ptr,desc->subs[i].canon_sub)) {
            draw_progress_bar("ROM CRC",0);
            s += sprintf(out,"%s\n%s %s\n%s\n",
                    lang_str(LANG_FIRMWARE_CRC_FAIL),
                    camera_info.platform, desc->subs[i].canon_sub,lang_str(LANG_FIRMWARE_CRC_FAILED_CHUNKS));
            unsigned b;
            const firmware_crc_block_t *blocks = desc->subs[i].blocks;
            for(b=0; b < desc->block_count; b++) {
                draw_progress_bar("ROM CRC",100*(b+1)/desc->block_count);
                // truncated dump could have zero sized blocks
                if(blocks[b].size != 0) {
                    unsigned long fw_crc = 0;
                    crc32(blocks[b].start,blocks[b].size,&fw_crc);
                    if(fw_crc != blocks[b].crc) {
                        s += sprintf(s,"%8x ",blocks[b].start);
                        failed++;
                    }
                }
            }
            sprintf(s,"\n%s",lang_str(LANG_FIRMWARE_CRC_DUMP_ROM));
            break;
        }
    }
    if(failed) {
        fwc_state = FWC_STATE_FAIL;
    } else {
        fwc_state = FWC_STATE_DONE;
    }
    return 0;
}

void gui_fwc_draw()
{
    switch(fwc_state) {
        case FWC_STATE_STARTED:
            fwc_state = FWC_STATE_CHECK;
            crc_core(&firmware_crc_desc);
            break;
        case FWC_STATE_FAIL:
            fwc_state = FWC_STATE_DIALOG;
            gui_mbox_init(LANG_WARNING, (int)out, MBOX_BTN_OK|MBOX_BTN_CANCEL|MBOX_FUNC_RESTORE, gui_fwc_dump_dialog );
            break;
        case FWC_STATE_DUMP:
            draw_progress_bar("Write PRIMARY.BIN",10); // LIES!
            if(dump_rom()) {
                fwc_state = FWC_STATE_DUMP_OK;
            } else {
                fwc_state = FWC_STATE_DUMP_FAIL;
            }
            draw_progress_bar("Write PRIMARY.BIN",100);
            break;
        case FWC_STATE_DUMP_OK:
            fwc_state = FWC_STATE_DIALOG;
            gui_mbox_init(LANG_INFORMATION,(int)"Wrote PRIMARY.BIN", MBOX_BTN_OK|MBOX_TEXT_CENTER|MBOX_FUNC_RESTORE, gui_fwc_done);
            break;
        case FWC_STATE_DUMP_FAIL:
            fwc_state = FWC_STATE_DIALOG;
            gui_mbox_init(LANG_ERROR, (int)"Failed to write PRIMARY.BIN", MBOX_BTN_OK|MBOX_TEXT_CENTER|MBOX_FUNC_RESTORE, gui_fwc_done);
            break;
        case FWC_STATE_DONE:
            // if set to only on boot, clear
            if(conf.check_firmware_crc == 1) {
                conf.check_firmware_crc = 0;
                conf_save();
            }
            // force one spytask iteration delay
            fwc_state = FWC_STATE_QUIT;
            break;
        case FWC_STATE_QUIT:
            fwc_state = FWC_STATE_END;
            // exiting alt will clear gui mode, meaning draw will no longer be called, exit_alt handler will terminate module
            exit_alt();
    }
}


//-------------------------------------------------------------------

/***************** BEGIN OF AUXILARY PART *********************
  ATTENTION: DO NOT REMOVE OR CHANGE SIGNATURES IN THIS SECTION
 **************************************************************/

int _run()
{
    // PROBLEM: we need to be in alt mode for keyboard handling and dialogs to work,
    // but entering alt changes gui and requires getting into the main spytask loop
    // solution: abuse the _module_can_unload handler, which is called periodically from the main loop
    running = 1;
    enter_alt(0);
    return 0;
}

int _module_can_unload()
{
    if(fwc_state == FWC_STATE_INIT) {
        gui_set_mode(&GUI_MODE_FWC);
        fwc_state = FWC_STATE_STARTED;
    }
    return (running==0);
}

int _module_exit_alt()
{
    running = 0;
    return 0;
}

/******************** Module Information structure ******************/

libsimple_sym _libfirmwarecrc =
{
    {
         0, 0, _module_can_unload, _module_exit_alt, _run
    },
};

ModuleInfo _module_info =
{
    MODULEINFO_V1_MAGICNUM,
    sizeof(ModuleInfo),
    {2,0},      // Module version

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,           // Requirements of CHDK version
    ANY_PLATFORM_ALLOWED,       // Specify platform dependency

    (int32_t)"Checksum ROM",
    MTYPE_TOOL,

    &_libfirmwarecrc.base,

    CONF_VERSION,               // CONF version
    CAM_SCREEN_VERSION,         // CAM SCREEN version
    ANY_VERSION,                // CAM SENSOR version
    ANY_VERSION,                // CAM INFO version

    0,
};

/*************** END OF AUXILARY PART *******************/

