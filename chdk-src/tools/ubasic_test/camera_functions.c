#include <stdio.h>
#include <math.h>
#include <time.h>

static int tv=0, av=0, zoom=0, focus=0, iso=0, raw=0, raw_nr=0,autostart=0;
int movie_status=0;
int shot_histogram_enabled=0;
int state_kbd_script_run=1;
int zoom_points = 3;
#define MODE_REC                0x0100
#define AS_SHOOT 1
#define CONF_VALUE 2
int script_keyid_by_name(const char *s) {
	return 1;
}

void console_set_autoredraw(int n) {
    printf("*** console_set_auto_redraw %d ***\n",n);
}

void action_push_press(int k)
{
    printf("*** button press %d ***\n",k);
}

void action_push_release(int k)
{
    printf("*** button release %d ***\n",k);
}

void action_push_click(int k)
{
    printf("*** button click %d ***\n",k);
}

void action_push_delay(int v)
{
    printf("*** sleep %d ***\n",v);
}

void action_push(int id)
{
	if(id == AS_SHOOT)
		printf("*** shoot ***\n");
	else
		printf("*** action_push(%d) ***\n",id);
}

void action_wait_for_click(int t)
{
    printf("*** wait_click %d ***\n", t);
}

int script_key_is_clicked(int k)
{
    printf("*** is_clicked %d ***\n", k);
    return 1;
}

int shooting_get_tv()
{
    return tv;
}

void shooting_set_tv(int v)
{
    tv = v;
    printf("*** set tv %d ***\n",v);
}

void shooting_set_tv_rel(int v)
{
    tv += v;
    printf("*** set tv rel %d ***\n",v);
}

int shooting_get_av()
{
    return av;
}

void shooting_set_av(int v)
{
    av = v;
    printf("*** set av %d ***\n",v);
}

void shooting_set_av_rel(int v)
{
    av += v;
    printf("*** set av rel %d ***\n",v);
}

int shooting_get_zoom()
{
    return zoom;
}

void shooting_set_zoom(int v)
{
    zoom = v;
    printf("*** set zoom %d ***\n",v);
}

void shooting_set_zoom_rel(int v)
{
    zoom += v;
    printf("*** set zoom rel %d ***\n",v);
}

void shooting_set_zoom_speed(int v)
{
    printf("*** set zoom speed %d ***\n",v);
}

int shooting_get_focus()
{
    return focus;
}

void shooting_set_focus(int v)
{
    focus = v;
    printf("*** set focus %d ***\n",v);
}

int shooting_get_iso()
{
    return iso;
}

void shooting_set_iso(int v)
{
    iso = v;
    printf("*** set iso %d ***\n",v);
}
//ARM Begin
int shooting_get_iso_real()
{
  return iso;
}
//ARM End
void script_console_add_line(const char *str) {
    printf(">>> %s\n", str);
}

void console_set_layout(int x1, int y1, int x2, int y2) {
    printf(">>> set console layout to %d %d %d %d\n", x1,y1,x2,y2);
}

void script_console_set_autoredraw(int value) {
    printf(">>> set console auto_redraw to %d\n", value);
}

void console_redraw() {
    printf("*** console redraw ***\n");
}

void console_clear() {
    printf("*** clear console ***\n");
}
int md_detect_motion(void)
{
    printf("*** md_detect_motion ***\n");
	return 0;
}
int md_get_cell_diff(int column, int row)
{
    printf("*** md_get_sell_diff %d %d ***\n", column, row);
	return 0;
}
int md_get_cell_val(int column, int row)
{
    printf("*** md_get_sell_abs %d %d ***\n", column, row);
	return 0;
}
int md_init_motion_detector()
{
    printf("*** md_init_motion_detector ***\n");
	return 0;
}
void camera_set_nr(to)
{
	raw_nr = to;
    printf("*** set raw nr %d ***\n",raw_nr);
};

int camera_get_nr(to)
{
    printf("*** get raw nr ***\n");
	return raw_nr;
};

void camera_set_raw(int mode)
{
	raw = mode;
    printf("*** set raw %d ***\n",raw);
}

int ubasic_camera_get_raw()
{
	return raw;
}
void shooting_set_prop(int id, int v)
{
    printf("*** set prop %d %d ***\n",id, v);
}
int shooting_get_prop(int id)
{
	return 0;
}
long stat_get_vbatt()
{
	return 4085;
}
void camera_set_led(int led, int state, int bright)
{
    printf("*** set led %d %d %d ***\n",led, state, bright);
}

int camera_get_script_autostart()
{
	return 0;
}
void camera_set_script_autostart(int state)
{
	autostart = state;
    printf("*** set autostart %d ***\n",autostart);
}
int get_usb_power(int edge)
{
	return 0;
}
int GetTotalCardSpaceKb()
{
	return 0;
}
int GetFreeCardSpaceKb()
{
	return 0;
}

int GetJpgCount()
{
    printf("*** GetJpgCount ***\n");
    return 0;
}

int GetRawCount()
{
    printf("*** GetRawCount ***\n");
    return 0;
}

void exit_alt()
{
    printf("*** exit alt ***\n");

}

void shooting_set_user_tv_by_id(int v)
{
    printf("*** shooting_set_user_tv_by_id %d ***\n", v);
}

void shooting_set_user_tv_by_id_rel(int v)
{
    printf("*** shooting_set_user_tv_by_id_rel %d ***\n", v);
}

void shooting_set_sv96(short sv96, short is_now)
{
    printf("*** shooting_set_sv96 %d %d ***\n", sv96, is_now);
}

short shooting_get_av96()
{
    printf("*** shooting_get_av96 ***\n");
    return 0;
}

short shooting_get_user_av96()
{
    printf("*** shooting_get_user_av96 ***\n");
    return 0;
}

int shooting_get_user_av_id()
{
    printf("*** shooting_get_user_av_id ***\n");
    return 0;
}

void shooting_set_av96(short v,short is_now)
{
    printf("*** shooting_set_av96 %d %d ***\n", v, is_now);
}

void shooting_set_av96_direct(short v, short is_now)
{
    printf("*** shooting_set_av96_direct %d %d ***\n", v, is_now);
}

void shooting_set_user_av96(short v)
{
    printf("*** shooting_set_user_av96 %d ***\n", v);
}

void shooting_set_user_av_by_id(int v)
{
    printf("*** shooting_set_user_av_by_id %d ***\n", v);
}

void shooting_set_user_av_by_id_rel(int v)
{
    printf("*** shooting_set_user_av_by_id_rel %d ***\n", v);
}

int shooting_get_subject_distance()
{
    printf("*** shooting_get_subject_distance ***\n");
    return 1000;
}

int shooting_get_hyperfocal_distance()
{
    printf("*** shooting_get_hyperfocal_distance ***\n");
    return 1000;
}

int mode_get()
{
    printf("*** mode_get ***\n");
    return MODE_REC;
}

short shooting_get_focus_mode()
{
    printf("*** shooting_get_focus_mode ***\n");
    return 0;
}

short shooting_get_focus_state()
{
    printf("*** shooting_get_focus_state ***\n");
    return 0;
}

short shooting_get_focus_ok()
{
    printf("*** shooting_get_focus_ok ***\n");
    return 0;
}

short shooting_get_iso_market()
{
    printf("*** shooting_get_iso_market ***\n");
    return 100;
}

short shooting_get_bv96()
{
    printf("*** shooting_get_bv96 ***\n");
    return 32;
}

short shooting_get_sv96_real()
{
    printf("*** shooting_get_sv96 ***\n");
    return 32;
}

void shooting_set_iso_real(short iso, short is_now)
{
    printf("*** shooting_set_iso_real %d %d ***\n", iso, is_now);
}

int shooting_get_iso_mode()
{
    printf("*** shooting_get_iso_mode ***\n");
    return 0;
}

void shooting_set_iso_mode(int v)
{
    printf("*** shooting_set_iso_mode %d ***\n", v);
}

void shooting_set_nd_filter_state(short v, short is_now)
{
    printf("*** shooting_set_nd_filter_state %d %d ***\n", v, is_now);
}

void camera_shutdown_in_a_second()
{
    printf("*** camera_shutdown_in_a_second ***\n");
}

short shooting_get_tv96()
{
    printf("*** shooting_get_tv96 ***\n");
    return 0;
}

short shooting_get_user_tv96()
{
    printf("*** shooting_get_user_tv96 ***\n");
    return 0;
}

int shooting_get_user_tv_id()
{
    printf("*** shooting_get_user_tv_id ***\n");
    return 0;
}

void shooting_set_tv96(short v, short is_now)
{
    printf("*** shooting_set_tv96 %d %d ***\n", v, is_now);
}

void shooting_set_tv96_direct(short v, short is_now)
{
    printf("*** shooting_set_tv96_direct %d %d ***\n", v, is_now);
}

void shooting_set_user_tv96(short v)
{
    printf("*** shooting_set_user_tv96 %d ***\n", v);
}

int get_exposure_counter(void)
{
    printf("*** get_exposure_counter ***\n");
	return 123;
}

int script_key_is_pressed( int keyid )
{
    printf("*** script_key_is_pressed %d ***\n", keyid);
	return 0;
}

int shot_histogram_get_range(int from, int to)
{
    printf("*** shot_histogram_get_range %d,%d ***\n", from,to);
	return 100;
}

void script_print_screen_statement(int v)
{
    printf("*** script_print_screensatement %d ***\n", v);
}

int get_tick_count(void)
{
    printf("*** get_tick_count ***\n");
	// what should it return ?
	return 10;
}

short shooting_get_drive_mode()
{
    printf("*** shooting_get_drive_mode ***\n");
	// what should it return ?
	return 10;
}

int shot_histogram_isenabled()
{
    printf("*** shot_histogram_isenabled ***\n");
	return shot_histogram_enabled;
}

int get_ccd_temp()
{
    printf("*** get_ccd_temp ***\n");
	return 69;
}
int get_optical_temp()
{
    printf("*** get_optical_temp ***\n");
	return 69;
}

int get_battery_temp()
{
    printf("*** get_battery_temp ***\n");
	return 69;
}

void play_sound()
{
    printf("*** play_sound ***\n");
}

void TurnOnBackLight()
{
    printf("*** TurOnBackLight ***\n");
}

void TurnOffBackLight()
{
    printf("*** TurnOffBackLight ***\n");
}

void set_draw_title_line(int m)
{
   printf("*** set CHDK OSD title line ( %d ) ***\n",m);
   return ;
}

void get_draw_title_line(int m)
{
   printf("*** get CHDK OSD title line state ***\n");
   return ;
}

void DoAFLock()
{
    printf("*** DoAFLock ***\n");
}

void UnlockAF()
{
    printf("*** UnlockAF ***\n");
}

int shot_histogram_set(int x)
{
    printf("*** shot_histogram_set ***\n");
	return 1;
}

void levent_set_record()
{
    printf("*** levent_set_record ***\n");
}

void levent_set_play()
{
    printf("*** levent_set_play ***\n");
}

int set_capture_mode(int x)
{
    printf("*** set_capture_mode %d ***\n",x);
	return 1;
}

int shooting_set_mode_chdk(int x)
{
    printf("*** set_capture_mode_canon %d ***\n",x);
	return 1;
}

int shooting_set_mode_canon(int x)
{
    printf("*** set_capture_mode_canon %d ***\n",x);
	return 1;
}

int shooting_mode_chdk2canon(int chdkmode)
{
    printf("*** shooting_mode_chdk2canon %d ***\n",chdkmode);
	if(chdkmode < 1 || chdkmode > 50)
		return 0;
	return 1;
}

int conf_getValue(unsigned short id, int* configVal) {
    printf("*** get config value %d ***\n",id);
    *configVal = 1;
    return CONF_VALUE;
}

int conf_setValue(unsigned short id, int configVal) {
    printf("*** set config value %d %d***\n",id, configVal);
    return CONF_VALUE;
}

void reboot(const char *filename)
{
    printf("*** reboot %s ***\n",filename?filename:"NULL");
}

int mode_is_video(int m)
{
    printf("*** mode is video %d ***\n",m);
    return 0;
}

int swap_partitions(int new)
{
    printf("*** swap_partitions %d ***\n",new);
    return 1;
}

void shooting_update_dof_values()
{
    printf("*** shooting_update_dof_values ***\n");
}

short shooting_sv96_real_to_market(short isor)
{
    printf("*** shooting_sv96_real_to_market %d ***\n",isor);
    return isor;
}

short shooting_sv96_market_to_real(short sv96)
{
    printf("*** shooting_sv96_market_to_real %d ***\n",sv96);
    return sv96;
}

void shooting_set_playrec_mode(int mode)
{
    printf("*** shooting_set_playrec_mode %d ***\n",mode);
}

short shooting_iso_real_to_market(short isor)
{
    printf("*** shooting_iso_real_to_market %d ***\n",isor);
    return isor;
}

short shooting_iso_market_to_real(short isom)
{
    printf("*** shooting_iso_market_to_real %d ***\n",isom);
    return isom;
}

int shooting_is_flash()
{
    printf("*** shooting_is_flash ***\n");
    return 0;
}

int shooting_in_progress()
{
    printf("*** shooting_in_progress ***\n");
    return 0;
}

static const double inv_log_2 = 1.44269504088906;   // 1 / log_2
#define SHOOTING_TV96_INVALID -10000

short shooting_get_tv96_from_shutter_speed(float t)
{
    printf("*** shooting_get_tv96_from_shutter_speed %f ***\n",t);
    if (t > 0)
    {
        t = ((96.0 * -log(t)) * inv_log_2);
        if (t < 0)
            return (short)(t - 0.5);
        return (short)(t + 0.5);
    }
    return SHOOTING_TV96_INVALID;
}

short shooting_get_sv96_from_iso(short iso)
{
    printf("*** shooting_get_sv96_from_iso %d ***\n",iso);
    // Equivalent to (short)(log2(iso/3.125)*96+0.5) [APEX equation]
    if (iso > 0)
        return (short)( log((double)(iso)*32.0/100.0)*96.0*(inv_log_2)+0.5 );
    return 0;
}

float shooting_get_shutter_speed_from_tv96(short tv96)
{
    printf("*** shooting_get_shutter_speed_from_tv96 %d ***\n",tv96);
    return pow(2,((float)(-tv96))/96.0);
}

short shooting_get_resolution()
{
    printf("*** shooting_get_resolution ***\n");
    return 0;
}

short shooting_get_real_focus_mode()
{
    printf("*** shooting_get_real_focus_mode ***\n");
    return 0;
}

short shooting_get_nd_value_ev96()
{
    printf("*** shooting_get_nd_value_ev96 ***\n");
    return 0;
}

short shooting_get_nd_current_ev96()
{
    printf("*** shooting_get_nd_current_ev96 ***\n");
    return 0;
}

short shooting_get_min_av96()
{
    printf("*** shooting_get_min_av96 ***\n");
    return -1;
}

short shooting_get_max_av96()
{
    printf("*** shooting_get_max_av96 ***\n");
    return -1;
}

short shooting_get_iso_from_sv96(short sv96)
{
    printf("*** shooting_get_shutter_speed_from_tv96 %d ***\n",sv96);
    // APEX equation --> (int)(POWER(2,(iso/96)) * 3.125) + 0.5)
    return (short)( (double)pow(2, (((double)sv96)/96.0))*100.0/32.0 + 0.5 );
}

short shooting_get_is_mode()
{
    printf("*** shooting_get_is_mode ***\n");
    return 0;
}

int shooting_get_imager_active() {
    printf("*** shooting_get_imager_active ***\n");
    return 0;
}

short shooting_get_flash_mode()
{
    printf("*** shooting_get_flash_mode ***\n");
    return 0;
}

short shooting_get_ev_correction1()
{
    printf("*** shooting_get_ev_correction1 ***\n");
    return 0;
}

short shooting_get_display_mode()
{
    printf("*** shooting_get_display_mode ***\n");
    return 0;
}

#define SHOOTING_TV96_INVALID -10000

long shooting_get_current_tv96()
{
    printf("*** shooting_get_current_tv96 ***\n");
    return SHOOTING_TV96_INVALID;
}

long shooting_get_current_av96()
{
    printf("*** shooting_get_current_av96 ***\n");
    return 0;
}

long shooting_get_current_base_sv96()
{
    printf("*** shooting_get_current_base_sv96 ***\n");
    return 0;
}

long shooting_get_current_delta_sv96()
{
    printf("*** shooting_get_current_delta_sv96 ***\n");
    return 0;
}

short shooting_get_av96_from_aperture(int aperture)
{
    printf("*** shooting_get_av96_from_aperture %d ***\n", aperture);
    return (int)((log((double)aperture/1000.0) * 192 * inv_log_2) + 0.5);
}

int shooting_get_aperture_from_av96(short av96)
{
    printf("*** shooting_get_aperture_from_av96 %d ***\n", av96);
    if (av96)
        return (short)((pow(2, ((double)av96)/192.0))*1000.0 + 0.5);
    return -1;
}

short shooting_can_focus()
{
    printf("*** shooting_can_focus ***\n");
    return 0;
}

int shooting_get_canon_image_format()
{
    printf("*** shooting_get_canon_image_format ***\n");
    return 0;
}

int shooting_set_canon_image_format(int fmt)
{
    printf("*** shooting_set_canon_image_format %d ***\n",fmt);
    return 0;
}


int shooting_get_canon_raw_support()
{
    printf("*** shooting_get_canon_raw_support ***\n");
    return 0;
}

void set_movie_status(int status)
{
    printf("*** set_movie_status %d ***\n", status);
}

int sd_over_modes()
{
    printf("*** sd_over_modes ***\n");
    return 0;
}

void script_set_terminate_key(int key, const char *keyname)
{
    printf("*** script_set_terminate_key %d %s ***\n", key, keyname);
}

void script_console_add_error(long str_id)
{
    printf("*** script_console_add_error %ld ***\n", str_id);
}

long lens_get_zoom_point()
{
    printf("*** lens_get_zoom_point ***\n");
    return 0;
}

int is_video_recording()
{
    printf("*** is_video_recording ***\n");
    return 0;
}

int is_raw_possible()
{
    printf("*** is_raw_possible ***\n");
    return 0;
}

int get_movie_status()
{
    printf("*** get_movie_status ***\n");
    return 0;
}

struct tm *get_localtime()
{
    printf("*** get_localtime ***\n");
    time_t t = time(NULL);
    return localtime(&t);
}

int get_focal_length(int zp)
{
    printf("*** get_focal_length  %d ***\n", zp);
    return 0;
}

int force_usb_state(int state)
{
    printf("*** force_usb_state  %d ***\n", state);
    return 0;
}

int kbd_force_analog_av(int state)
{
    printf("*** force_analog_av  %d ***\n", state);
    return 0;
}


void enter_alt(int script_mode)
{
    printf("*** enter_alt  %d ***\n", script_mode);
}

long action_top(int n)
{
    printf("*** action_top  %d ***\n", n);
    return 0;
}

long action_pop_func(int nParam)
{
    printf("*** action_pop_func  %d ***\n", nParam);
    return 0;
}

typedef int (*action_func)(void);

void action_push_func(action_func f)
{
    printf("*** action_push_func  %ld ***\n", (long)f);
}

void action_push_shoot(int retry)
{
    printf("*** action_push_shoot  %d ***\n", retry);
}

void DoAELock()
{
    printf("*** DoAELock ***\n");
}

void UnlockAE()
{
    printf("*** UnlockAE ***\n");
}

int DoMFLock()
{
    printf("*** DoMFLock ***\n");
    return 0;
}

int UnlockMF()
{
    printf("*** UnlockMF ***\n");
    return 0;
}

void JogDial_CW()
{
    printf("*** JogDial_CW ***\n");
}

void JogDial_CCW()
{
    printf("*** JogDial_CW ***\n");
}

void TurnOnDisplay()
{
    printf("*** TurnOnDisplay ***\n");
}

void TurnOffDisplay()
{
    printf("*** TurnOffDisplay ***\n");
}
