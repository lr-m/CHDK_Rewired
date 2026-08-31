--[[
********************************
Licence: GPL
(c) 2009-2024 reyalp, rudi, msl, fbonomi
v 0.5.1
********************************

http://chdk.setepontos.com/index.php?topic=2929.0
based on dummy.lua by fbonomi

Script to run CHDK lua scripts, emulating as much of the camera as required in lua
Should be used with lua from "hostlua" in the CHDK source tree
--]]
local usage_string=[[
usage: lua emu.lua <chdk_script> [options] [menu params]
 <chdk_script> is the script to be tested
options:
 -conf=<conf_script> is a lua script customizing the test case
 -modroot=<cam module root> sets directory for emulated require to look for
  SCRIPTS and LUALIB
 -loadparams=<file> load script settings from CHDK saved settings file, as found
  in CHDK/DATA/<scriptname>.N
 -saveparams=<file> save final params after loadparams and command line values
menu params:
 Script menu parameters can be overridden using paramname=<value>, where <value>
 is a number in decimal or hex (with 0x) or true or false.
 Table and enum values can be set using paramname.value=<string> where <string>
 is a valid item in the table / enum
 for backward compatibility, single letter menu options can also be set like -a=X
 menu params specified on the command line override values from the -loadparams
]]

-- global environment seen by chdk_script
local camera_env={
}
-- copy the global environment, before we add our own globals
for k,v in pairs(_G) do
    camera_env[k]=v;
end

-- load modules after camera env to avoid inadvertently adding globals
local cms=require'chdkmenuscript'
local util=require'util'
local fsutil=require'fsutil'
-- make warnings go to stdout, npp in hostluaPortable puts stderr after normal output
cms.warnf=function(format,...)
    util.fprintf(util.util_stdout,"WARNING: " .. format,...)
end


-- table to track state used by emulated camera functions
-- global so it's accessible to camera funcs
camera_state={
    tick_count      = 0,                -- sleep, tick count
    exp_count       = 1000,             -- current image number (IMG_1000.JPG)
    image_dir       = "A/DCIM/100CANON",-- path of the recent image dir
    jpg_count       = 1000,             -- jpg image counter
    raw             = 0,                -- raw status
    raw_count       = 100,              -- raw image counter
    raw_nr          = 0,                -- noise reduction status (0 = Auto 1 = OFF, 2 = ON)
    canon_raw_support = true,           -- camera with native raw support
    canon_image_format = 1,             -- Canon format setting 1 = jpeg, 2 = raw, 3 = raw+jpg
    curve_file      = '',               -- curve file
    curve_state     = 0 ,               -- curve mode
    mem             = {},               -- peek and poke
    av96            = 320,              -- F3.2
    bv96            = 388,              -- tv96+av96-sv96
    tv96            = 576,              -- 1/60s
    sv96            = 508,              -- ISO200
    iso_market      = 200,              -- Canon ISO value
    iso_mode        = 2,                -- number of ISO mode
    ev96            = 0,                -- exposure correction (APEX96)
    nd              = 0,                -- ND filter status
    nd_present      = 2,                -- ND filter present (0 = no, 1 = ND, 2 = iris + ND)
    nd_ev96         = 288,              -- ND value, 3 stops is typical
    min_av96        = 192,              -- Min Av (F2.0)
    max_av96        = 664,              -- Max Av (F11.0)
    disk_size       = 1024*1024*1024,   -- value in byte
    free_disk_space = 1000000,          -- value in byte
    propset         = 4,                -- propset 1 - 6
    rec             = true,             -- status of capture mode (false = play mode)
    vid             = false,            -- status of video mode
    mode            = 258,              -- P mode
    drive           = 0,                -- 0 = single 1 = continuous 2 = timer (on Digic 2) 3 = timer (on Digic 3,4,5)
    video_button    = true,             -- camera has extra video button
    movie_state     = 0,                -- 0 or 1 = movie recording is stopped or paused, 4 = recording, 5 or 6 = recording has stopped, writing movie to SD card
    flash           = 0,                -- 0 = flash auto, 1 = flash on, 2 = flash off
    coc             = 5,                -- coc x 1000
    focus           = 2000,             -- subject distance
    sd_over_modes   = 0x04,             -- 0x01 AutoFocus mode, 0x02  AFL active, 0x04  MF active
    f_mode          = 0,                -- focus mode, 0=auto, 1=MF, 3=inf., 4=macro, 5=supermacro
    focus_state     = 1,                -- focus ok = 1, not ok = 0, MF < 0
    focus_ok        = false,            -- can be true after release/click "shoot_half"
    zoom_steps      = 15,               -- maximum zoomsteps
    zoom            = 0,                -- zoom position
    IS_mode         = 0,                -- 0 = continuous, 1 or 2 = shoot only, 2 or 3 = panning, 3 or 4 = off
    user_av_id      = 1,                -- av id for Av and M mode
    user_tv_id      = 1,                -- tv id for Tv and M mode
    histo_range     = 100,              -- return value of a histogram range
    autostarted     = 0,                -- 0 = false, 1 = true
    autostart       = 0,                -- autostart status (0 = off, 1 = on, 2 = once)
    alt_mode        = true,             -- alte mode status
    usb_sync        = false,            -- usb sync mode for mulitcam sync
    yield_count     = 25,               -- maximum number of lua VM instructions
    yield_ms        = 10,               -- maximum number of milliseconds
    props           = {},               -- propcase values
    props_str       = {},               -- abitrary byte propcase values for the prop_str functions, not initialized by default
    title_line      = 1,                -- CHDK line 1 = on, 0 = off
    remote_timing   = 0,                -- value for high precision USB remote timing
    camera_os       = "dryos",          -- camera OS (vxworks or dryos)
    platformid      = 0xDEADBEEF,       -- platform ID of the camera
    battmax         = 3000,             -- maximum battery value
    battmin         = 2300,             -- minimum battery value
    screen_width    = 360,              -- LCD screen width (360 or 480 px)
    screen_height   = 240,              -- LCD screen heigth
    usb_capture_target = 0,             -- remotecap target
    rawop_cfa       = 0x2010100,        -- CFA pattern, as in DNG, RGGB little endian
    rawop_bits_per_pixel = 12,          -- raw bits per pixel
    rawop_raw_width = 5248,             --- dimensions from elph180, typical 20MP
    rawop_raw_height = 3920,
    rawop_active_left = 8,
    rawop_active_top = 16,
    rawop_active_width = 5174,
    rawop_active_height = 3888,
    rawop_jpeg_left = 19,
    rawop_jpeg_top  = 28,
    rawop_jpeg_width = 5152,
    rawop_jpeg_height = 3864,
}


-- TODO not used
local buttons={"up", "down", "left", "right", "set", "shoot_half", "shoot_full", "shoot_full_only", "zoom_in", "zoom_out", "menu", "display"}

-- fill propertycases
-- some cams uses 1001 as a proxy for propcase_shooting.
-- Getting out of range propcases on real cams returns an arbitrary value
for i=0, 1001 do
    camera_state.props[i]=0
end

-- root to search for camera modules
local camera_module_root = 'A/CHDK'

camera_funcs=require'camera_funcs'

-- and tidy some things up
camera_env._G=camera_env
camera_env.arg=nil
camera_env.package = {
    loaded={}
}
-- mark stuff that would be loaded on cam as loaded
for i,k in ipairs({'string','debug','package','_G','io','table','math',--[['coroutine',]]'imath','fmath'}) do
    camera_env.package.loaded[k] = package.loaded[k]
end
-- make a version of require that runs the module in the camera environment
camera_env.require=function(mname)
    if camera_env.package.loaded[mname] then
        return camera_env.package.loaded[mname]
    end
    -- TODO CHDK now honors package.path, this only does defaults!
    local f=loadfile(camera_module_root..'/SCRIPTS/'..mname..'.lua')
    if not f then
        f=loadfile(camera_module_root..'/LUALIB/'..mname..'.lua')
    end
    if not f then
        error("camera module '"..tostring(mname).."' not found")
    end
    setfenv(f,camera_env)
    camera_env.package.loaded[mname]=f()
    return camera_env.package.loaded[mname]
end

script_title = '<no title>'

local chdk_version = {0, 0, 0, 0}

function parse_script_header(scriptname,menu_override,load_params,save_params)
    local hdr=cms.new_header{
        file=scriptname,
        cfgfile=load_params,
    }
    if hdr.title then
        script_title = hdr.title
    end
    print("title:",script_title)
    print("@chdk_version:", table.concat({
        hdr.chdk_version_req.major,
        hdr.chdk_version_req.minor,
        hdr.chdk_version_req.sub,
        hdr.chdk_version_req.build,
    }, "."))

    -- merge menu overrides, if given
    hdr:set_values(menu_override)
    if save_params then
        fsutil.writefile(save_params,hdr:make_saved_cfg(),{bin=true})
    end

    for i, item in ipairs(hdr.items) do
        if item.paramtype ~= 'subtitle' then
            if item.paramtype == 'table' then
                camera_env[item.name] = {index=item.val, value=item.items[item.val]}
            else
                camera_env[item.name] = item.val
            end
            -- use shorthand to summarize menu values
            util.printf('%s',item:format_shorthand())
        end
    end
end

function usage()
    error(usage_string,0)
end

chdk_script_name=arg[1]
if not chdk_script_name then
    usage()
end


local menu_override={}
local load_params
local save_params
local i=2
while arg[i] do
    local opt,val=arg[i]:match("-([%w_]+)=(.*)")
    if opt and val then
        opt=opt:lower()
        if opt == "conf" then
            print("conf =",val)
            conf_script=val
        elseif opt == "modroot" then
            print("modroot =",val)
            camera_module_root=val
        elseif opt == "loadparams" then
            load_params=val
            print("load params from:",val)
        elseif opt == "saveparams" then
            save_params=val
            print("save params to:",val)
        -- for backward compatibility, single letter options are accepted like -a=1
        elseif opt:match("^%l$") then
            table.insert(menu_override,{opt,val})
        else
            error(("bad arg %s"):format(tostring(arg[i])),0)
        end
    else
        -- accept name=value for menu options
        opt, val = arg[i]:match("^(%a[%w_.]*)=(.*)$")
        if opt then
            table.insert(menu_override,{opt,val})
        else
            error(("bad arg %s"):format(tostring(arg[i])),0)
        end
    end
    i=i+1
end
-- header funcs use errlib catch to convert error to string
local status, err = xpcall(function()
    parse_script_header(chdk_script_name,menu_override,load_params,save_params)
end,
function(err)
    return tostring(err)
end)
if not status then
    error(err,0)
end

if conf_script then
    dofile(conf_script)
end

-- import the base camera functions, after conf so it can modify
for k,v in pairs(camera_funcs) do
    camera_env[k]=v
end

local chdk_script_f,err = loadfile(chdk_script_name)
if err then
    error("error loading " .. chdk_script_name .. " " .. err)
end
if chdk_version[1] == 1 and chdk_version[2] == 3 then
    print("load wrap13.lua")
    camera_env.require("wrap13")
end
setfenv (chdk_script_f, camera_env)
print("=== START =============================")
local status,result = pcall(chdk_script_f)
if not status then
    error("error running " .. chdk_script_name .. " " .. result)
end
