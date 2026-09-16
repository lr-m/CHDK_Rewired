--[[
********************************
Licence: GPL
(C)2012 - 2015 rudi
Version: 1.0
GPS LIBRARY
********************************
]]

gpsLib = {}

gpsLib.version = "1.0"

local props = require("propcase")
local bs = require("binstr")

-- begin: LIBRARY INTERNAL FUNCTIONS
local offset, gps_str, gps_cam = 1

local function has_gps()
    local gps_cams = {
        {platform = "d20", a_sats = 0x74},
        {platform = "d30", a_sats = 0x74},
        {platform = "s100", a_sats = 0x98},
        {platform = "sx230hs", a_sats = 0x98},
        {platform = "sx260hs", a_sats = 0x74}
    }
    local bi = get_buildinfo()
    gps_cam = nil
    for i = 1, #gps_cams do
        if (bi.platform == gps_cams[i].platform)then
            gps_cam = gps_cams[i]
            break
        end
    end
    return (gps_cam ~= nil)
end

local function read_gps_property()
    -- size of complete GPS-Prop: 0x110 Byte
    -- size of equal values for SX230/SX260: 0x70 Byte
    -- "sats in fix" is highest position
    local size = gps_cam.a_sats + 4
    gps_str = get_prop_str(props.GPS, size)
    return (#gps_str == size)
end

-- read and calculate latitude or longitude or height
local function get_coordinate(pos, count, positiv_ref, scale)
    local _val, _res, _scale = {}, 0, scale or 1
    for i = 1, 2 * count + 1 do
        _val[i] = bs.getnum(gps_str, 4, pos + 4 * (i-1))
    end
    _val[1] = (_val[1] == positiv_ref) and 1 or -1
    for i = count, 1, -1 do
        if  _val[2 * i + 1] ~= 0 then
            _res = _res / 60 + _scale * _val[2 * i] / _val[2 * i + 1]
        end
    end
    return (_val[1] and _res) and _val[1] * _res or 0
end

local function get_latitude(scale)
    return get_coordinate(0x00 + offset, 3, string.byte("N"), scale)
end

local function get_longitude(scale)
    return get_coordinate(0x1C + offset, 3, string.byte("E"), scale)
end

local function get_height(scale)
    return get_coordinate(0x38 + offset, 1, 0, scale)
end

local function get_date()
    local _pos = 0x65 + offset
    local y, m, d = string.match(string.sub(gps_str, _pos, _pos + 9),"(%d+):(%d+):(%d+)")
    if y and m and d then
        -- day, month, year
        return tonumber(d), tonumber(m), tonumber(y)
    else
        return 0, 0, 0
    end
end

local function get_time()
    local _time, _pos = {}, 0x44 + offset
    for i = 1, 6 do
        _time[i] = bs.getnum(gps_str, 4, _pos + 4 * (i-1))
    end
        -- h, m, s
    if _time[2] * _time[4] * _time[6] > 0 then 
        return _time[1] / _time[2], _time[3] / _time[4], _time[5] / _time[6]
    else
        return 0, 0, 0
    end
end

local function get_sats_in_fix()
    -- position for sx230, s100: 0x98
    -- position for sx260, d20: 0x74
    return bs.getnum(gps_str, 4, gps_cam.a_sats + offset)
end

-- return timezone (min), daylight saving time (bool)
local function get_timezone()
    local atz = {0, 60, 120, 180, 210, 240, 270, 300, 330, 345, 360, 390, 420, 480, 540, 570, 600, 660, 720, 765,
        -660, -600, -540, -480, -420, -360, -300, -240, -210, -180, -150, -120, -60}
    stz = get_parameter_data(20)
    if stz then
        local is_world = (string.byte(stz, 5) == 1)
        local tz = is_world and string.byte(stz, 3) or string.byte(stz, 1)
        local isdst = ((is_world and string.byte(stz, 7) or string.byte(stz, 6)) == 1)
        return atz[tz + 1], isdst
    else
        return 0, 0
    end
end

local function is_valid()
    local _pos = 0x5C + offset
    return (string.sub(gps_str, _pos, _pos) == "A")
end

local function get_fix_dim()
    --equal to sx260 dimensions in fix
    local res, sats = 0, get_sats_in_fix()
    if is_valid() then
        if sats > 5 then res = 3
        elseif sats > 2 then res = 2 end
    end
    return res
end
-- end: LIBRARY INTERNAL FUNCTIONS

-- begin: LIBRARY EXTERNAL FUNCTION
function gpsLib.data(scale)
    if read_gps_property() then
        scale = scale or 10
        local _res = {}
        _res.status = is_valid()
        _res.lat    = get_latitude(scale)
        _res.lon    = get_longitude(scale)
        _res.height = get_height(scale)
        _res.date   = { get_date() }
        _res.time   = { get_time() }
        _res.tz,
        _res.isdst  = get_timezone()
        _res.sats   = get_sats_in_fix()
        _res.fix    = get_fix_dim()
        return _res
    end
end
-- end: LIBRARY EXTERNAL FUNCTION

-- begin: LIBRARY RESULT
--[[
    returns library only if camera has gps
]]
if has_gps() and read_gps_property() then
    return gpsLib
else
    return false
end
-- end: LIBRARY RESULT
