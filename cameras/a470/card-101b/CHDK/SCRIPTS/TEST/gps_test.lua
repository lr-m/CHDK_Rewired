--[[
********************************
Licence: GPL
(C)2012 - 2015 rudi
Version: 1.0
READ GPS-PROPERTY
********************************
@title GPS values
@chdk_version 1.3
]]
function printf(...)
    print(string.format(...))
end

function divformat(val, div)
    return val / div, math.abs(val % div)
end

set_console_layout(0,0,25,14)

gps = require("gpsLib")
scale = 10  --Scaling of geographical values

if gps then
    tab = gps.data(scale)
    printf("GPS PROPERTY Library V%s", gps.version)
    printf("State      : %s", tostring(tab.status))
    printf("Latitude   : %d.%d°", divformat(tab.lat, scale))
    printf("Longitude  : %d.%d°", divformat(tab.lon, scale))
    printf("Height     : %d.%dm", divformat(tab.height, scale))
    printf("Date       : %2d.%02d.%04d", tab.date[1], tab.date[2], tab.date[3])
    printf("Time       : %2d:%02d:%02d", tab.time[1], tab.time[2], tab.time[3])
    printf("Time zone  : %d", tab.tz)
    printf("DST        : %s", tostring(tab.isdst))
    printf("Satellites : %d", tab.sats)
    if tab.fix > 0 then printf("Dimension : %sD", tostring(tab.fix))end
else
    print("No GPS")
end