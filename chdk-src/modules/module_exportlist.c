/*
 *   THIS FILE IS FOR DECLARATION/LISTING OF EXPORTED TO MODULES SYMBOLS
 *
 *    CHDK-FLAT Module System.  Sergey Taranenko aka tsvstar
 */

// This file is parsed by the makeexport.c program to generate the
// symbol hash table loaded later (from module_hashlist.h)
// Symbols to be exported should be on separate lines. Blank lines and '//' style comments are allowed
// (Despite the name, this is not a C source file. File should probably be renamed.)

            module_get_adr
            module_exit_alt
            module_run
            module_preload
            &chdk_colors
            theme_color

            &script_version

            &libscriptapi
            &libtextbox
            &librawop
            &libmotiondetect
            &libcurves
            &libdng
            &libfselect
            &libmpopup
            &libtxtread
            &libhisto
            &libshothisto

            &altGuiHandler
            &camera_info
            &camera_screen
            &camera_sensor
            &conf
            &zoom_points
            &recreview_hold
            &root_menu
            &user_submenu

            malloc
            free
            umalloc
            ufree
            exmem_alloc_uncached
            exmem_free_uncached
            dcache_clean_all
            load_file
            process_file

            write
            lseek
            open
            close
            read
            remove
            rename
            stat
            opendir
            opendir_chdk
            readdir
            closedir
            mkdir

            fopen
            fclose
            fseek
            fread
            fwrite
            feof
            fflush
            fgets
            SetFileAttributes

            get_tick_count
            time
            utime
            localtime
            get_localtime
            mktime
            strftime
            msleep
            set_clock

            rand
            srand
            qsort
            pow
            sqrt
            log10
            log2
            log

            lang_str
            sprintf
            strlen
            strcpy
            strcat
            strncmp
            strpbrk
            strchr
            strcmp
            strtol
            strrchr
            strncpy
            strerror
            strtoul
            strstr
            tolower
            toupper
            memcpy
            memset
            memchr
            memcmp
            isalnum
            isalpha
            iscntrl
            islower
            ispunct
            isspace
            isupper
            isxdigit

            text_dimensions
            draw_string
            draw_string_justified
            draw_text_justified
            draw_string_scaled
            draw_line
            draw_char
            draw_pixel
            draw_or_erase_edge_pixel
            draw_ellipse
            draw_set_draw_proc
            draw_set_grid_clip
            draw_restore
            draw_button
            draw_rectangle
            draw_hline
            draw_vline
            draw_progress_bar

            bend_tag_read
            bend_store_save_recipe
            bend_store_slot_at
            bend_store_detach
            bend_seg_prep
            bend_seg_name
            bend_seg_label
            bend_seg_count
            bend_src_name
            bendx_chain_count
            bendx_chain_sanitize
            bendx_name
            bendx_label

            // special functions for yuv overlay (DIGIC 6)
            erase_zebra
            draw_dblpixel_raw
            color_to_rawpx

            chdkColorToCanonColor
            user_color
            get_script_color
            gui_set_mode
            gui_default_kbd_process_menu_btn
            get_batt_perc
            gui_mbox_init
            gui_set_need_restore
            gui_load_user_menu_script

            gui_draw_osd_elements
            gui_osd_draw_clock

            gui_activate_sub_menu
            get_curr_menu

            vid_get_bitmap_fb
            vid_bitmap_refresh
            vid_get_bitmap_active_palette
            vid_get_viewport_height
            vid_get_viewport_width
            vid_get_viewport_height_proper
            vid_get_viewport_width_proper
            vid_get_viewport_byte_width
            vid_get_viewport_display_xoffset
            vid_get_viewport_display_yoffset
            vid_get_viewport_image_offset
            vid_get_viewport_row_offset
            vid_get_viewport_yscale
            vid_get_viewport_active_buffer

            get_raw_image_addr
            hook_raw_image_addr
            raw_prepare_develop
            get_raw_pixel
            set_raw_pixel
            patch_bad_pixel
            raw_createfile
            raw_closefile
            raw_get_path

            kbd_get_autoclicked_key
            kbd_is_key_pressed
            kbd_key_press
            kbd_get_clicked_key
            get_jogdial_direction
            JogDial_CCW
            JogDial_CW
            kbd_force_analog_av

            debug_led
            camera_set_led

            shooting_get_av96
            shooting_get_bv96
            shooting_get_common_focus_mode
            shooting_get_display_mode
            shooting_get_drive_mode
            shooting_get_ev_correction1
            shooting_get_flash_mode
            shooting_get_focus_mode
            shooting_get_focus_ok
            shooting_get_focus_state
            shooting_get_hyperfocal_distance
            shooting_get_is_mode
            shooting_get_iso_market
            shooting_get_iso_mode
            shooting_get_iso_real
            shooting_get_max_av96
            shooting_get_min_av96
            shooting_get_prop
            shooting_get_real_focus_mode
            shooting_get_resolution
            shooting_get_subject_distance
            shooting_get_exif_subject_dist
            shooting_get_sv96_real
            shooting_get_tv96
            shooting_get_user_av96
            shooting_get_user_av_id
            shooting_get_user_tv96
            shooting_get_user_tv_id
            shooting_get_zoom
            shooting_in_progress
            shooting_is_flash
            shooting_mode_chdk2canon
            shooting_set_av96
            shooting_set_av96_direct
            shooting_set_focus
            shooting_set_iso_mode
            shooting_set_iso_real
            shooting_set_mode_canon
            shooting_set_mode_chdk
            shooting_set_nd_filter_state
            shooting_set_prop
            shooting_set_sv96
            shooting_set_tv96
            shooting_set_tv96_direct
            shooting_set_user_av96
            shooting_set_user_av_by_id
            shooting_set_user_av_by_id_rel
            shooting_set_user_tv96
            shooting_set_user_tv_by_id
            shooting_set_user_tv_by_id_rel
            shooting_set_zoom
            shooting_set_zoom_rel
            shooting_set_zoom_speed
            shooting_set_playrec_mode
            shooting_update_dof_values
            shooting_iso_market_to_real
            shooting_iso_real_to_market
            shooting_sv96_market_to_real
            shooting_sv96_real_to_market
            shooting_get_sv96_from_iso
            shooting_get_iso_from_sv96
            shooting_get_aperture_from_av96
            shooting_get_av96_from_aperture
            shooting_get_tv96_from_shutter_speed
            shooting_get_shutter_speed_from_tv96
            shooting_can_focus
            is_video_recording
            get_movie_status
            set_movie_status
            shooting_get_nd_value_ev96
            shooting_get_nd_current_ev96
            shooting_get_current_av96
            shooting_get_current_tv96
            shooting_get_current_delta_sv96
            shooting_get_current_base_sv96
            shooting_get_imager_active
            shooting_get_canon_image_format
            shooting_set_canon_image_format

            rbf_char_width
            rbf_font_height
            rbf_draw_char
            rbf_load_from_file
            rbf_str_width
            rbf_draw_menu_header
            rbf_draw_symbol
            rbf_draw_string_len
            rbf_draw_clipped_string
            rbf_draw_string

            GetMemInfo
            GetExMemInfo
            GetARamInfo
            GetCombinedMemInfo
            get_exmem_type_name
            get_exmem_type_status
            exmem_type_count

            img_prefixes
            img_exts
            GetTotalCardSpaceKb
            GetFreeCardSpaceKb
            get_exposure_counter
            get_target_dir_name
            get_target_file_num
            get_playback_image_number
            GetJpgCount
            GetRawCount
            is_raw_possible

            TurnOnBackLight
            TurnOffBackLight
            TurnOnDisplay
            TurnOffDisplay

            // Action stack functions
            action_stack_create
            action_pop_func
            action_top
            action_push
            action_push_func
            action_push_delay
            action_push_click
            action_push_press
            action_push_release
            action_push_shoot

            // Console functions
            console_clear
            console_add_line
            console_redraw
            console_set_autoredraw
            console_set_layout
            script_console_add_line
            script_console_add_error
            script_print_screen_statement

            get_focal_length
            get_effective_focal_length

            get_flash_params_count
            get_parameter_size
            get_parameter_data
            get_property_case
            set_property_case

            config_save
            config_restore
            conf_getValue
            conf_save
            conf_setValue
            conf_setAutosave
            save_config_file
            load_config_file

            DoAELock
            UnlockAE
            DoAFLock
            UnlockAF
            DoMFLock
            UnlockMF

            enter_alt
            exit_alt

            get_battery_temp
            get_ccd_temp
            get_optical_temp

            lens_get_zoom_point
            play_sound

            live_histogram_read_y

            stat_get_vbatt
            swap_partitions
            get_part_count
            get_part_type
            get_active_partition

            PostLogicalEventForNotPowerType
            PostLogicalEventToUI
            SetLogicalEventActive
            SetScriptMode
            errnoOfTaskGet

            levent_set_play
            levent_set_record
            levent_count
            levent_id_for_name
            levent_index_for_id
            levent_index_for_name
            levent_table

            script_set_terminate_key
            script_start_gui
            script_end

            reboot
            shutdown
            camera_shutdown_in_a_second

            get_usb_power
            start_usb_HPtimer
            stop_usb_HPtimer

            switch_mode_usb

            force_usb_state

            &usb_sync_wait_flag

            call_func_ptr
            _ExecuteEventProcedure

            ptp_script_create_msg
            ptp_script_write_msg
            ptp_script_read_msg
            ptp_script_write_error_msg

            CreateTask
            ExitTask

            SetHPTimerAfterNow
            CancelHPTimer

            remotecap_set_target
            remotecap_using_dng_module
            remotecap_set_timeout

            set_focus_bypass

            &firmware_crc_desc
            live_view_get_data

            TakeSemaphore
            GiveSemaphore
            CreateBinarySemaphore
            DeleteSemaphore
