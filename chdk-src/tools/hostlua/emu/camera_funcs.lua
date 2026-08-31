--[[
********************************
Licence: GPL
(c) 2009-2024 reyalp, rudi, msl
v 0.5.1
********************************
Add here virtual camera functions
for using with emu.lua.
********************************
]]

--helper function

local function on_off_value(value)
    if type(value) == "boolean" then
        return value and 1 or 0
    else
        return value
    end
end


local camera_funcs = {}

-- Lens Functions & Depth of Field
function camera_funcs.get_focus()
    return camera_state.focus
end

function camera_funcs.get_focus_mode()
    return camera_state.f_mode
end

function camera_funcs.get_focus_ok()
    return camera_state.focus_ok
end

function camera_funcs.get_focus_state()
    return camera_state.focus_state
end

function camera_funcs.get_IS_mode()
    return camera_state.IS_mode
end

function camera_funcs.get_zoom()
    return camera_state.zoom
end

function camera_funcs.get_zoom_steps()
    return camera_state.zoom_steps
end

function camera_funcs.get_sd_over_modes()
    return camera_state.sd_over_modes
end

function camera_funcs.set_mf(n)
    local _n = on_off_value(n)
    if _n == 1 then print(">MF on<") end
    if _n == 0 then print(">MF off<") end
    return camera_state.sd_over_modes >= 4 and 1 or 0
end

function camera_funcs.set_aflock(n)
    local _n = on_off_value(n)
    if _n == 1 then print(">aflock on<") end
    if _n == 0 then print(">aflock off<") end
end

function camera_funcs.set_focus(n)
    camera_state.focus=n
end

function camera_funcs.set_focus_interlock_bypass(n)
end

function camera_funcs.set_zoom(n)
    camera_state.zoom=n
end

function camera_funcs.set_zoom_rel(n)
    camera_state.zoom=camera_state.zoom+n
end

function camera_funcs.set_zoom_speed(n)
    print(">Zoom Speed<", n)
end

-- lookup tables for get_dofinfo() based on A720 zoom steps
local fl = {5800, 6420, 7059, 7701, 8336, 9954, 11546, 13159, 14745, 17152, 19574, 22763, 26749, 30750, 34800}
local efl = {35000, 38741, 42597, 46471, 50303, 60067, 69674, 79407, 88978, 103503, 118118, 137362, 161416, 185560, 210000}
local aperture = {2880, 2901, 2943, 2986, 3029, 3152, 3268, 3376, 3475, 3642, 3789, 4000, 4299, 4638, 4950}

local function div_add_round(val, div, add)
    --usage div_add_round(val, div) or div_add_round(val, div, add)
    local _round = val < 0 and -1 or 1
    return (val / (div / 2) + 2 * (add and add or 0) + _round) / 2
end

local function calc_hyp_dist(fl, av, coc) -- focal length, aperture, coc scaled 1000
    local min_stack = 65000 -- MAX_DIST
    local hyp_1e3 = imath.div(imath.mul(imath.div(fl, coc), fl), av) + fl + 500
    local hyp = hyp_1e3 / 1000
    if hyp > 0 then
        local v = div_add_round(hyp_1e3 - fl, 500, 1)
        if v > 0 then
            local q_1e3 = div_add_round(imath.muldiv(fl, fl -  hyp_1e3 - 1000, v), 1000)
            local q2_1e3 = div_add_round(imath.muldiv(hyp, 2 * fl - hyp_1e3, v), 1000)
            min_stack = (imath.sqrt((div_add_round(imath.mul(q_1e3, q_1e3), 1000) - q2_1e3) * 10) - q_1e3 / 10) / 100
        end
    end
    return hyp, min_stack
end

local function calc_nfd(fl, focus, hyp)
    local near, far, dof = -1, -1, -1
    if fl and focus and hyp and fl > 0 and focus > 0 and hyp > 0 then
        local hyp_1e3 = 1000 * hyp
        local focus_1e3 = 1000 * focus
        local m = imath.mul(focus, hyp_1e3 - fl)
        local vn = (hyp_1e3 - 2 * fl + focus_1e3 + 500)/ 1000
        local vf = (hyp_1e3 - focus_1e3 + 500)/ 1000
        if vn > 0  and vf > 0 then
            near, far = m / vn, m / vf
            dof  = far - near
        end
    end
    return near, far, dof
end

function camera_funcs.get_dofinfo()
    local _focus = camera_state.focus
    local _fl, _hyp_dist, _min_stack = nil, nil, nil
    local _near, _far, _dof = -1, -1, -1
    if #fl >= camera_state.zoom+1 then
        _fl = fl[camera_state.zoom+1]
        _hyp_dist, _min_stack = calc_hyp_dist(_fl, aperture[camera_state.zoom+1], camera_state.coc)
        _near, _far, _dof = calc_nfd(_fl, _focus, _hyp_dist)
    end

    return {
            hyp_valid = true,
            focus_valid = true,
            aperture = #aperture >= camera_state.zoom+1 and aperture[camera_state.zoom+1] or nil,
            coc = camera_state.coc,
            focal_length = _fl,
            eff_focal_length = #efl >= camera_state.zoom+1 and efl[camera_state.zoom+1] or nil,
            focus = _focus,
            hyp_dist = _hyp_dist,
            near = _near,
            far = _far,
            dof = _dof,
            min_stack_dist = _min_stack
    }
end

local function get_zoom_pos_arg(pos)
    if not pos then
        pos = camera_state.zoom
    end
    if pos < 0 then
        pos = 0
    elseif pos >= #fl then
        pos = #fl - 1
    end
    -- adjust to 1 based array
    pos = pos + 1
    return pos
end

function camera_funcs.get_focal_length(pos)
    return fl[get_zoom_pos_arg(pos)]
end

function camera_funcs.get_eff_focal_length(pos)
    return efl[get_zoom_pos_arg(pos)]
end

--Exposure Functions
function camera_funcs.get_av96()
    return camera_state.av96
end

function camera_funcs.get_bv96()
    return camera_state.bv96
end

-- live view Tv
function camera_funcs.get_current_av96()
    return camera_state.av96
end

-- live view Tv
function camera_funcs.get_current_tv96()
    return camera_state.tv96
end

-- live view ISO https://chdk.setepontos.com/index.php?topic=13508.20
function camera_funcs.get_current_base_sv96()
    return 795
end
function camera_funcs.get_current_delta_sv96()
    return 96
end

function camera_funcs.get_ev()
    return camera_state.ev96
end

function camera_funcs.get_iso_market()
    return camera_state.iso_market
end

function camera_funcs.get_iso_mode()
    return camera_state.iso_mode
end

function camera_funcs.get_iso_real()
    return camera_funcs.iso_market_to_real(camera_state.iso_market)
end

function camera_funcs.get_max_av96()
    return camera_state.max_av96
end

function camera_funcs.get_min_av96()
    return camera_state.min_av96
end

function camera_funcs.get_nd_present()
    return camera_state.nd_present
end

function camera_funcs.get_nd_value_ev96()
    if camera_state.nd_present > 0 then
        return camera_state.nd_ev96
    end
    return 0
end

function camera_funcs.get_nd_current_ev96()
    if camera_state.nd_present > 0 and camera_state.nd == 1 then
        return camera_state.nd_ev96
    end
    return 0
end

function camera_funcs.get_sv96()
    return camera_state.sv96
end

function camera_funcs.get_tv96()
    return camera_state.tv96
end

function camera_funcs.get_user_av_id()
    return camera_state.user_av_id
end

function camera_funcs.get_user_av96()
    return camera_state.av96
end

function camera_funcs.get_user_tv_id()
    return camera_state.user_tv_id
end

function camera_funcs.get_user_tv96()
    return camera_state.tv96
end

function camera_funcs.set_aelock(n)
    local _n = on_off_value(n)
    if _n == 1 then print(">aelock on<") end
    if _n == 0 then print(">aelock off<") end
end

function camera_funcs.set_av96(n)
    camera_state.av96=n
end

function camera_funcs.set_av96_direct(n)
    camera_state.av96=n
end

function camera_funcs.set_ev(n)
    camera_state.ev96=n
end

function camera_funcs.set_iso_mode(n)
    camera_state.iso_mode=n
end

function camera_funcs.set_iso_real(n)
    camera_state.iso_market=camera_funcs.sv96_real_to_market(n)
end

function camera_funcs.set_nd_filter(n)
    camera_state.nd=n
end

function camera_funcs.set_sv96(n)
    camera_state.sv96=n
end

function camera_funcs.set_tv96(n)
    camera_state.tv96=n
end

function camera_funcs.set_tv96_direct(n)
    camera_state.tv96=n
end

function camera_funcs.set_user_av_by_id(n)
    camera_state.user_av_id = n
end

function camera_funcs.set_user_av_by_id_rel(n)
    camera_state.user_av_id = camera_state.user_av_id + n
end

function camera_funcs.set_user_av96(n)
    camera_state.av96 = n
end

function camera_funcs.set_user_tv_by_id(n)
    camera_state.user_tv_id = n
end

function camera_funcs.set_user_tv_by_id_rel(n)
    camera_state.user_tv_id = camera_state.user_tv_id + n
end

function camera_funcs.set_user_tv96(n)
    camera_state.tv96 = n
end

function camera_funcs.get_live_histo()
    local res ={}
    for i=1, 255 do
        res[i] = 0
    end
    return res, 7200
end


--APEX96 Conversion
local ISOmarket={50,64,80,100,125,160,200,250,320,400,500,640,800,1000,1250,1600,2000,2500,3200,4000,5000,6400,8000,10000,12800}
local sv96market={382,419,450,481,511,545,577,607,641,672,703,737,768,799,830,864,895,926,960,991,1022,1087,1118,1152}
local ISOreal={30,39,49,61,76,97,122,152,194,243,304,389,486,608,760,972,1215,1519,1944,2430,3038,4861,6076,7777}
local sv96real={313,350,381,412,442,476,508,538,572,603,634,668,699,730,761,795,826,857,891,922,953,1018,1049,1083}

function camera_funcs.iso_to_sv96(val)
     local i = 1
     while i <= #ISOmarket and val > ISOmarket[i] do i=i+1 end
     return sv96real[i]
end

function camera_funcs.sv96_to_iso(val)
     local i = 1
     while i <= #sv96real and val > sv96real[i] do i=i+1 end
     return ISOreal[i]
end

function camera_funcs.iso_real_to_market(val)
     local i = 1
     while i <= #ISOreal and val > ISOreal[i] do i=i+1 end
     return ISOmarket[i]
end

function camera_funcs.iso_market_to_real(val)
     local i = 1
     while i <= #ISOmarket and val > ISOmarket[i] do i=i+1 end
     return ISOreal[i]
end

function camera_funcs.sv96_real_to_market(val)
     local i = 1
     while i <= #sv96real and val > sv96real[i] do i=i+1 end
     return sv96market[i]
end

function camera_funcs.sv96_market_to_real(val)
     local i = 1
     while i <= #sv96market and val > sv96market[i] do i=i+1 end
     return sv96real[i]
end

local aperture = {2000, 2200, 2500, 2800, 3200, 3500, 4000, 4500, 5000, 5600, 6300, 7100, 8000, 9000, 10000, 11000, 13000, 14000, 16000}
local av96 = {200, 224, 256, 288, 320, 352, 384, 416, 448, 480, 512, 544, 576, 608, 640, 672, 704, 736, 768}

function camera_funcs.aperture_to_av96(val)
     local i = 1
     while i <= #aperture and val > aperture[i] do i=i+1 end
     return av96[i]
end

function camera_funcs.av96_to_aperture(val)
     local i = 1
     while i <= #av96 and val > av96[i] do i=i+1 end
     return aperture[i]
end

function camera_funcs.tv96_to_usec(val)
    if val > 300 then
        return imath.div(1000000, imath.pow(2000, imath.div(val, 96)))
    else
        return imath.pow(2000, imath.div(-val, 96)) * 1000
    end
end

function camera_funcs.usec_to_tv96(val)
    if val == 0 then
        return 0
    elseif val > 100000 then
        return imath.round(-imath.log2(val / 1000) * 96) / 1000
    else
        return imath.round(imath.log2(imath.div(1000000, val)) * 96) / 1000
    end
end

function camera_funcs.seconds_to_tv96(val1, val2)
    return camera_funcs.usec_to_tv96(imath.muldiv(1000000, val1, val2))
end

--Camera Functions
function camera_funcs.get_canon_image_format()
    return camera_state.canon_image_format
end

function camera_funcs.get_canon_raw_support()
    return camera_state.canon_raw_support
end

function camera_funcs.get_curve_file()
    return camera_state.curve_file
end

function camera_funcs.get_curve_state()
    return camera_state.curve_state
end

function camera_funcs.get_drive_mode()
    return camera_state.drive
end

function camera_funcs.get_flash_mode()
    return camera_state.flash
end

function camera_funcs.get_imager_active()
    -- could hook up to display off
    return camera_state.rec
end

function camera_funcs.get_meminfo(s)
    if s == nil then s = "system" end
    local mem = {
    name                = "system",
    chdk_malloc         = true,
    chdk_start          = 643108,
    chdk_size           = 149148,
    start_address       = nil, -- pool start, not set for "combined"
    end_address         = nil, -- pool end, not set for "combined"
    total_size          = 1978624,
    allocated_size      = 1114216,
    allocated_peak      = 1147664,
    allocated_count     = 1761,
    free_size           = 850144,
    free_block_max_size = 822016,
    free_block_count    = 24
    }
    return s == "system" and mem or false
end

function camera_funcs.get_flash_params_count()
    return 300
end

function camera_funcs.get_flash_ready()
    return 1
end

function camera_funcs.get_movie_status()
    return camera_state.movie_state
end

function camera_funcs.get_orientation_sensor()
    return 0
end

function camera_funcs.get_parameter_data(id)
    return "",0
end

function camera_funcs.get_mode()
    return camera_state.rec, camera_state.vid, camera_state.mode
end

function camera_funcs.get_prop(prop)
    if camera_state.props[prop] then
        return camera_state.props[prop]
    end
    -- CHDK returns an aribtrary value for out of range
    return 0
end

function camera_funcs.get_prop_str(prop,n)
    local s
    if camera_state.props_str[prop] then
        s=camera_state.props_str[prop]
    -- if a string value isn't defined for prop, try numeric
    elseif camera_state.props[prop] then
        s = ''
        local v=camera_state.props[prop]
        for i=0,3 do
            local b=camera_funcs.bitand(camera_funcs.bitshru(v,8*i),0xff)
            s = s .. string.char(b)
        end
    end
    if not s then
        return false -- CHDK returns false for out of range
    end
    -- pad or trim to length
    if s:len() < n then
        s = s .. ('\0'):rep(n - s:len())
    elseif s:len() > n then
        s = s:sub(1,n)
    end
    return s
end

function camera_funcs.get_propset()
    return camera_state.propset
end

-- return 1 or 0, depending on system clock... a quick&dirty simulation for shooting process
-- TODO could / should base on shoot(), shoot_full etc
function camera_funcs.get_shooting()
    if os.date("%S")%3 == 1 then
        return true
    else
        return false
    end
end

function camera_funcs.get_temperature(n)
    return 40
end

function camera_funcs.get_vbatt()
    return camera_state.battmax
end

function camera_funcs.get_video_button()
    return camera_state.video_button
end

function camera_funcs.get_video_recording()
    return false
end

function camera_funcs.is_capture_mode_valid(n)
    return n > 0 and true or false
end

function camera_funcs.play_sound(n)
    print(">beep<", n)
end

function camera_funcs.reboot(s)
    print(">reboot cam<")
    if type(s) == "string" then print(">with<", s) end
end

function camera_funcs.set_canon_image_format(n)
    camera_state.canon_image_format = n
end

function camera_funcs.set_capture_mode_canon(n)
    print(">set canon mode<", n)
    return camera_state.rec
end

function camera_funcs.set_capture_mode(n)
    print(">set capture mode<", n)
    return camera_state.rec
end

function camera_funcs.set_clock(year, month, day, hour, minute, second)
end

function camera_funcs.set_curve_file(f)
    camera_state.curve_file = f
end

function camera_funcs.set_curve_state(n)
    camera_state.curve_state = n
end


function camera_funcs.set_led(n, s)
    if s == 1 then print(">LED", n, "on<") end
    if s == 0 then print(">LED", n, "off<") end
end

function camera_funcs.set_movie_status(n)
    camera_state.movie_state = n
    print(">movie status<", n)
end

function camera_funcs.set_prop(prop, val)
    camera_state.props[prop]=val
end

function camera_funcs.set_prop_str(prop,s)
    camera_state.props_str[prop] = s
    local n = 0
    -- update the numeric prop with the first 4 bytes of s
    for i, b in ipairs{s:byte(1,4)} do
        n = camera_funcs.bitor(n,camera_funcs.bitshl(b,8*(i-1)))
    end
    camera_state.props[prop] = n
    return true -- chdk func returns false for out of range
end

function camera_funcs.set_record(n)
    local _n = on_off_value(n)
    if _n == 1 then
        camera_state.rec = true
        print(">record mode<")
    elseif _n == 0 then
        camera_state.rec = false
        print(">play mode<")
    end
end

function camera_funcs.shut_down()
    print(">shutdown camera ...<")
end


--Keypad & Switches
function camera_funcs.click(s)
    print(">click<", s)
    if s == "shoot_full" or s == "shoot_full_only" then
        camera_state.exp_count = camera_state.exp_count + 1
    end
end

function camera_funcs.is_key()
    -- needs logic
end

function camera_funcs.is_pressed()
    -- needs logic
end

function camera_funcs.press(s)
    print(">press<", s)
    if s == "shoot_half" then
        camera_state.focus_ok = true
    end
end

function camera_funcs.release(s)
    print(">release<", s)
    if s == "shoot_full" or s == "shoot_full_only" then
        camera_state.exp_count = camera_state.exp_count + 1
    elseif s == "shoot_half" then
        camera_state.focus_ok = false
    end
end

function camera_funcs.shoot()
    if camera_state.raw > 0 then
        camera_state.tick_count = camera_state.tick_count + 3000
    else
        camera_state.tick_count = camera_state.tick_count + 1000
    end
    camera_state.exp_count = camera_state.exp_count + 1
    print(">Shoot<")
end

function camera_funcs.wait_click(n)
    print(">wait click<", n)
end

function camera_funcs.wheel_left()
    print(">wheel left<")
end

function camera_funcs.wheel_right()
    print(">wheel right<")
end

function camera_funcs.set_exit_key(s)
    print("exit key is:", s)
end


--SD Card Functions
function camera_funcs.get_disk_size()
    return camera_state.disk_size
end

function camera_funcs.get_exp_count()
    return camera_state.exp_count
end

function camera_funcs.get_image_dir()
    return camera_state.image_dir
end

function camera_funcs.file_browser(s)
    print(">startup dir:<", s)
    return ""
end

function camera_funcs.get_free_disk_space(n)
    return camera_state.free_disk_space
end

function camera_funcs.get_jpg_count()
    return camera_state.jpg_count
end

function camera_funcs.get_partitionInfo()
    return {count=1, active=1, type=6, size=camera_state.disk_size/1024/1024}
end

function camera_funcs.set_file_attributes(s,n)
    print(">Set file attributes for:<", s)
    print(">attribute value:<", n)
end

function camera_funcs.swap_partitions(n)
    print(">Swap to partition #:<", n)
end


--Script Status Functions
function camera_funcs.autostarted()
    return camera_state.autostarted
end

function camera_funcs.get_autostart()
    return camera_state.autostart
end

function camera_funcs.get_day_seconds()
    local H=os.date("%H")*60*60
    local M=os.date("%M")*60
    local S=os.date("%S")
    return H+M+S
end

function camera_funcs.get_tick_count()
    return camera_state.tick_count
end

function camera_funcs.get_time(c)
    if c=="D" then
        return os.date("%d")
    elseif c=="M" then
        return os.date("%m")
    elseif c=="Y" then
        return os.date("%Y")
    elseif c=="h" then
        return os.date("%H")
    elseif c=="m" then
        return os.date("%M")
    elseif c=="s" then
        return os.date("%S")
    else
        return 9999
    end
end

function camera_funcs.set_autostart(n)
    camera_state.autostart=n
end

function camera_funcs.set_yield(count, ms)
    local old_max_count = camera_state.yield_count
    local old_max_ms = camera_state.yield_ms
    local default_max_count = 25
    local default_max_ms = 10
    if count == nil then camera_state.yield_count = default_max_count else camera_state.yield_count = count end
    if ms == nil then camera_state.yield_ms = default_max_ms else camera_state.yield_ms = ms end
    return old_max_count, old_max_ms
end

function camera_funcs.sleep(n)
    camera_state.tick_count=camera_state.tick_count+n
end


--Firmware Interface
function camera_funcs.call_event_proc(...)
    return -1
end

function camera_funcs.call_func_ptr(...)
    return -1
end

function camera_funcs.get_levent_def(n)
    return nil
end

function camera_funcs.get_levent_index(n)
    return nil
end

function camera_funcs.get_levent_def_by_index(n)
    return nil
end

function camera_funcs.post_levent_to_ui(...)
    print(">levent to ui:<")
    print(...)
end

function camera_funcs.post_levent_for_npt(...)
    print(">levent for npt:<")
    print(...)
end

function camera_funcs.set_levent_active(...)
    print(">set levent active:<")
    print(...)
end

function camera_funcs.set_levent_script_mode(...)
    print(">set levent script mode:<")
    print(...)
end


--Display & Text Console
function camera_funcs.set_backlight(n)
    local _n = on_off_value(n)
    if _n == 1 then print(">Backlight on<") end
    if _n == 0 then print(">Backlight off<") end
end

function camera_funcs.set_lcd_display(n)
    local _n = on_off_value(n)
    if _n == 1 then print(">LCD Display on<") end
    if _n == 0 then print(">LCD Display off<") end
end

function camera_funcs.set_draw_title_line(n)
    local _n = on_off_value(n)
    if _n == 1 then print(">title line on<") camera_state.title_line = 1 end
    if _n == 0 then print(">title line off<") camera_state.title_line = 0 end
end

function camera_funcs.get_draw_title_line()
    return camera_state.title_line
end

function camera_funcs.cls()
    print(">delete screen<")
end

function camera_funcs.console_redraw()
    print(">redraw console<")
end

function camera_funcs.print_screen(n)
    local _n = on_off_value(n)
    if _n == 0 then
        print(">write log file off<")
    elseif _n == -1000 then
        print(">write log file append<")
    else
        print(">write log file on<")
    end
end

function camera_funcs.set_console_autoredraw(n)
    print(">redraw console<", n)
end

function camera_funcs.set_console_layout(n, m, o, p)
    print(">change console layout<", n, m, o, p)
end


--LCD Graphics
function camera_funcs.draw_string(x,y,s,c1,c2)
    print(string.format(">draw string x:%d y:%d Colors:%d %d Msg> %s<", x, y, c1, c2, s))
end

function camera_funcs.draw_pixel(x,y,cl)
    print(string.format(">draw pixel x:%d y:%d Color:%d<", x, y, cl))
end

function camera_funcs.draw_line(x1, y1, x2, y2, cl)
    print(string.format(">draw line x1:%d y1:%d x2:%d y2:%d Color:%d<", x1, y1, x2, y2, cl))
end

function camera_funcs.draw_rect(x1, y1, x2, y2, cl, th)
    print(string.format(">draw rect x1:%d y1:%d x2:%d y2:%d Color:%d<", x1, y1, x2, y2, cl))
end

function camera_funcs.draw_rect_filled(x1, y1, x2, y2, c1, c2, th)
    print(string.format(">draw filled rect x1:%d y1:%d x2:%d y2:%d Colors:%d %d<", x1, y1, x2, y2, c1, c2))
end

function camera_funcs.draw_ellipse(x, y, r1, r2, cl)
    print(string.format(">draw ellipse x:%d y:%d r1:%d r2:%d Color:%d<", x, y, r1, r2, cl))
end

function camera_funcs.draw_ellipse_filled(x, y, r1, r2, cl)
    print(string.format(">draw filled ellipse x:%d y:%d r1:%d r2:%d Color:%d<", x, y, r1, r2, cl))
end

function camera_funcs.draw_clear()
    print(">del drawings<")
end

function camera_funcs.get_gui_screen_width()
    return camera_state.screen_width
end

function camera_funcs.get_gui_screen_height()
    return camera_state.screen_height
end

function camera_funcs.textbox( t, m, d, l)
    print(">title:<", t)
    print(">message:<", m)
    print(">default string<", d)
    print(">max len:<", l)
    local res = ""
    if d ~= nil then res = d end
    return res
end

function camera_funcs.screen_needs_refresh()
    -- TODO might be better to have it be every N calls or something
    return false
end

--RAW
function camera_funcs.get_raw()
    return camera_state.raw == 1 and true or false
end

function camera_funcs.get_raw_count()
    return camera_state.raw_count
end

function camera_funcs.get_raw_nr()
    return camera_state.raw_nr
end

function camera_funcs.get_raw_support()
    -- TODO could vary depending on shooting mode
    return true
end

function camera_funcs.raw_merge_add_file(s)
    return print(">raw merge: add file:<", s)
end

function camera_funcs.raw_merge_end()
    return print(">raw merge: finish<")
end

function camera_funcs.raw_merge_start()
    return print(">raw merge: start<")
end

function camera_funcs.set_raw(n)
    local _n = on_off_value(n)
    if _n == 0 or _n == 1 then camera_state.raw = _n end
end

function camera_funcs.set_raw_develop(s)
    return print(">raw development:<", s)
end

function camera_funcs.set_raw_nr(n)
    camera_state.raw_nr = n
end


--CHDK Functionality
function camera_funcs.enter_alt()
    print(">alt mode on<")
    camera_state.alt_mode=true
end

function camera_funcs.exit_alt()
    print(">alt mode off<")
    camera_state.alt_mode=false
end

function camera_funcs.get_alt_mode()
    return camera_state.alt_mode
end

function camera_funcs.get_buildinfo()
    return {
            platform="PC",
            platsub="sub",
            version="PC Emulation CHDK",
            build_number="9.9.9",
            build_revision="9999",
            build_date=os.date("%x"),
            build_time=os.date("%X"),
            os=camera_state.camera_os,
            platformid=camera_state.platformid,
            digic=40,
    }
end

-- start get/set_config_value()--
CAM_SCREEN_WIDTH = camera_funcs.get_gui_screen_width()
CAM_SCREEN_HEIGHT = camera_funcs.get_gui_screen_height()
FONT_HEIGHT = 16
FONT_WIDTH = 8

-- fictional color values
IDX_COLOR_TRANSPARENT = 0
IDX_COLOR_BLACK = 1
IDX_COLOR_WHITE = 2
IDX_COLOR_RED = 3
IDX_COLOR_RED_DK = 4
IDX_COLOR_RED_LT = 5
IDX_COLOR_GREEN = 6
IDX_COLOR_GREEN_DK = 7
IDX_COLOR_GREEN_LT = 8
IDX_COLOR_BLUE = 9
IDX_COLOR_BLUE_DK = 10
IDX_COLOR_BLUE_LT = 11
IDX_COLOR_GREY = 12
IDX_COLOR_GREY_DK = 13
IDX_COLOR_GREY_LT = 14
IDX_COLOR_YELLOW = 15
IDX_COLOR_YELLOW_DK = 16
IDX_COLOR_YELLOW_LT = 17

cfg = {}
cfg[  1] = {newID=2001, oldID=  1, value=1}
cfg[  2] = {newID=1020, oldID=  2, value=0}
cfg[  3] = {newID=1050, oldID=  3, value=0}
cfg[  4] = {newID=1060, oldID=  4, value=0}
cfg[  5] = {newID=1052, oldID=  6, value=0}
cfg[  6] = {newID=2150, oldID=  7, value=0}
cfg[  7] = {newID=2100, oldID=  8, value=camera_state.battmax}
cfg[  8] = {newID=2101, oldID=  9, value=camera_state.battmin}
cfg[  9] = {newID=2102, oldID= 11, value=1}
cfg[ 10] = {newID=2103, oldID= 12, value=0}
cfg[ 11] = {newID=2104, oldID= 13, value=1}
cfg[ 12] = {newID=2090, oldID= 14, value=1}
cfg[ 13] = {newID=2160, oldID= 15, value=0}
cfg[ 14] = {newID=1061, oldID= 16, value=1}
cfg[ 15] = {newID=1062, oldID= 17, value=0}
cfg[ 16] = {newID=1063, oldID= 18, value=1}
cfg[ 17] = {newID=1064, oldID= 19, value=4}
cfg[ 18] = {newID=1065, oldID= 20, value=0}
cfg[ 19] = {newID=2020, oldID= 21, value={45, 150}}
cfg[ 20] = {newID=2021, oldID= 22, value={490, 45}}
cfg[ 21] = {newID=2022, oldID= 23, value={178, 0}}
cfg[ 22] = {newID=2023, oldID= 24, value={178, FONT_HEIGHT}}
cfg[ 23] = {newID=2024, oldID= 25, value={35, 0}}
cfg[ 24] = {newID=2025, oldID= 26, value={CAM_SCREEN_WIDTH-9*FONT_WIDTH,30}}
cfg[ 25] = {newID=2050, oldID= 27, value={IDX_COLOR_GREY_DK,IDX_COLOR_WHITE}}
cfg[ 26] = {newID=2052, oldID= 28, value={IDX_COLOR_GREY_DK_TRANS,IDX_COLOR_WHITE}}
cfg[ 27] = {newID=2055, oldID= 30, value={IDX_COLOR_GREY_DK,IDX_COLOR_WHITE}}
cfg[ 28] = {newID=2059, oldID= 31, value={IDX_COLOR_GREY,IDX_COLOR_WHITE}}
cfg[ 29] = {newID=1207, oldID= 32, value=0}
cfg[ 30] = {newID=1226, oldID= 33, value=0}
cfg[ 31] = {newID=2230, oldID= 34, value=0}
cfg[ 32] = {newID=1021, oldID= 35, value=1}
cfg[ 33] = {newID=1022, oldID= 36, value=1}
cfg[ 34] = {newID=1023, oldID= 37, value=1}
cfg[ 35] = {newID=2110, oldID= 38, value="A/CHDK/BOOKS/README.TXT"}
cfg[ 36] = {newID=2111, oldID= 39, value=0}
cfg[ 37] = {newID=2080, oldID= 41, value=2}
cfg[ 38] = {newID=2026, oldID= 42, value={CAM_SCREEN_WIDTH-5*FONT_WIDTH-2,0}}
cfg[ 39] = {newID=2112, oldID= 43, value=0}
cfg[ 40] = {newID=2113, oldID= 44, value=5}
cfg[ 41] = {newID=2114, oldID= 45, value=""}
cfg[ 42] = {newID=2115, oldID= 46, value=0}
cfg[ 43] = {newID=2120, oldID= 47, value=1}
cfg[ 44] = {newID=2051, oldID= 48, value={IDX_COLOR_RED,IDX_COLOR_WHITE}}
cfg[ 45] = {newID=1070, oldID= 49, value=0}
cfg[ 46] = {newID=1071, oldID= 50, value=1}
cfg[ 47] = {newID=1072, oldID= 51, value=1}
cfg[ 48] = {newID=1073, oldID= 52, value=1}
cfg[ 49] = {newID=1074, oldID= 53, value=1}
cfg[ 50] = {newID=1075, oldID= 54, value=0}
cfg[ 51] = {newID=2062, oldID= 55, value={IDX_COLOR_RED,IDX_COLOR_RED}}
cfg[ 52] = {newID=1076, oldID= 56, value=1}
cfg[ 53] = {newID=3002, oldID= 57, value=0}
cfg[ 54] = {newID=2175, oldID= 58, value=0}
cfg[ 55] = {newID=1222, oldID= 59, value=0}
cfg[ 56] = {newID=1024, oldID= 60, value=0}
cfg[ 57] = {newID=2116, oldID= 61, value=1}
cfg[ 58] = {newID=2130, oldID= 62, value=1}
cfg[ 59] = {newID=1220, oldID= 63, value=12}
cfg[ 60] = {newID=2135, oldID= 64, value=""}
cfg[ 61] = {newID=2136, oldID= 65, value=2}
cfg[ 62] = {newID=2131, oldID= 66, value=""}
cfg[ 63] = {newID=1221, oldID= 67, value=1}
cfg[ 64] = {newID=2140, oldID= 68, value=0}
cfg[ 65] = {newID=2141, oldID= 69, value=""}
cfg[ 66] = {newID=1039, oldID= 70, value=0}
cfg[ 67] = {newID=2142, oldID= 71, value=0}
cfg[ 68] = {newID=2060, oldID= 72, value={IDX_COLOR_GREY_DK,IDX_COLOR_WHITE}}
cfg[ 69] = {newID=2151, oldID= 80, value=0}
cfg[ 70] = {newID=2152, oldID= 81, value=0}
cfg[ 71] = {newID=2153, oldID= 82, value=1}
cfg[ 72] = {newID=2154, oldID= 83, value=1}
cfg[ 73] = {newID=2155, oldID= 84, value=1}
cfg[ 74] = {newID=2156, oldID= 85, value=1}
cfg[ 75] = {newID=2157, oldID= 86, value=0}
cfg[ 76] = {newID=2161, oldID= 87, value=0}
cfg[ 77] = {newID=2162, oldID= 88, value=1}
cfg[ 78] = {newID=2163, oldID= 89, value=0}
cfg[ 79] = {newID=2164, oldID= 90, value=1}
cfg[ 80] = {newID=2165, oldID= 91, value=0}
cfg[ 81] = {newID=2166, oldID= 92, value=0}
cfg[ 82] = {newID=2167, oldID= 93, value=0}
cfg[ 83] = {newID=2168, oldID= 94, value=0}
cfg[ 84] = {newID=2169, oldID= 95, value=0}
cfg[ 85] = {newID=2170, oldID= 96, value=0}
cfg[ 86] = {newID=2171, oldID= 97, value=0}
cfg[ 87] = {newID=2172, oldID= 98, value=0}
cfg[ 88] = {newID=1080, oldID= 99, value=0}
cfg[ 89] = {newID=1081, oldID=100, value=84}
cfg[ 90] = {newID=1082, oldID=101, value=3}
cfg[ 91] = {newID=1112, oldID=102, value=0}
cfg[ 92] = {newID=1110, oldID=103, value=0}
cfg[ 93] = {newID=1130, oldID=105, value=0}
cfg[ 94] = {newID=1131, oldID=106, value=0}
cfg[ 95] = {newID=1140, oldID=107, value=0}
cfg[ 96] = {newID=1141, oldID=108, value=0}
cfg[ 97] = {newID=1150, oldID=109, value=0}
cfg[ 98] = {newID=1151, oldID=110, value=0}
cfg[ 99] = {newID=1152, oldID=111, value=0}
cfg[100] = {newID=1153, oldID=112, value=0}
cfg[101] = {newID=1154, oldID=113, value=0}
cfg[102] = {newID=1155, oldID=114, value=0}
cfg[103] = {newID=1156, oldID=115, value=0}
cfg[104] = {newID=1113, oldID=116, value=0}
cfg[105] = {newID=1114, oldID=117, value=0}
cfg[106] = {newID=1120, oldID=118, value=0}
cfg[107] = {newID=1999, oldID=119, value=0}
cfg[108] = {newID=1053, oldID=120, value=0}
cfg[109] = {newID=1200, oldID=121, value=0}
cfg[110] = {newID=2173, oldID=122, value=0}
cfg[111] = {newID=1100, oldID=123, value=1}
cfg[112] = {newID=2002, oldID=124, value=0}
cfg[113] = {newID=1157, oldID=126, value=1}
cfg[114] = {newID=1077, oldID=127, value=0}
cfg[115] = {newID=1003, oldID=128, value=0}
cfg[116] = {newID=1066, oldID=129, value=0}
cfg[117] = {newID=2053, oldID=130, value={IDX_COLOR_GREY_DK_TRANS,IDX_COLOR_RED}}
cfg[118] = {newID=2061, oldID=131, value={IDX_COLOR_GREY_DK_TRANS,IDX_COLOR_WHITE}}
cfg[119] = {newID=2180, oldID=132, value=0}
cfg[120] = {newID=2027, oldID=133, value={CAM_SCREEN_WIDTH-100,0}}
cfg[121] = {newID=2181, oldID=134, value=0}
cfg[122] = {newID=2182, oldID=135, value=1}
cfg[123] = {newID=2028, oldID=136, value={128,0}}
cfg[124] = {newID=2200, oldID=137, value=1}
cfg[125] = {newID=2029, oldID=138, value={CAM_SCREEN_WIDTH-7*FONT_WIDTH-2,CAM_SCREEN_HEIGHT-3*FONT_HEIGHT-2}}
cfg[126] = {newID=2201, oldID=139, value=1}
cfg[127] = {newID=2174, oldID=140, value=0}
cfg[128] = {newID=1111, oldID=141, value=0}
cfg[129] = {newID=3003, oldID=142, value=0}
cfg[130] = {newID=3001, oldID=143, value=0}
cfg[131] = {newID=2176, oldID=144, value=100}
cfg[132] = {newID=2183, oldID=145, value=1}
cfg[133] = {newID=2184, oldID=146, value=1}
cfg[134] = {newID=2030, oldID=147, value={CAM_SCREEN_WIDTH-7,0}}
cfg[135] = {newID=2031, oldID=148, value={0,CAM_SCREEN_HEIGHT-7}}
cfg[136] = {newID=2185, oldID=149, value=2}
cfg[137] = {newID=2186, oldID=150, value=10}
cfg[138] = {newID=2187, oldID=151, value=20}
cfg[139] = {newID=2188, oldID=152, value=0}
cfg[140] = {newID=2202, oldID=153, value=0}
cfg[141] = {newID=1086, oldID=154, value=1}
cfg[142] = {newID=2081, oldID=155, value=0}
cfg[143] = {newID=2082, oldID=156, value=0}
cfg[144] = {newID=2083, oldID=157, value=1}
cfg[145] = {newID=1160, oldID=158, value=0}
cfg[146] = {newID=1161, oldID=159, value=5}
cfg[147] = {newID=1162, oldID=160, value=5}
cfg[148] = {newID=1163, oldID=161, value=2}
cfg[149] = {newID=1164, oldID=162, value=550}
cfg[150] = {newID=1165, oldID=163, value=320}
cfg[151] = {newID=1166, oldID=164, value=50}
cfg[152] = {newID=2056, oldID=165, value={IDX_COLOR_WHITE,IDX_COLOR_BLACK}}
cfg[153] = {newID=2057, oldID=166, value={IDX_COLOR_BLUE,IDX_COLOR_YELLOW}}
cfg[154] = {newID=2134, oldID=167, value=1}
cfg[155] = {newID=1084, oldID=168, value=0}
cfg[156] = {newID=1033, oldID=169, value=0}
cfg[157] = {newID=1085, oldID=170, value=0}
cfg[158] = {newID=2054, oldID=171, value={IDX_COLOR_GREY_DK_TRANS,IDX_COLOR_RED}}
cfg[159] = {newID=2003, oldID=172, value=0}
cfg[160] = {newID=2004, oldID=173, value=1}
cfg[161] = {newID=2005, oldID=174, value=1}
cfg[162] = {newID=1025, oldID=175, value=1}
cfg[163] = {newID=2210, oldID=176, value=3}
cfg[164] = {newID=2211, oldID=177, value=1}
cfg[165] = {newID=2032, oldID=178, value={CAM_SCREEN_WIDTH-25*FONT_WIDTH-2,CAM_SCREEN_HEIGHT-6*FONT_HEIGHT-2}}
cfg[166] = {newID=1083, oldID=179, value=0}
cfg[167] = {newID=1087, oldID=180, value=0}
cfg[168] = {newID=1088, oldID=181, value=1}
cfg[169] = {newID=2033, oldID=182, value={CAM_SCREEN_WIDTH-40*FONT_WIDTH-2,CAM_SCREEN_HEIGHT-8*FONT_HEIGHT-2}}
cfg[170] = {newID=2132, oldID=183, value="A/CHDK/SYMBOLS/icon_10.rbf"}
cfg[171] = {newID=2058, oldID=184, value={IDX_COLOR_GREY_DK,IDX_COLOR_WHITE}}
cfg[172] = {newID=1180, oldID=185, value=""}
cfg[173] = {newID=1181, oldID=186, value=0}
cfg[174] = {newID=1190, oldID=187, value=0}
cfg[175] = {newID=1191, oldID=188, value=60}
cfg[176] = {newID=2063, oldID=189, value=IDX_COLOR_RED}
cfg[177] = {newID=1201, oldID=190, value=0}
cfg[178] = {newID=1202, oldID=191, value=0}
cfg[179] = {newID=1203, oldID=192, value=100}
cfg[180] = {newID=1054, oldID=194, value=""}
cfg[181] = {newID=2234, oldID=195, value=0x1000}
cfg[182] = {newID=1026, oldID=196, value=0}
cfg[183] = {newID=1027, oldID=197, value=0}
cfg[184] = {newID=1028, oldID=198, value=0}
cfg[185] = {newID=1029, oldID=199, value=0}
cfg[186] = {newID=2203, oldID=200, value=1}
cfg[187] = {newID=2133, oldID=201, value=1}
cfg[188] = {newID=1089, oldID=202, value=0}
cfg[189] = {newID=2220, oldID=203, value=1}
cfg[190] = {newID=2034, oldID=204, value={CAM_SCREEN_WIDTH-9*FONT_WIDTH-2,FONT_HEIGHT}}
cfg[191] = {newID=1090, oldID=205, value=0}
cfg[192] = {newID=2121, oldID=208, value=0}
cfg[193] = {newID=1041, oldID=209, value=2}
cfg[194] = {newID=1042, oldID=210, value=1}
cfg[195] = {newID=2231, oldID=213, value=0}
cfg[196] = {newID=1055, oldID=214, value=1}
cfg[197] = {newID=2035, oldID=215, value={18, 80}}
cfg[198] = {newID=1040, oldID=219, value=0}
cfg[199] = {newID=2221, oldID=220, value=0}
cfg[200] = {newID=1192, oldID=222, value=0}
cfg[201] = {newID=1193, oldID=223, value=0}
cfg[202] = {newID=1194, oldID=224, value=1}
cfg[203] = {newID=1030, oldID=225, value=1}
cfg[204] = {newID=1036, oldID=226, value=1}
cfg[205] = {newID=1210, oldID=227, value=0}
cfg[206] = {newID=1034, oldID=228, value=0}
cfg[207] = {newID=1001, oldID=229, value=camera_state.platformid}
cfg[208] = {newID=1031, oldID=230, value=0}
cfg[209] = {newID=1032, oldID=231, value=0}
cfg[210] = {newID=1211, oldID=232, value=0}
cfg[211] = {newID=1212, oldID=233, value=0}
cfg[212] = {newID=1035, oldID=234, value=1}
cfg[213] = {newID=1038, oldID=235, value=0}
cfg[214] = {newID=1213, oldID=236, value=0}
cfg[215] = {newID=1002, oldID=237, value=3}
cfg[216] = {newID=1224, oldID=238, value=0}
cfg[217] = {newID=2232, oldID=239, value=0}
cfg[218] = {newID=2233, oldID=240, value=0}
cfg[219] = {newID=1195, oldID=241, value=0}
cfg[220] = {newID=1196, oldID=242, value=0}
cfg[221] = {newID=1197, oldID=243, value=30}
cfg[222] = {newID=2240, oldID=244, value=0}
cfg[223] = {newID=2241, oldID=245, value=0}
cfg[224] = {newID=2250, oldID=246, value=0}
cfg[225] = {newID=2036, oldID=247, value={95, 0}}
cfg[226] = {newID=1204, oldID=248, value=0}
cfg[227] = {newID=1205, oldID=249, value=0}
cfg[228] = {newID=1206, oldID=250, value=0}
cfg[229] = {newID=2189, oldID=251, value=0}
cfg[230] = {newID=1091, oldID=252, value=0}
cfg[231] = {newID=4001, oldID=253, value=0}
cfg[232] = {newID=4002, oldID=254, value=0}
cfg[233] = {newID=4003, oldID=255, value=0}
cfg[234] = {newID=4004, oldID=256, value=0}
cfg[235] = {newID=4005, oldID=257, value=0}
cfg[236] = {newID=4006, oldID=258, value=0}
cfg[237] = {newID=4007, oldID=259, value=1}
cfg[238] = {newID=4008, oldID=260, value=0}
cfg[239] = {newID=4009, oldID=261, value=300}
cfg[240] = {newID=4010, oldID=262, value=1}
cfg[241] = {newID=4011, oldID=263, value=1}
cfg[242] = {newID=4012, oldID=264, value=5}
cfg[243] = {newID=4013, oldID=265, value=7}
cfg[244] = {newID=4014, oldID=266, value=25}
cfg[245] = {newID=4015, oldID=267, value=0}
cfg[246] = {newID=4016, oldID=268, value=2}
cfg[247] = {newID=4017, oldID=269, value=1}
cfg[248] = {newID=4018, oldID=270, value=0}
cfg[249] = {newID=4019, oldID=271, value=0}
cfg[250] = {newID=4020, oldID=272, value=30}
cfg[251] = {newID=4021, oldID=273, value=45}
cfg[252] = {newID=4022, oldID=274, value=0}
cfg[253] = {newID=4023, oldID=275, value=0}
cfg[254] = {newID=4024, oldID=276, value=10}
cfg[255] = {newID=4025, oldID=277, value=15}
cfg[256] = {newID=4026, oldID=278, value=0}
cfg[257] = {newID=4027, oldID=279, value=0}
cfg[258] = {newID=4029, oldID=280, value=0}
cfg[259] = {newID=4030, oldID=281, value=0}
cfg[260] = {newID=4031, oldID=282, value=0}
cfg[261] = {newID=1170, oldID=283, value=0}
cfg[262] = {newID=1171, oldID=284, value=600}
cfg[263] = {newID=1172, oldID=285, value=1}
cfg[264] = {newID=1173, oldID=286, value=5}
cfg[265] = {newID=1174, oldID=287, value=0}
cfg[266] = {newID=1223, oldID=288, value=0}
cfg[267] = {newID=1037, oldID=289, value=0}
cfg[268] = {newID=2137, oldID=290, value=0}
cfg[269] = {newID=2260, oldID=291, value=1}
cfg[270] = {newID=2261, oldID=292, value=3}
cfg[271] = {newID=2263, oldID=293, value=0}
cfg[272] = {newID=1225, oldID=294, value=0}
cfg[273] = {newID=2262, oldID=295, value=1}
cfg[274] = {newID=2270, oldID=296, value=0}
cfg[275] = {newID=2271, oldID=297, value=3}
cfg[276] = {newID=1230, oldID=298, value=0}
cfg[277] = {newID=1231, oldID=299, value=0}
cfg[278] = {newID=1214, oldID=300, value=9}
cfg[279] = {newID=1215, oldID=301, value=0}

function camera_funcs.get_config_value(id)
    local res = nil
    for i=1, #cfg do
        if type(id) == "number" then
            if id < 1000 then
                if cfg[i].oldID == id then
                    res = cfg[i].value
                    break
                end
            elseif id > 999 then
                if cfg[i].newID == id then
                    res = cfg[i].value
                    break
                end
            end
        end
    end
    if res ~= nil and type(res) == "table" and #res == 2 then
        return res[1], res[2]
    else
        return res
    end
end

function camera_funcs.set_config_value(id, val1, val2)
    local val
    if val2 == nil then
        val = val1
    else
        val = {val1, val2}
    end
    local res = false
    for i=1, #cfg do
        if type(id) == "number" then
            if id < 1000 then
                if cfg[i].oldID == id then
                    if type(cfg[i].value) == "number" and type(val) == "number" then
                        cfg[i].value = val
                        res = true
                        break
                    elseif type(cfg[i].value) == "table" and type(val) == "table" then
                        cfg[i].value = val
                        res = true
                        break
                    elseif type(cfg[i].value) == "string" and type(val) == "string" then
                        cfg[i].value = val
                        res = true
                        break
                    end
                end
            elseif id > 999 then
                if cfg[i].newID == id then
                    if type(cfg[i].value) == "number" and type(val) == "number" then
                        cfg[i].value = val
                        res = true
                        break
                    elseif type(cfg[i].value) == "table" and type(val) == "table" then
                        cfg[i].value = val
                        res = true
                        break
                    elseif type(cfg[i].value) == "string" and type(val) == "string" then
                        cfg[i].value = val
                        res = true
                        break
                    end
                end
            end
        end
    end
    return res
end
-- end get/set_config_value()--

function camera_funcs.set_config_autosave(n)
    local _n = on_off_value(n)
end

function camera_funcs.save_config_file(id, file)
    print(string.format(">save cfg id='%s', file='%s'<",id, file or ">default<"))
    return false
end

function camera_funcs.load_config_file(id, file)
    print(string.format(">load cfg id='%s', file='%s'<",id, file or ">default<"))
    return false
end

function camera_funcs.get_histo_range(n1,n2)
    return camera_state.histo_range
end

function camera_funcs.shot_histo_enable(n)
    local _n = on_off_value(n)
end

function camera_funcs.shot_histo_write_to_file()
    print(">write histo to file<")
end

--Programming
-- chdk bit routines for lua 5.1
local function to32bits(value)
    if type(value) ~= "number" then
        error "wrong argument in bit functions"
    end
    local bittab, neg = {}, (value < 0)
    local val = (neg) and -(value + 1) or value
    for i=1, 32 do
       bittab[i] = (val % 2 == 1) and true or false
       val = val / 2
    end
    if neg then
        for i = 1, #bittab do bittab[i] = not(bittab[i]) end
    end
    return bittab
end

local function from32bits(bittab)
    local res = 0
    for i=#bittab, 1, -1 do
        res = 2 * res + (bittab[i] and 1 or 0)
    end
    return res
end

function camera_funcs.bitand(v1, v2)
    local v1t = to32bits(v1)
    local v2t = to32bits(v2)
    for i = 1, #v1t do v1t[i] = (v1t[i] and v2t[i]) end
    return from32bits(v1t)
end

function camera_funcs.bitor(v1, v2)
    local v1t = to32bits(v1)
    local v2t = to32bits(v2)
    for i = 1, #v1t do v1t[i] = (v1t[i] or v2t[i]) end
    return from32bits(v1t)
end

function camera_funcs.bitxor(v1, v2)
    local v1t = to32bits(v1)
    local v2t = to32bits(v2)
    for i = 1, #v1t do v1t[i] = (v1t[i] ~= v2t[i]) end
    return from32bits(v1t)
end

function camera_funcs.bitshl(v1, shift)
    local v1t = to32bits(v1)
    for i = #v1t, 1, -1 do
        v1t[i] = (i - shift > 0) and v1t[i - shift] or false
    end
    return from32bits(v1t)
end

function camera_funcs.bitshri(v1, shift)
    local v1t = to32bits(v1)
    for i = 1, #v1t - 1 do
        v1t[i] = v1t[(i + shift <= #v1t) and i + shift or #v1t]
    end
    return from32bits(v1t)
end

function camera_funcs.bitshru(v1, shift)
    local v1t = to32bits(v1)
    for i = 1, #v1t do
        v1t[i] = (i + shift <= #v1t) and v1t[i + shift] or false
    end
    return from32bits(v1t)
end

function camera_funcs.bitnot(v1)
    local v1t = to32bits(v1)
    for i = 1, #v1t do v1t[i] = not(v1t[i]) end
    return from32bits(v1t)
end

function camera_funcs.peek(n)
    return 0
end

function camera_funcs.poke(address,value,size)
    local s=""
    if size ~= nil then s = "size: "..size end
    print("poke adress: "..address.." value "..value..s)
end


--Motion Detection
function camera_funcs.md_detect_motion()
    return 1
end

function camera_funcs.md_get_cell_diff(col, row)
    return 0
end

function camera_funcs.md_get_cell_val(col, row)
    return 0
end

function camera_funcs.md_af_led_control(d, t)

end

--PTP
function camera_funcs.read_usb_msg(timeout)
    print((">read_usb_msg(%s)<"):format(tostring(timeout or '')))
    return nil
end

function camera_funcs.write_usb_msg(msg,timeout)
    print((">write_usb_msg(%s,%s)<"):format(tostring(msg),tostring(timeout or '')))
    return true
end

function camera_funcs.get_usb_capture_support()
    if camera_state.canon_raw_support then
        return 0xf
    end
    return 7
end

function camera_funcs.get_usb_capture_target()
    return camera_state.usb_capture_target
end

function camera_funcs.init_usb_capture(n)
    if camera_funcs.bitand(n,camera_funcs.get_usb_capture_support()) == n then
        camera_state.usb_capture_target = n
        return true
    end
    return false
end

function camera_funcs.set_usb_capture_timeout(n)
end

--USB Port Interface
function camera_funcs.get_usb_power()
    return 0
end

function camera_funcs.force_analog_av(n)
end

function camera_funcs.set_remote_timing(n)
    if type(n) == "number" and n <= 10000 then
        camera_state.remote_timing = n
    end
end

function camera_funcs.switch_mode_usb(n)
    local _n = on_off_value(n)
    if _n == 1 then
        camera_state.rec = true
        print(">record mode<")
    elseif _n == 0 then
        camera_state.rec = false
        print(">play mode<")
    end
end

function camera_funcs.usb_force_active(n)
end

function camera_funcs.usb_sync_wait(n)
    local _n = on_off_value(n)
    if _n == 1 then
        camera_state.usb_sync = true
        print(">usb sync mode ON<")
    elseif _n == 0 then
        camera_state.usb_sync= false
        print(">usb sync mode OFF<")
    end
end

--Tone Curves


--Shooting Hooks
camera_funcs.hook_preshoot={}
camera_funcs.hook_shoot={}
camera_funcs.hook_raw={}

function camera_funcs.hook_preshoot.set(timeout)

end

function camera_funcs.hook_preshoot.is_ready()
    return true
end

function camera_funcs.hook_preshoot.continue()

end

function camera_funcs.hook_preshoot.count()
    return 1000
end

function camera_funcs.hook_shoot.set(timeout)

end

function camera_funcs.hook_shoot.is_ready()
    return true
end

function camera_funcs.hook_shoot.continue()

end

function camera_funcs.hook_shoot.count()
    return 1000
end

function camera_funcs.hook_raw.set(timeout)

end

function camera_funcs.hook_raw.is_ready()
    return true
end

function camera_funcs.hook_raw.continue()

end

function camera_funcs.hook_raw.count()
    return 1000
end

-- Rawop
camera_funcs.rawop={}

function camera_funcs.rawop.get_cfa()
    return camera_state.rawop_cfa
end

function camera_funcs.rawop.get_cfa_offsets()
    local cfa=camera_funcs.rawop.get_cfa()
    local t={ }
    for y=0,1 do
        for x=0,1 do
            local n=camera_funcs.bitand(cfa,0xff)
            local c=({'r','g1','b'})[n+1]
            if not c then
                error(('invalid color index %d in rawop_cfa %x'):format(n,cfa))
            end
            if t[c] then
                if c == 'g1' then
                    c = 'g2'
                else
                    error(('dupe color %s in rawop_cfa %x'):format(c,cfa))
                end
            end
            t[c] = {x=x,y=y}
            cfa = camera_funcs.bitshru(cfa,8)
        end
    end
    return t
end

function camera_funcs.rawop.get_bits_per_pixel()
    return camera_state.rawop_bits_per_pixel
end

function camera_funcs.rawop.get_raw_neutral()
    -- empirical value from rawhookops.c
    local bl=camera_funcs.rawop.get_black_level()
    return ((camera_funcs.rawop.get_white_level() - bl)*1000)/6669 + bl
end

function camera_funcs.rawop.get_black_level()
    -- typical, some cameras vary
    return camera_funcs.bitshl(1,camera_state.rawop_bits_per_pixel-5) - 1
end

function camera_funcs.rawop.get_white_level()
    return camera_funcs.bitshl(1,camera_state.rawop_bits_per_pixel) - 1
end

function camera_funcs.rawop.get_raw_width()
    return camera_state.rawop_raw_width
end

function camera_funcs.rawop.get_raw_height()
    return camera_state.rawop_raw_height
end

function camera_funcs.rawop.get_active_left()
    return camera_state.rawop_active_left
end

function camera_funcs.rawop.get_active_top()
    return camera_state.rawop_active_top
end

function camera_funcs.rawop.get_active_width()
    return camera_state.rawop_active_width
end

function camera_funcs.rawop.get_active_height()
    return camera_state.rawop_active_height
end

function camera_funcs.rawop.get_jpeg_left()
    return camera_state.rawop_jpeg_left
end

function camera_funcs.rawop.get_jpeg_top()
    return camera_state.rawop_jpeg_top
end

function camera_funcs.rawop.get_jpeg_width()
    return camera_state.rawop_jpeg_width
end

function camera_funcs.rawop.get_jpeg_height()
    return camera_state.rawop_jpeg_height
end

-- real pixel access functions error if not in raw hook or raw not available
function camera_funcs.rawop.get_pixel(x,y)
    if x < 0 or x >= camera_funcs.rawop.get_raw_width()
        or y < 0 or y >= camera_funcs.rawop.get_raw_width() then
        return nil
    end
    return camera_funcs.rawop.get_raw_neutral()
end

function camera_funcs.rawop.set_pixel(x,y,v)
end

function camera_funcs.rawop.get_pixels_rgbg(x,y)
    if x < 0 or x >= camera_funcs.rawop.get_raw_width()
        or y < 0 or y >= camera_funcs.rawop.get_raw_width() then
        return nil
    end
    local v=camera_funcs.rawop.get_raw_neutral()
    return v,v,v,v
end

function camera_funcs.rawop.set_pixels_rgbg(x,y,r,g1,b,g2)
end

function camera_funcs.rawop.fill_rect(x,y,width,height,val,xstep,ystep)
end

function camera_funcs.rawop.meter(x,y,x_count,y_count,x_step,y_step)
    -- real function returns nil on various invalid arguments
    if x_count == 0 or y_count == 0 then
        return
    end
    local x_max = x + x_step * x_count
    local y_max = y + y_step * y_count
    if x_max > camera_funcs.rawop.get_raw_width() then
        return
    end
    if y_max > camera_funcs.rawop.get_raw_height() then
        return
    end

    if x_count * y_count > camera_funcs.bitshru(0xFFFFFFFF,camera_funcs.rawop.get_bits_per_pixel()) then
        return
    end

    return camera_funcs.rawop.get_raw_neutral()
end

function camera_funcs.rawop.raw_to_ev(rawval,scale)
    if not scale then
        scale = 96
    end
    local bl = camera_funcs.rawop.get_black_level()
    if rawval <= bl then
        rawval = bl + 1
    end
    -- assume we have fmath
    local r=scale*(fmath.new(rawval - bl):log2() - fmath.new(camera_funcs.rawop.get_raw_neutral() - bl):log2())
    return r:round()
end

function camera_funcs.rawop.ev_to_raw(ev,scale)
    if not scale then
        scale = 96
    end
    local bl = camera_funcs.rawop.get_black_level()
    return (2^(fmath.new(ev)/scale + fmath.new(camera_funcs.rawop.get_raw_neutral() - bl):log2()) + bl):round()
end

local histo_methods = {}
local histo_meta = { __index = histo_methods }

function histo_methods:update(top,left,width,height,xstep,ystep,bits)
    local bpp = camera_funcs.rawop.get_bits_per_pixel()
    if not bits then
        bits = bpp
    elseif bits > bpp or bits < 1 then
        error('invalid bit depth')
    end
    local shift = bpp - bits
    local entries = camera_funcs.bitshl(1 ,bits)

    if left >= camera_funcs.rawop.get_raw_width()
        or top >= camera_funcs.rawop.get_raw_height()
        or xstep == 0 or ystep == 0
        or width == 0 or height == 0 then
        error('invalid update area')
    end
    local xmax = left+width
    local ymax = top+height
    if xmax > camera_funcs.rawop.get_raw_width() then
        xmax = camera_funcs.rawop.get_raw_width()
    end
    if ymax > camera_funcs.rawop.get_raw_height() then
        ymax = camera_funcs.rawop.get_raw_height()
    end
    local total_pixels = ((1+(xmax - left - 1)/xstep))*((1+(ymax - top - 1)/ystep))

    self._params = {
        top=top,
        left=left,
        width=width,
        height=height,
        xstep=xstep,
        ystep=ystep,
        bits=bits,
        shift=shift,
        entries=entries,
        total_pixels=total_pixels,
    }
    -- make a table with full data range so tests could fill as they please and re-use other functions
    self._data={}
    for i=0,entries-1 do
        self._data[i]=0
    end
    -- put all at raw neutral
    self._data[camera_funcs.bitshru(camera_funcs.rawop.get_raw_neutral(),shift)] = total_pixels
end

function histo_methods:range(min,max,scale)
    if not self._params then
        error('no data')
    end
    if not scale then
        scale = 1000
    elseif type(scale) == 'number' then
        if scale == 0 then
            error('invalid format')
        end
    elseif type(scale) == 'string' then
        if scale ~= 'count' then
            error('invalid format')
        end
    else
        error('invalid format')
    end
    if max >= self._params.entries or min > max then
        error('invalid range')
    end
    local count = 0
    for i=min,max do
        count = count + self._data[i]
    end
    if scale == 'count' then
        return count
    end
    return ((scale*fmath.new(count))/self._params.total_pixels):round()
end

function histo_methods:total_pixels()
    if not self._params then
        error('no data')
    end
    return self._params.total_pixels
end

function histo_methods:bits()
    if not self._params then
        error('no data')
    end
    return self._params.bits
end


function histo_methods:free()
    self._params = nil
    self._data = nil
end

function camera_funcs.rawop.create_histogram()
    local h={}
    setmetatable(h,histo_meta)
    return h
end

return camera_funcs
