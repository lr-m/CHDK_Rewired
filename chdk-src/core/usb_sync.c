/*===================================================================================================
    usb_sync.c
    - platform dependent code related to USB remote precision sync / button release

  ===================================================================================================*/

#include "camera.h"
#include "clock.h"
#include "modes.h"
#include "conf.h"
#include "usb_remote.h"
#include "script_api.h"
#include "shooting.h"

/*===================================================================================================
    Variables
  ===================================================================================================*/

extern int usb_sync_wait_flag ;
extern int usb_remote_active;
#ifdef USB_REMOTE_DEBUGGING  
int sync_counter=0;
#endif

/*---------------------------------------------------------------------------------------------------------

    force_usb_state()
        
      - causes camera to think the USB port is active ( i.e. 5V on the +V input pin)
      - enables the simultaneous use of the USB port for PTP communications and USB remote precision sync

  ---------------------------------------------------------------------------------------------------------*/

int forced_usb_port = 0 ;

int force_usb_state(int state)
{
    forced_usb_port = state ;
#ifdef CAM_ALLOWS_USB_PORT_FORCING
    return 1 ;
#else
    return 0 ;
#endif
}


/*---------------------------------------------------------------------------------------------------------

    get_remote_state()

    - return state of current remote input
  ---------------------------------------------------------------------------------------------------------*/

int get_remote_state()
{
#ifdef CAM_REMOTE_MULTICHANNEL
    switch(conf.remote_input_channel)
    {
        case REMOTE_INPUT_USB:
            return get_usb_bit();
#ifdef CAM_REMOTE_HDMI_HPD
        case REMOTE_INPUT_HDMI_HPD:
            return get_hdmi_hpd_bit();
#endif
#ifdef CAM_REMOTE_ANALOG_AV
        case REMOTE_INPUT_ANALOG_AV:
            return get_analog_av_bit();
#endif
#ifdef CAM_REMOTE_AtoD_CHANNEL
        case REMOTE_INPUT_AD_CHANNEL:
            return (GetAdChValue(CAM_REMOTE_AtoD_CHANNEL) < CAM_REMOTE_AtoD_THRESHOLD) ? 1 : 0;
#endif
    }
    return 0;
#else // not CAM_REMOTE_MULTICHANNEL
    return( get_usb_bit() );
#endif
}

/*---------------------------------------------------------------------------------------------------------

    wait_until_remote_button_is released()

    - called from capt_seq.c after all focus, exposure and flash things have been setup
    - if enabled,  waits for a USB 1->0 transition to allow accurate sync between cameras connected in parrallel

  ---------------------------------------------------------------------------------------------------------*/
      
#define GPIO_VSYNC_UPDATE  0xC0F06000
#define GPIO_VSYNC_MAX     0xC0F06014
// note : GPIO_VSYNC_CURRENT is camera dependent and define in platform_camera.h

void _wait_until_remote_button_is_released(void)
{
    int tick;

    // hook for script to block processing just prior to exposure start
    libscriptapi->shoot_hook(SCRIPT_SHOOT_HOOK_SHOOT);

    if ( usb_sync_wait_flag )                 // flag set when something wants the current shot to be sync'd
    {
        usb_remote_status_led(1);             // indicate to user we are waiting for remote button to release - this happens every time the camera takes a picture
        tick = get_tick_count();              // timestamp so we don't hang here forever if something goes wrong

    #ifdef CAM_REMOTE_USES_PRECISION_SYNC

        int std_period = EngDrvRead(GPIO_VSYNC_MAX);

        do { }  while( get_remote_state() &&  ((int)get_tick_count()-tick < DELAY_TIMEOUT));

        int cur_cnt = *(volatile int*)(GPIO_VSYNC_CURRENT) & 0xffff; // get the counter state at the time of sync
        
        int sync_period = std_period * 2 + cur_cnt;                  // schedule the end of extended period at t = t(synch pulse) + sync_time
        
        if (std_period - cur_cnt < 10)                               // if too close to overflow, wait for the next period
        {
            sync_period -= (std_period - cur_cnt);
            while ((*(volatile int*)(GPIO_VSYNC_CURRENT) & 0xffff) >= cur_cnt) {};
        }
        
        *(volatile int*)(GPIO_VSYNC_MAX) = sync_period; // write the length of the extended period to the register
        *(volatile int*)(GPIO_VSYNC_UPDATE) = 1;

        while (*(volatile int*)(GPIO_VSYNC_UPDATE)) {}; // wait until the new value is applied
        
        //now we are at the beginning of extended period
        
        *(volatile int*)(GPIO_VSYNC_MAX) = std_period; // back to standard timing on next period
        *(volatile int*)(GPIO_VSYNC_UPDATE) = 1;
        
        /* on s95 the std_period is 0x110
           1. if the shooting starts with GPIO_VSYNC_CURRENT value between 0 and 0xe1, it starts immediately
           2. if the shooting starts with value between 0xe1 and 0x110 (end of period), it waits for the next period
           
           now we want to go for the case 2
           msleep(40) should get us there, the timing is not critical and with sleep we give the camera a chance
           to run the delayed low prio tasks now
        */
        
        msleep(40); 
        
        // now we are in the second half of the extended period, shooting can start, it will wait for the end of the period

    #else // CAM_REMOTE_USES_PRECISION_SYNC
        // delay until USB state goes to "Off" or timeout

        do { }  while( get_remote_state() &&  ((int)get_tick_count()-tick < DELAY_TIMEOUT));

        // add a sync calibration delay if requested

        if ( conf.synch_delay_enable && conf.synch_delay_value>0 ) kbd_synch_delay( conf.synch_delay_value );

    #endif
    #ifdef USB_REMOTE_DEBUGGING   
        sync_counter++ ;
    #endif
        usb_sync_wait_flag = 0 ;
        usb_remote_status_led(0);       // alert the user that we are all done
    }

}
