#include "camera_info.h"
#include "conf.h"
#include "keyboard.h"
#include "font.h"
#include "lang.h"
#include "gui.h"
#include "gui_lang.h"
#include "gui_draw.h"
#include "gui_menu.h"
#include "gui_user_menu.h"
#include "gui_mbox.h"
#include "modules.h"
#include "module_load.h"

#define USER_MENU_IS_SCRIPT     -1
#define USER_MENU_IS_MODULE     -2

//-------------------------------------------------------------------

static CMenuItem* find_menu_item(CMenu *cmenu, int itemid )
{
    if (itemid)
    {
        int i;

        for (i = 0; cmenu->menu[i].text; i += 1)
        {
            if ( lang_strhash31(cmenu->menu[i].text) == (unsigned)itemid)
            {
                return (CMenuItem*) &(cmenu->menu[i]);
            }
            if ((cmenu->menu[i].type & MENUITEM_MASK) == MENUITEM_SUBMENU)
            {
                if (cmenu->menu[i].text != LANG_MENU_USER_MENU)
                {
                    CMenuItem* rv = find_menu_item((CMenu*)(cmenu->menu[i].value), itemid);
                    if (rv)
                        return rv;
                }
            }
        }
    }

    return 0;
}

//-------------------------------------------------------------------
static int user_menu_saved_size()
{
    // User menu saved as:
    //      - num_items (int)
    //      - (var + script_file + script_title)[num_items]
    return conf.user_menu_vars.cfg.num_items * (sizeof(int) + CONF_STR_LEN * 2) + sizeof(int);
}

static char* user_menu_store_data(char *p)
{
    int i;

    memcpy(p, &conf.user_menu_vars.cfg.num_items, sizeof(int));
    p += sizeof(int);
    
    for (i=0; i<conf.user_menu_vars.cfg.num_items; i++)
    {
        memcpy(p, &conf.user_menu_vars.items[i].var, sizeof(int));
        p += sizeof(int);
        memset(p, 0, CONF_STR_LEN * 2);
        if (conf.user_menu_vars.items[i].script_file) memcpy(p, conf.user_menu_vars.items[i].script_file, strlen(conf.user_menu_vars.items[i].script_file));
        p += CONF_STR_LEN;
        if (conf.user_menu_vars.items[i].script_title) memcpy(p, conf.user_menu_vars.items[i].script_title, strlen(conf.user_menu_vars.items[i].script_title));
        p += CONF_STR_LEN;
    }

    return p;
}

static int user_menu_load_data(char *p)
{
    int i, n;

    memcpy(&n, p, sizeof(int));
    if (n > conf.user_menu_vars.cfg.num_items)
        init_user_menu(n);
    p += sizeof(int);
    
    for (i=0; i<n; i++)
    {
        memcpy(&conf.user_menu_vars.items[i].var, p, sizeof(int));
        p += sizeof(int);
        if (*p)
        {
            if (conf.user_menu_vars.items[i].script_file == 0)
                conf.user_menu_vars.items[i].script_file = malloc(CONF_STR_LEN);
            strcpy(conf.user_menu_vars.items[i].script_file,p);
        }
        p += CONF_STR_LEN;
        if (*p)
        {
            if (conf.user_menu_vars.items[i].script_title == 0)
                conf.user_menu_vars.items[i].script_title = malloc(CONF_STR_LEN);
            strcpy(conf.user_menu_vars.items[i].script_title,p);
        }
        p += CONF_STR_LEN;
    }

    return user_menu_saved_size();
}

void init_user_menu(int num_items)
{
    if (conf.user_menu_vars.items)
    {
        int i;
        for (i=0; i<conf.user_menu_vars.cfg.num_items; i++)
        {
            if (conf.user_menu_vars.items[i].script_file)
                free(conf.user_menu_vars.items[i].script_file);
            if (conf.user_menu_vars.items[i].script_title)
                free(conf.user_menu_vars.items[i].script_title);
        }
        free(conf.user_menu_vars.items);
        conf.user_menu_vars.items = 0;
    }

    conf.user_menu_vars.cfg.num_items = num_items;
    conf.user_menu_vars.cfg.item_size = sizeof(tUserMenuItem);
    conf.user_menu_vars.cfg.saved_size = user_menu_saved_size;
    conf.user_menu_vars.cfg.save = user_menu_store_data;
    conf.user_menu_vars.cfg.load = user_menu_load_data;
    conf.user_menu_vars.items = malloc(num_items * sizeof(tUserMenuItem));
    memset(conf.user_menu_vars.items, 0, num_items * sizeof(tUserMenuItem));
}

//-------------------------------------------------------------------
static void rinit()
{
    // Erase screen if switching from user menu to main menu
    // in case the user menu is larger than the main menu
    // otherwise it leaves remnants of the user menu above and below
    // the main menu.
    gui_set_need_restore();
    gui_menu_init(&root_menu);
}

//-------------------------------------------------------------------
/*
 * 1 extra entry for the "Main menu" and 1 for null when the menu is full with user selections
 * Compiler will zero init remaining portion of array so no there is no hidden relationship between
 * this structure and the value of USER_MENU_ITEMS. The value of USER_MENU_ITEMS can be anything you
 * wish and everything automagically works.
*/

static CMenuItem user_submenu_items[USER_MENU_ITEMS + 2] = {
    MENU_ITEM(0x20,LANG_MENU_MAIN_TITLE,     MENUITEM_PROC,  rinit, 0 )
};

CMenu user_submenu = {0x2e,LANG_MENU_USER_MENU, user_submenu_items };

//-------------------------------------------------------------------

extern CMenu script_submenu;
static char sfname[CONF_STR_LEN];

static void load_user_menu_script_callback(unsigned int btn)
{
    if (btn==MBOX_BTN_YES)
    {
        script_load(sfname);
        gui_activate_sub_menu(&script_submenu) ;
    }
}

void gui_load_user_menu_script(const char *fn) 
{
    if (fn)
    {
        strcpy(sfname, fn);
        
        if( kbd_get_clicked_key() == KEY_SHOOT_FULL )       
        {
            script_load(sfname);                            // load script from menu choice            
            gui_set_mode(&altGuiHandler);                   // exit the menu system
            script_start_gui(0);                            // and run the script
        }
        else
        {
            gui_mbox_init(LANG_MENU_SCRIPT_LOAD, (int)sfname, MBOX_BTN_YES_NO|MBOX_TEXT_CENTER|MBOX_FUNC_RESTORE, load_user_menu_script_callback);   
        }
    }
}

//-------------------------------------------------------------------
void user_menu_save() {
    int x;
    for (x=0; x<USER_MENU_ITEMS; x++) {
        /*
         * First entry in user_submenu_items is reserved for the "Main Menu"
         * conf.user_menu_vars only tracks/saves the real user entries.
         */

        if ( user_submenu_items[x+1].text )
        {
            if ( user_submenu_items[x+1].value == (int *)gui_load_user_menu_script )
            {
                conf.user_menu_vars.items[x].var = USER_MENU_IS_SCRIPT;                             // flag script entries specially 
            }
            else if ((user_submenu_items[x+1].value == (int *)module_run) && ((user_submenu_items[x+1].type & MENUITEM_USER_MODULE) == MENUITEM_USER_MODULE))
            {
                conf.user_menu_vars.items[x].var = USER_MENU_IS_MODULE;                             // flag module entries specially 
            }
            else
            {
                conf.user_menu_vars.items[x].var = lang_strhash31(user_submenu_items[x+1].text);    // otherwise save a hash
            }
        }
        else
        {
            conf.user_menu_vars.items[x].var = 0;
            if (conf.user_menu_vars.items[x].script_file != 0)
            {
                free(conf.user_menu_vars.items[x].script_file);
                conf.user_menu_vars.items[x].script_file = 0;
            }
            if (conf.user_menu_vars.items[x].script_title != 0)
            {
                free(conf.user_menu_vars.items[x].script_title);
                conf.user_menu_vars.items[x].script_title = 0;
            }
        }
    } 
}

static void set_user_menu_extern(int menu, int var, char sym, short type, int* func)
{
    // restore the script/module entry
    user_submenu_items[menu].symbol = sym;
    user_submenu_items[menu].opt_len = 0;
    user_submenu_items[menu].type = type;
    user_submenu_items[menu].text = (int)conf.user_menu_vars.items[var].script_title;
    user_submenu_items[menu].value = func;
    user_submenu_items[menu].arg = (int)conf.user_menu_vars.items[var].script_file;  
}

void user_menu_restore()
{
    int x, y;
    CMenuItem* item=0;
    /*
     * First entry in user_submenu_items is reserved for the "Main Menu"
     * conf.user_menu_vars only tracks/saves the real user entries.
     */
       
    for (x=0, y=1; x<USER_MENU_ITEMS; x++, y++)
    {
        if (conf.user_menu_vars.items[x].var == USER_MENU_IS_SCRIPT)    // special flag- there is no hash for script entries
        {
            set_user_menu_extern(y, x, 0x35, MENUITEM_PROC, (int*)gui_load_user_menu_script);
        }
        else if (conf.user_menu_vars.items[x].var == USER_MENU_IS_MODULE)    // special flag- there is no hash for module entries
        {   
            set_user_menu_extern(y, x, 0x28, MENUITEM_PROC|MENUITEM_USER_MODULE, (int*)module_run);
        }
        else
        { 
            if (conf.user_menu_vars.items[x].var > 0 )  // look up the menu text
                 item = find_menu_item(&root_menu, conf.user_menu_vars.items[x].var);
            else item = NULL ;
            
            if ( item )                                 // add back in if found
            {
                user_submenu_items[y] = *item;
            }
            else                                        // otherwise clear the menu entry
            {
                user_submenu_items[y].text = 0;
                break ;                                 // and exit - all done !
            }
        }
    }  
}

