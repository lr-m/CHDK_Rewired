/*
* Based on work by Dave Hansen <dave@sr71.net>
* original licensing:
* This software may be redistributed and/or modified under the terms of
 * the GNU General Public License ("GPL") version 2 as published by the
 * Free Software Foundation.
 */
#include "gui.h"
#include "string.h"
#include "ctype.h"
#include "gui_lang.h"
#include "gui_draw.h"
#include "gui_mbox.h"
#include "gui_tbox.h"
#include "gui_mpopup.h"
#include "gui_menu.h"
#include "eyefi.h"

//-------------------------------------------------------------------

#define EINVAL              1000
#define EYEFI_BUF_SIZE      16384
#define ESSID_LEN           32

// Standard size for various wifi key types
#define WPA_KEY_BYTES       32
#define WEP_KEY_BYTES       13
#define WEP_40_KEY_BYTES    5

// Status values while testing network
enum test_status
{
    EYEFI_NOT_SCANNING,
    EYEFI_LOCATING_NETWORK,
    EYEFI_VERIFYING_NETWORK_KEY,
    EYEFI_WAITING_FOR_DHCP,
    EYEFI_TESTING_CONN_TO_SERVER,
    EYEFI_SUCCESS
};

/*
 * The 'o' command has several sub-commands:
 */
enum card_info_subcommand
{
    EYEFI_MAC_ADDRESS   = 1,
    EYEFI_FIRMWARE_INFO = 2,
    EYEFI_CARD_KEY      = 3,
    EYEFI_API_URL       = 4,
    EYEFI_UNKNOWN_5     = 5, // Chris says these are
    EYEFI_UNKNOWN_6     = 6, // checksums
    EYEFI_LOG_LEN       = 7,
    EYEFI_WLAN_DISABLE  = 10, // 1=disable 0=enable, write is 1 byte, read is var_byte
    EYEFI_UPLOAD_PENDING= 11, // {0x1, STATE}
    EYEFI_HOTSPOT_ENABLE= 12, // {0x1, STATE}
    EYEFI_CONNECTED_TO  = 13, // Currently connected Wifi network
    EYEFI_UPLOAD_STATUS = 14, // current uploading file info
    EYEFI_UNKNOWN_15    = 15, // always returns {0x01, 0x1d} as far as I've seen
    EYEFI_TRANSFER_MODE = 17,
    EYEFI_ENDLESS       = 27, // 0x1b
    EYEFI_DIRECT_WAIT_FOR_CONNECTION = 0x24, // 0 == "direct mode off"
    EYEFI_DIRECT_WAIT_AFTER_TRANSFER = 0x25, // set to 60 when direct mode off
    EYEFI_UNKNOWN_ff    = 0xff, // The D90 does this, and it looks to
                  // return a 1-byte response length
                  // followed by a number of 8-byte responses
                  // But I've only ever seen a single response
                  // [000]: 01 04 1d 00 18 56 aa d5 42 00 00 00 00 00 00 00
                  // It could be a consolidates info command like "info for
                  // everything" so the camera makes fewer calls.
};

// Wifi key structure sent to card
struct network_key
{
    u8  len;
    u8  key[WPA_KEY_BYTES];     // Large enough for longest key
} __attribute__((packed));

// Structures that map to data sent and received from card

// network info returned from scan
typedef struct
{
    char            essid[ESSID_LEN];
    signed char     strength;
    u8              type;
} __attribute__((packed)) scanned_net;

// network configured on card
typedef struct
{
    char            essid[ESSID_LEN];
} __attribute__((packed)) configured_net;

// set of networks scanned
typedef struct
{
    u8              count;
    scanned_net     nets[32];
} __attribute__((packed)) scanned_nets;

// set of configured networks
typedef struct
{
    u8              count;
    configured_net  nets[32];
} __attribute__((packed)) configured_nets;

// command parameters
typedef struct
{
    union {
        struct {
            unsigned char subcommand;
            unsigned char bytes;
            unsigned char args[32];
        } __attribute__((packed)) config;
        struct {
            unsigned char length;
            char SSID[32];
            struct network_key key;
        } __attribute__((packed)) network;
    };
} __attribute__((packed)) eyefi_param;

// command sent to card
typedef struct
{
    u8              cmd;
    eyefi_param     param;
} __attribute__((packed)) eyefi_command;

//-------------------------------------------------------------------

// map various communication structures to the memory buffer read/written to card
typedef struct
{
    union
    {
        u8              status;     // network testing status
        unsigned int    seq;        // command sequence number
        scanned_nets    a;          // list of available networks
        configured_nets c;          // list of configured networks
        eyefi_command   ec;         // command
        unsigned char   buf[EYEFI_BUF_SIZE];    // raw buffer data
    };
} __attribute__((packed)) _eyefi_interface;

static int running = 0;     // module status
static _eyefi_interface eyefi_buf;

static void clear_eyefi_buf()
{
    memset(eyefi_buf.buf, 0, EYEFI_BUF_SIZE);
}

static u8 atoh(char c)
{
    char lc = tolower(c);
    if ((c >= '0') && (c <= '9'))
        return c - '0';
    else if ((lc >= 'a') && (lc <= 'z'))
        return (lc - 'a') + 10;
    return 0;
}

/*
 * Take a string like "0ab1" and make it
 * a series of bytes: { 0x0a, 0xb1 }
 *
 * @len is the strlen() of the ascii
 */
static u8 *convert_ascii_to_hex(char *ascii)
{
    int i;
    static u8 hex[32];
    int len = strlen(ascii);

    memset(hex, 0, 32);

    for (i = 0; i < len; i += 2)
    {
        u8 high = atoh(ascii[i]);
        u8 low  = atoh(ascii[i+1]);
        hex[i/2] = (high<<4) | low;
    }

    return hex;
}

static int hex_only(char *str)
{
    int i;

    for (i = 0; i < strlen(str); i++) {
        if (((str[i] >= 'a') && str[i] <= 'f') ||
            ((str[i] >= 'A') && str[i] <= 'F') ||
            ((str[i] >= '0') && str[i] <= '9')) {
            continue;
        }
        return 0;
    }
    return 1;
}

// Convert user input to a network key - tries to guess key type based on length of input
// ToDo: should probably look at the network type returned from the list of available networks instead
static int make_network_key(struct network_key *key, char *essid, char *pass)
{
    u8 *hex_pass;
    int pass_len = strlen(pass);
    memset(key, 0, sizeof(*key));

    switch (pass_len) {
        case WPA_KEY_BYTES*2:
        case WEP_KEY_BYTES*2:
        case WEP_40_KEY_BYTES*2:
            if (hex_only(pass))
            {
                hex_pass = convert_ascii_to_hex(pass);
                if (!hex_pass)
                    return -EINVAL;
                key->len = pass_len/2;
                memcpy(&key->key[0], hex_pass, key->len);
                break;
            }
        // Fall through to default case
        default:
            key->len = WPA_KEY_BYTES;
            pbkdf2_sha1(pass, essid, strlen(essid), 4096, &key->key[0], WPA_KEY_BYTES);
            break;
    }
    return 0;
}

// Full path to EyeFi command/control file
static char *eyefi_filename(const char *nm)
{
    static char path[16];

    strcpy(path, "A/EyeFi/");
    strcat(path, nm);

    return path;
}

// Write a 16K command/control file to the card. Data is written from the eyefi_buf array.
// Returns true (1) is successful, false (0) otherwise
static int eyefi_writeFile(const char *filename)
{
    int fd = open(eyefi_filename(filename), O_RDWR|O_CREAT, 0600);

    if (fd < 0)
        return 0;

    int bytesWritten = write(fd, eyefi_buf.buf, EYEFI_BUF_SIZE);
    close(fd);

    return bytesWritten == EYEFI_BUF_SIZE;
}

// Read a 16K command/control file to the card. Data is placed in eyefi_buf array.
// Returns true (1) is successful, false (0) otherwise
static int eyefi_readFile(const char *filename)
{
    clear_eyefi_buf();

    int fd = open(eyefi_filename(filename), O_RDONLY, 0777);

    if (fd < 0)
        return 0;

    int bytesRead = read(fd, eyefi_buf.buf, EYEFI_BUF_SIZE);
    close(fd);

    return bytesRead == EYEFI_BUF_SIZE;
}

// Send a command with optional parameters to the card
// Basic flow:
//      - get current sequence number from RSPC file
//      - write command and parameters to REQM file
//      - write new sequence number to REQC file
//      - wait for sequence number to update in RSPC file
//      - read command results from RSPM file
static int eyefi_sendCommandParam(unsigned char cmd, eyefi_param *param)
{
    int i;

    if (!eyefi_readFile("RSPC")) return 0;
    unsigned int cur_seq = eyefi_buf.seq;

    clear_eyefi_buf();
    eyefi_buf.ec.cmd = cmd;

    if (param)
        memcpy(&eyefi_buf.ec.param, param, sizeof(eyefi_param));

    if (!eyefi_writeFile("REQM")) return -1;

    clear_eyefi_buf();
    eyefi_buf.seq = ++cur_seq;
    if (!eyefi_writeFile("REQC")) return -2;

    for (i=0; i<20; i++)
    {
        if (!eyefi_readFile("RSPC")) return -3;
        if (eyefi_buf.seq == cur_seq)
        {
            if (!eyefi_readFile("RSPM")) return -5;
            return 1;
        }
        msleep(250);
    }

    return -4;
}

#define eyefi_sendCommand(cmd)          eyefi_sendCommandParam(cmd, NULL)
#define eyefi_getAvailableNetworks()    eyefi_sendCommand('g')
#define eyefi_getConfiguredNetworks()   eyefi_sendCommand('l')
#define eyefi_getNetworkStatus()        eyefi_sendCommand('s')

// Send a network action command (command with SSID & optional password)
int eyefi_networkAction(unsigned char cmd, char *SSID, char *pwd)
{
    eyefi_param param;
    memset(&param, 0, sizeof(eyefi_param));
    param.network.length = (unsigned char)strlen(SSID);
    strcpy(param.network.SSID, SSID);
    if (pwd)
    {
        make_network_key(&param.network.key, SSID, pwd);
    }
    return eyefi_sendCommandParam(cmd, &param);
}

int eyefi_deleteNetwork(char *SSID)
{
    return eyefi_networkAction('d', SSID, NULL);
}

int eyefi_testNetwork(char *SSID,char *pwd)
{
    return eyefi_networkAction('t', SSID, pwd);
}

int eyefi_addNetwork(char *SSID,char *pwd)
{
    return eyefi_networkAction('a', SSID, pwd);
}

// Turn WiFi on or off.
int eyefi_enableWlan(int enable)
{
    eyefi_param param;
    memset(&param, 0, sizeof(eyefi_param));
    param.config.subcommand = EYEFI_WLAN_DISABLE;
    param.config.bytes = 1;
    param.config.args[0] = (unsigned char)enable;
    return eyefi_sendCommandParam('O', &param);
}

//int eyefi_wlanEnabled(int *pEnabled) {
//  eyefi_param param;
//    memset(&param,0,sizeof(param));
//  param.config.subcommand=EYEFI_WLAN_DISABLE;
//  param.config.bytes=0;
//  eyefi_sendCommandParam('o',&param);
//  *pEnabled=eyefi_buf.buf[1];
//  return 1;
//}

// Test mode status strings
char *eyefi_statusName(int n)
{
    static char *eyefi_status[] =
    {
        "not scanning",
        "locating network",
        "verifying network_key",
        "waiting for DHCP",
        "testing conn. to server",
        "success"
    };

    if (n<0 || n>=(int)(sizeof(eyefi_status)/sizeof(*eyefi_status)))
        return "?";

    return eyefi_status[n];
}

//-----------------------------------------------------------------------------
// GUI.c interface

#define MAX_NETWORK 9           // Limit of popup menu
#define PWD_LEN     32          // Max password length

static struct mpopup_item popup_eyefi[MAX_NETWORK+1];   // list of entries for popup menu (+1 for Cancel)
static char eyefi_selectedNetwork[ESSID_LEN+1];
static char eyefi_password[PWD_LEN+1];

// Exit module
static void eyefi_exit(__attribute__ ((unused))unsigned int button)
{
    running = 0;
    gui_set_need_restore();
}

static void eyefi_wlan_state(int on_off)
{
    running = 1;

    int n = eyefi_enableWlan(on_off);

    if (n <= 0)
        gui_mbox_init(LANG_ERROR_INITIALIZING_EYEFI,LANG_NO_EYEFI_CARD_FOUND,MBOX_BTN_OK,eyefi_exit);
    else
        gui_mbox_init(LANG_SUCCESS,(on_off)?LANG_EYEFI_WLAN_TURNED_ON:LANG_EYEFI_WLAN_TURNED_OFF,MBOX_BTN_OK,eyefi_exit);
}

static void confirm_delete_network_cb(unsigned int btn)
{
    if (btn == MBOX_BTN_YES)
    {
        int n = eyefi_deleteNetwork(eyefi_selectedNetwork);
        gui_mbox_init(LANG_SUCCESS,n<=0?LANG_CANNOT_DELETE_NETWORK:LANG_NETWORK_DELETED,MBOX_BTN_OK,eyefi_exit);
    }
    eyefi_exit(0);
}

static void select_configured_network_cb(unsigned nSelected)
{
    unsigned flag=1;
    int i, n;

    if (nSelected == MPOPUP_CANCEL)
    {
        eyefi_exit(0);
        return;
    }

    n = eyefi_buf.c.count;
    for (i=0; i<n && i<MAX_NETWORK; i++)
    {
        if (nSelected == flag) break;
        flag <<= 1;
    }

    if (nSelected != flag)
    {
        gui_mbox_init(LANG_POPUP_DELETE,LANG_CANNOT_FIND_NETWORK,MBOX_BTN_OK,eyefi_exit);
        return;
    }

    strcpy(eyefi_selectedNetwork, eyefi_buf.c.nets[i].essid);

    static char s[80];
    sprintf(s,"Delete \"%s\"?",eyefi_selectedNetwork);
    gui_mbox_init(LANG_POPUP_DELETE,(int)s/*LANG_ARE_YOU_SURE*/,MBOX_BTN_YES_NO|MBOX_DEF_BTN2,confirm_delete_network_cb);
}

static void eyefi_configured_networks()
{
    unsigned flag=1,flags=0;
    int i,n;

    running = 1;

    if (eyefi_getConfiguredNetworks() <= 0)
    {
        gui_mbox_init(LANG_ERROR_INITIALIZING_EYEFI,LANG_NO_EYEFI_CARD_FOUND,MBOX_BTN_OK,eyefi_exit);
    }
    else
    {
        memset(eyefi_selectedNetwork, 0, sizeof(eyefi_selectedNetwork));
        n = eyefi_buf.c.count;
        for (i=0; i<n && i<MAX_NETWORK; i++)
        {
            popup_eyefi[i].text = (int)(eyefi_buf.c.nets[i].essid);
            popup_eyefi[i].flag = flag;
            flags |= flag;
            flag <<= 1;
        }
        popup_eyefi[i].text = (int)"Cancel";
        popup_eyefi[i].flag = MPOPUP_CANCEL;

        libmpopup->show_popup(popup_eyefi, flags, select_configured_network_cb);
    }
}

static void confirm_add_network_cb(unsigned int btn)
{
    int n,i;

    if (btn==MBOX_BTN_YES)
    {
        n = eyefi_testNetwork(eyefi_selectedNetwork, eyefi_password);
        draw_progress_bar("testing network",5);
        for (i=0; i<50; i++)
        {
            msleep(10);
            n = eyefi_getNetworkStatus();
            if (eyefi_buf.status == 0)
            {
                gui_mbox_init(LANG_FAILED,LANG_WRONG_PASSWORD,MBOX_BTN_OK,eyefi_exit);
                return;
            }
            if (eyefi_buf.status <= EYEFI_SUCCESS)
            {
                draw_progress_bar(eyefi_statusName(eyefi_buf.status),(eyefi_buf.status*100)/(EYEFI_SUCCESS+1));
                if (eyefi_buf.status == EYEFI_SUCCESS)
                    break;
            }
            else
                draw_progress_bar("????",50);
        }

        if (eyefi_buf.status != EYEFI_SUCCESS)
        {
            gui_mbox_init(LANG_FAILED,LANG_WRONG_PASSWORD,MBOX_BTN_OK,eyefi_exit);
            return;
        }
    }

    draw_progress_bar("adding network", 95);
    n = eyefi_addNetwork(eyefi_selectedNetwork, eyefi_password);
    if (n > 0)
        gui_mbox_init(LANG_SUCCESS,LANG_NETWORK_ADDED,MBOX_BTN_OK,eyefi_exit);
    else
        gui_mbox_init(LANG_FAILED,LANG_PROBLEM_ADDING_NETWORK,MBOX_BTN_OK,eyefi_exit);
}

static void password_cb(const char *str)
{
    if (str == NULL)
    {
        eyefi_exit(0);
    }
    else
    {
        strncpy(eyefi_password, str, sizeof(eyefi_password)-1);
        gui_mbox_init(LANG_ADD_NETWORK,LANG_TEST_NETWORK,MBOX_BTN_YES_NO|MBOX_DEF_BTN1,confirm_add_network_cb);
    }
}

static void select_available_network_cb(unsigned nSelected)
{
    unsigned flag=1;
    int i,n;

    if (nSelected == MPOPUP_CANCEL)
    {
        eyefi_exit(0);
        return;
    }

    n = eyefi_buf.a.count;
    for (i=0; i<n && i<MAX_NETWORK; i++)
    {
        if (nSelected == flag) break;
        flag <<= 1;
    }

    if (nSelected != flag)
    {
        gui_mbox_init(LANG_ADD_NETWORK,LANG_CANNOT_FIND_NETWORK,MBOX_BTN_OK,eyefi_exit);
        return;
    }

    strcpy(eyefi_selectedNetwork,eyefi_buf.a.nets[i].essid);
    libtextbox->textbox_init((int)eyefi_selectedNetwork, LANG_PASSWORD, eyefi_password, PWD_LEN, password_cb, NULL);
}

static void eyefi_available_networks()
{
    unsigned flag=1,flags=0;
    int i,n;

    running = 1;

    if (eyefi_getAvailableNetworks() <= 0)
    {
        gui_mbox_init(LANG_ERROR_INITIALIZING_EYEFI,LANG_NO_EYEFI_CARD_FOUND,MBOX_BTN_OK,eyefi_exit);
    }
    else
    {
        memset(eyefi_selectedNetwork,0,sizeof(eyefi_selectedNetwork));
        n = eyefi_buf.a.count;

        if (n == 0)
        {
            gui_mbox_init(LANG_ADD_NETWORK,LANG_CANNOT_FIND_NETWORK,MBOX_BTN_OK,eyefi_exit);
        }
        else
        {
            for (i=0; i<n && i<MAX_NETWORK; i++)
            {
                popup_eyefi[i].text = (int)&(eyefi_buf.a.nets[i].essid);
                popup_eyefi[i].flag = flag;
                flags |= flag;
                flag <<= 1;
            }
            popup_eyefi[i].text = (int)"Cancel";
            popup_eyefi[i].flag = MPOPUP_CANCEL;

            libmpopup->show_popup(popup_eyefi,flags,select_available_network_cb);
        }
    }
}

//-----------------------------------------------------------------------------
static void eyefi_wlan_off()
{
    eyefi_wlan_state(0);
}

static void eyefi_wlan_on()
{
    eyefi_wlan_state(1);
}

static CMenuItem eyefi_submenu_items[] = {
    MENU_ITEM   (0x5c,LANG_MENU_EYEFI_AVAILABLE_NETWORKS,   MENUITEM_PROC,          eyefi_available_networks,  0 ),
    MENU_ITEM   (0x5c,LANG_MENU_EYEFI_CONFIGURED_NETWORKS,  MENUITEM_PROC,          eyefi_configured_networks, 0 ),
    MENU_ITEM   (0x5c,LANG_FORCE_EYEFI_WLAN_OFF,            MENUITEM_PROC,          eyefi_wlan_off,            0 ),
    MENU_ITEM   (0x5c,LANG_FORCE_EYEFI_WLAN_ON,             MENUITEM_PROC,          eyefi_wlan_on,             0 ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,            0,                         0 ),
    {0}
};

static CMenu eyefi_submenu = {0x21,LANG_MENU_EYEFI_TITLE, eyefi_submenu_items };

//-----------------------------------------------------------------------------
int _module_can_unload()
{
    return (running == 0) || (get_curr_menu() != &eyefi_submenu);
}

int _run()
{
    running = 1;
    gui_activate_sub_menu(&eyefi_submenu);
    return 0;
}

int _module_exit_alt()
{
    running = 0;
    return 0;
}

#include "simple_module.h"

// =========  MODULE INIT =================

libsimple_sym _libeyefi =
{
    {
         0, 0, _module_can_unload, _module_exit_alt, _run
    },
};

ModuleInfo _module_info =
{
    MODULEINFO_V1_MAGICNUM,
    sizeof(ModuleInfo),
    SIMPLE_MODULE_VERSION,      // Module version

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,         // Requirements of CHDK version
    ANY_PLATFORM_ALLOWED,       // Specify platform dependency

    -LANG_EYEFI,
    MTYPE_TOOL|MTYPE_SUBMENU_TOOL,  //Handle Eyefi SD cards

    &_libeyefi.base,

    ANY_VERSION,                // CONF version
    ANY_VERSION,                // CAM SCREEN version
    ANY_VERSION,                // CAM SENSOR version
    ANY_VERSION,                // CAM INFO version

    0,
};
