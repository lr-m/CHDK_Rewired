--[[
********************************
Licence: GPL
(C)2013 - 2015 rudi
Version: 1.0
INI FILE LIBRARY EXAMPLE
********************************
@title ini test
@chdk_version 1.3
]]

--formatted output
local function fmt_inikey(tab, section, key)
    local res = "unknown"
    if type(tab) == "table" and type(tab[section]) == "table" and tab[section][key] ~= nil  then
        res = ("%s = %s (type is %s)"):format(tostring(key), tostring(tab[section][key]), type(tab[section][key]))
    end
    return res
end

--Bind iniLib
local iniLib = require("iniLib")

--Read from ini file "A/CHDK/DATA/initest.ini"
ini, new = iniLib.read("initest")

if new then
    --If new ini file creates necessary sections and keys
    --Creates section "main" with keys "top", "left", "caption", "hidden"
    ini.main = {
        top = 10,
        left = 2,
        caption = "window",
        hidden = false,
        version = "1"
    }
    --... and storing
    iniLib.write(ini)
    print("> leere INI ist initialisiert")
    print("> zum Vergleich starte Skript erneut")
end

--Output of section "main"
print("[main]")
print(fmt_inikey(ini, "main", "top"))
print(fmt_inikey(ini, "main", "left"))
print(fmt_inikey(ini, "main", "caption"))
print(fmt_inikey(ini, "main", "hidden"))
print(fmt_inikey(ini, "main", "version"))

--Output of section "bool" if exists
if ini.bool then
    print("[bool]")
    print(fmt_inikey(ini, "bool", "a"))
    print(fmt_inikey(ini, "bool", "b"))
end

--Creates section "bool"
ini.bool = { a = true, b = false }

--Changes of key values
ini.main.left = 20
ini.main.caption = "main menu"

-- stores ini file with test
if iniLib.write(ini) then
    print("> ini stored.")
else
    print("Error - storing failed!")
end
