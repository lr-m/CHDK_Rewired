#include "string.h"
#include "fileutil.h"
#include "conf_migrate.h"

// Module to handle migration of config file versions to new formats.
// Currently supports loading 1.2 file into current conf structure.
// Also provides access to conf variable for scripts using old (1.2) ID's.

int running = 0;

// mapping table to convert previous version (1.2) ConfInfo ID's to new version
static short conf_map_1_2[] =
{
    0,
    2001, // 1 conf.show_osd
    1020, // 2 conf.save_raw
    1050, // 3 conf.script_shoot_delay
    1060, // 4 conf.show_histo
    2006, // 5 conf.rotate_osd
    1052, // 6 conf.script_param_set
    2150, // 7 conf.show_dof
    2100, // 8 conf.batt_volts_max
    2101, // 9 conf.batt_volts_min
    0,
    2102, // 11 conf.batt_perc_show
    2103, // 12 conf.batt_volts_show
    2104, // 13 conf.batt_icon_show
    2090, // 14 conf.show_state
    2160, // 15 conf.show_values
    1061, // 16 conf.show_overexp
    1062, // 17 conf.histo_mode
    1063, // 18 conf.histo_auto_ajust
    1064, // 19 conf.histo_ignore_boundary
    1065, // 20 conf.histo_layout
    2020, // 21 conf.histo_pos
    2021, // 22 conf.dof_pos
    2022, // 23 conf.batt_icon_pos
    2023, // 24 conf.batt_txt_pos
    2024, // 25 conf.mode_state_pos
    2025, // 26 conf.values_pos
    2050, // 27 conf.histo_color
    2052, // 28 conf.osd_color
    0,
    2055, // 30 conf.menu_color
    2059, // 31 conf.reader_color
    1207, // 32 conf.ricoh_ca1_mode
    1226, // 33 conf.flashlight
    2230, // 34 conf.debug_shortcut_action
    1021, // 35 conf.raw_in_dir
    1022, // 36 conf.raw_prefix
    1023, // 37 conf.raw_ext
    2110, // 38 conf.reader_file
    2111, // 39 conf.reader_pos
    0,
    2080, // 41 conf.show_clock
    2026, // 42 conf.clock_pos
    2112, // 43 conf.reader_autoscroll
    2113, // 44 conf.reader_autoscroll_delay
    2114, // 45 conf.reader_rbf_file
    2115, // 46 conf.reader_codepage
    2120, // 47 conf.splash_show
    2051, // 48 conf.histo_color2
    1070, // 49 conf.zebra_draw
    1071, // 50 conf.zebra_mode
    1072, // 51 conf.zebra_restore_screen
    1073, // 52 conf.zebra_restore_osd
    1074, // 53 conf.zebra_over
    1075, // 54 conf.zebra_under
    2062, // 55 conf.zebra_color
    1076, // 56 conf.zebra_draw_osd
    3002, // 57 conf.user_menu_as_root
    2175, // 58 conf.zoom_value
    1222, // 59 conf.use_zoom_mf
    1024, // 60 conf.raw_save_first_only
    2116, // 61 conf.reader_wrap_by_words
    2130, // 62 conf.menu_symbol_enable
    1220, // 63 conf.alt_mode_button
    2135, // 64 conf.lang_file
    2136, // 65 conf.font_cp
    2131, // 66 conf.menu_rbf_file
    1221, // 67 conf.alt_prevent_shutdown
    2140, // 68 conf.show_grid_lines
    2141, // 69 conf.grid_lines_file
    1039, // 70 conf.raw_nr
    2142, // 71 conf.grid_force_color
    2060, // 72 conf.grid_color
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2151, // 80 conf.dof_subj_dist_as_near_limit
    2152, // 81 conf.dof_use_exif_subj_dist
    2153, // 82 conf.dof_subj_dist_in_misc
    2154, // 83 conf.dof_near_limit_in_misc
    2155, // 84 conf.dof_far_limit_in_misc
    2156, // 85 conf.dof_hyperfocal_in_misc
    2157, // 86 conf.dof_depth_in_misc
    2161, // 87 conf.values_show_in_review
    2162, // 88 conf.values_show_zoom
    2163, // 89 conf.values_show_real_aperture
    2164, // 90 conf.values_show_real_iso
    2165, // 91 conf.values_show_market_iso
    2166, // 92 conf.values_show_iso_only_in_autoiso_mode
    2167, // 93 conf.values_show_ev_seted
    2168, // 94 conf.values_show_ev_measured
    2169, // 95 conf.values_show_bv_measured
    2170, // 96 conf.values_show_bv_seted
    2171, // 97 conf.values_show_overexposure
    2172, // 98 conf.values_show_luminance
    1080, // 99 conf.video_mode
    1081, // 100 conf.video_quality
    1082, // 101 conf.video_bitrate
    1112, // 102 conf.tv_override_value
    1110, // 103 conf.tv_override_enabled
    1121, // 104 conf.av_override_value
    1130, // 105 conf.iso_override_value
    1131, // 106 conf.iso_override_koef
    1140, // 107 conf.subj_dist_override_value
    1141, // 108 conf.subj_dist_override_koef
    1150, // 109 conf.tv_bracket_value
    1151, // 110 conf.av_bracket_value
    1152, // 111 conf.iso_bracket_value
    1153, // 112 conf.iso_bracket_koef
    1154, // 113 conf.subj_dist_bracket_value
    1155, // 114 conf.subj_dist_bracket_koef
    1156, // 115 conf.bracket_type
    1113, // 116 conf.tv_override_long_exp
    1114, // 117 conf.tv_override_short_exp
    1120, // 118 conf.av_override_enabled
    1999, // 119 conf.script_allow_lua_native_calls
    1053, // 120 conf.script_startup
    1200, // 121 conf.remote_enable
    2173, // 122 conf.values_show_canon_overexposure
    1100, // 123 conf.clear_override
    2002, // 124 conf.show_osd_in_review
    0,
    1157, // 126 conf.clear_bracket
    1077, // 127 conf.zebra_multichannel
    1003, // 128 conf.nd_filter_state
    1066, // 129 conf.histo_show_ev_grid
    2053, // 130 conf.osd_color_warn
    2061, // 131 conf.space_color
    2180, // 132 conf.space_icon_show
    2027, // 133 conf.space_icon_pos
    2181, // 134 conf.space_perc_show
    2182, // 135 conf.space_mb_show
    2028, // 136 conf.space_txt_pos
    2200, // 137 conf.show_remaining_raw
    2029, // 138 conf.mode_raw_pos
    2201, // 139 conf.show_raw_state
    2174, // 140 conf.show_values_in_video
    1111, // 141 conf.tv_enum_type
    3003, // 142 conf.user_menu_enable
    3001, // 143 conf.user_menu_vars
    2176, // 144 conf.zoom_scale
    2183, // 145 conf.space_bar_show
    2184, // 146 conf.space_bar_size
    2030, // 147 conf.space_ver_pos
    2031, // 148 conf.space_hor_pos
    2185, // 149 conf.space_bar_width
    2186, // 150 conf.space_perc_warn
    2187, // 151 conf.space_mb_warn
    2188, // 152 conf.space_warn_type
    2202, // 153 conf.remaining_raw_treshold
    1086, // 154 conf.unlock_optical_zoom_for_video
    2081, // 155 conf.clock_format
    2082, // 156 conf.clock_indicator
    2083, // 157 conf.clock_halfpress
    1160, // 158 conf.autoiso_enable
    1161, // 159 conf.autoiso_shutter_enum
    1162, // 160 conf.autoiso_user_factor
    1163, // 161 conf.autoiso_is_factor
    1164, // 162 conf.autoiso_max_iso_hi
    1165, // 163 conf.autoiso_max_iso_auto
    1166, // 164 conf.autoiso_min_iso
    2056, // 165 conf.menu_title_color
    2057, // 166 conf.menu_cursor_color
    2134, // 167 conf.menu_center
    1084, // 168 conf.mute_on_zoom
    1033, // 169 conf.bad_pixel_removal
    1085, // 170 conf.video_af_key
    2054, // 171 conf.osd_color_override
    2003, // 172 conf.override_disable
    2004, // 173 conf.override_disable_all
    2005, // 174 conf.hide_osd
    1025, // 175 conf.save_raw_in_video
    2210, // 176 conf.show_movie_time
    2211, // 177 conf.show_movie_refresh
    2032, // 178 conf.mode_video_pos
    1083, // 179 conf.clear_video
    1087, // 180 conf.fast_ev
    1088, // 181 conf.fast_ev_step
    2033, // 182 conf.mode_ev_pos
    2132, // 183 conf.menu_symbol_rbf_file
    2058, // 184 conf.menu_symbol_color
    1180, // 185 conf.curve_file
    1181, // 186 conf.curve_enable
    1190, // 187 conf.edge_overlay_enable
    1191, // 188 conf.edge_overlay_thresh
    2063, // 189 conf.edge_overlay_color
    1201, // 190 conf.synch_enable
    1202, // 191 conf.synch_delay_enable
    1203, // 192 conf.synch_delay_value
    0,
    1054, // 194 conf.script_file
    2234, // 195 conf.mem_view_addr_init
    1026, // 196 conf.save_raw_in_sports
    1027, // 197 conf.save_raw_in_burst
    1028, // 198 conf.save_raw_in_ev_bracketing
    1029, // 199 conf.save_raw_in_timer
    2203, // 200 conf.raw_exceptions_warn
    2133, // 201 conf.menu_select_first_entry
    1089, // 202 conf.fast_movie_control
    2220, // 203 conf.show_temp
    2034, // 204 conf.temp_pos
    1090, // 205 conf.fast_movie_quality_control
    0,
    0,
    2121, // 208 conf.start_sound
    1041, // 209 conf.sub_batch_prefix
    1042, // 210 conf.sub_batch_ext
    0,
    0,
    2231, // 213 conf.debug_display
    1055, // 214 conf.script_param_save
    2035, // 215 conf.ev_video_pos
    0,
    0,
    0,
    1040, // 219 conf.bracketing_add_raw_suffix
    2221, // 220 conf.temperature_unit
    0,
    1192, // 222 conf.edge_overlay_play
    1193, // 223 conf.edge_overlay_pano
    1194, // 224 conf.edge_overlay_zoom
    1030, // 225 conf.raw_cache
    1036, // 226 conf.dng_raw
    1210, // 227 conf.flash_sync_curtain
    1034, // 228 conf.raw_timer
    1001, // 229 conf.platformid
    1031, // 230 conf.save_raw_in_edgeoverlay
    1032, // 231 conf.save_raw_in_auto
    1211, // 232 conf.flash_video_override
    1212, // 233 conf.flash_video_override_power
    1035, // 234 conf.raw_dng_ext
    1038, // 235 conf.dng_usb_ext
    1213, // 236 conf.flash_manual_override
    1002, // 237 conf.fast_image_quality
    1224, // 238 conf.debug_lua_restart_on_error
    2232, // 239 conf.debug_propcase_page
    2233, // 240 conf.debug_misc_vals_show
    1195, // 241 conf.edge_overlay_filter
    1196, // 242 conf.edge_overlay_show
    1197, // 243 conf.edge_overlay_pano_overlap
    2240, // 244 conf.touchscreen_disable_video_controls
    2241, // 245 conf.touchscreen_disable_shortcut_controls
    2250, // 246 conf.usb_info_enable
    2036, // 247 conf.usb_info_pos
    1204, // 248 conf.remote_switch_type
    1205, // 249 conf.remote_control_mode
    1206, // 250 conf.remote_enable_scripts
    2189, // 251 conf.show_partition_nr
    1091, // 252 conf.ext_video_time
    4001, // 253 conf.gps_record
    4002, // 254 conf.gps_navi_show
    4003, // 255 conf.gps_compass_show
    4004, // 256 conf.gps_coordinates_show
    4005, // 257 conf.gps_height_show
    4006, // 258 conf.gps_waypoint_save
    4007, // 259 conf.gps_track_time
    4008, // 260 conf.gps_compass_hide
    4009, // 261 conf.gps_wait_for_signal
    4010, // 262 conf.gps_compass_time
    4011, // 263 conf.gps_navi_time
    4012, // 264 conf.gps_wait_for_signal_time
    4013, // 265 conf.gps_compass_smooth
    4014, // 266 conf.gps_batt
    4015, // 267 conf.gps_countdown
    4016, // 268 conf.gps_2D_3D_fix
    4017, // 269 conf.gps_countdown_blink
    4018, // 270 conf.gps_rec_play_set
    4019, // 271 conf.gps_play_dark_set
    4020, // 272 conf.gps_rec_play_time
    4021, // 273 conf.gps_play_dark_time
    4022, // 274 conf.gps_rec_play_set_1
    4023, // 275 conf.gps_play_dark_set_1
    4024, // 276 conf.gps_rec_play_time_1
    4025, // 277 conf.gps_play_dark_time_1
    4026, // 278 conf.gps_show_symbol
    4027, // 279 conf.gps_batt_warn
    4029, // 280 conf.gps_test_timezone
    4030, // 281 conf.gps_beep_warn
    4031, // 282 conf.gps_on_off
    1170, // 283 conf.autoiso2_shutter_enum
    1171, // 284 conf.autoiso2_max_iso_auto
    1172, // 285 conf.autoiso2_over
    1173, // 286 conf.overexp_threshold
    1174, // 287 conf.overexp_ev_enum
    1223, // 288 conf.zoom_assist_button_disable
    1037, // 289 conf.dng_version
    2137, // 290 conf.tbox_char_map
    2260, // 291 conf.show_alt_helper
    2261, // 292 conf.show_alt_helper_delay
    2263, // 293 conf.extra_button
    1225, // 294 conf.module_logging
    2262, // 295 conf.enable_shortcuts
    2270, // 296 conf.console_show
    2271, // 297 conf.console_timeout
    1230, // 298 conf.memdmp_start
    1231, // 299 conf.memdmp_size
    1214, // 300 conf.flash_exp_comp
    1215, // 301 conf.flash_enable_exp_comp
    2297, // 302 conf.enable_raw_shortcut
};

//-----------------------------------------------------------------------------

// Load values from 1.2 config file if it exists
int load_old_config(const confinfo_handler *confinfo_handlers)
{
    int rv = 0;
    int rcnt, i, offs;
    unsigned short id, size;

    char* buf = load_file("A/CHDK/CCHDK3.CFG", &rcnt, 0);

    if (buf)
    {
        if (*(int*)buf == CONF_MAGICK_VALUE)
        {
            rv = 1;
            offs = sizeof(int);
            while (1)
            {
                if (offs + (int)sizeof(short) > rcnt)
                    break;
                id = conf_map_1_2[*((short*)(buf + offs))];
                offs += sizeof(short);

                if (offs + (int)sizeof(short) > rcnt)
                    break;
                size = *((short*)(buf + offs));
                offs += sizeof(short);

                ConfInfo *ci = 0;
                void (*info_func)(unsigned short id) = 0;
                for (i=0; confinfo_handlers[i].ci != 0; i++)
                {
                    if ((id >= confinfo_handlers[i].start_id) && (id <= confinfo_handlers[i].end_id))
                    {
                        ci = confinfo_handlers[i].ci;
                        info_func = confinfo_handlers[i].info_func;
                        id -= confinfo_handlers[i].start_id;
                        break;
                    }
                }

                if (ci != 0)
                {
                    for (i=0; ci[i].id > 0; ++i)
                    {
                        if (ci[i].id==id && ci[i].size==size)
                        {
                            if (offs + size <= rcnt)
                            {
                                if (ci[i].type == CONF_STRUCT_PTR)
                                {
                                    tVarArrayConfig *cfg = (tVarArrayConfig*)(ci[i].var);
                                    size = cfg->load(buf+offs);
                                }
                                else
                                {
                                    memcpy(ci[i].var, buf+offs, size);
                                }
                                if (info_func) info_func(ci[i].id);
                            }
                            break;
                        }
                    }
                }

                offs += size;
            }
        }
        free(buf);
    }

    // module can be unloaded
    running = 0;
    return rv;
}

// Map 1.2 ID to current value
short map_old_id(short id)
{
    // Note: this is called from scripts.
    // Module will not be unloaded if this is ever called, as it is likely to be needed again by the script(s).
    return conf_map_1_2[id];
}

//-----------------------------------------------------------------------------
int _module_loader()
{
    running = 1;
    return 0;
}

int _module_unloader()
{
    return 0;
}

int _module_can_unload()
{
    return running == 0;
}

// =========  MODULE INIT =================

libconfmigrate_sym _libconfmigrate =
{
    {
         _module_loader, _module_unloader, _module_can_unload, 0, 0
    },

    load_old_config,
    map_old_id,
};

ModuleInfo _module_info =
{
    MODULEINFO_V1_MAGICNUM,
    sizeof(ModuleInfo),
    CONF_MIGRATE_VERSION,       // Module version

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,        // Requirements of CHDK version
    ANY_PLATFORM_ALLOWED,       // Specify platform dependency

    (int32_t)"Config Migrate",
    MTYPE_EXTENSION,

    &_libconfmigrate.base,

    ANY_VERSION,                // CONF version
    ANY_VERSION,                // CAM SCREEN version
    ANY_VERSION,                // CAM SENSOR version
    ANY_VERSION,                // CAM INFO version

    0,
};
