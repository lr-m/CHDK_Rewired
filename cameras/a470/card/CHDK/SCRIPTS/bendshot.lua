--[[
@title Shoot all saved bends
@chdk_version 1.3
@param a Pause between shots (s)
@default a 2
@range a 0 30
]]

-- BENDNN.BND contains an eight-byte header followed by the 32-byte bend_t.
-- Config ID 1288 is that same structure exposed as eight 32-bit integers.
local BEND_CFG = 1288
local ENABLE_CFG = 1280

local function le32(s, p)
    local a,b,c,d = string.byte(s, p, p + 3)
    local v = a + b*256 + c*65536 + d*16777216
    if v >= 2147483648 then v = v - 4294967296 end
    return v
end

local function load_bend(slot)
    local name = string.format("A/CHDK/BENDS/BEND%02d.BND", slot)
    local f = io.open(name, "rb")
    if not f then return false end
    local s = f:read("*a")
    f:close()
    if #s ~= 40 or string.sub(s,1,4) ~= "BND1" or
       string.byte(s,5) ~= 1 or string.byte(s,6) ~= 0 or
       string.byte(s,7) ~= 32 or string.byte(s,8) ~= 0 then
        print(string.format("skip bad BEND%02d", slot))
        return false
    end
    local words = {}
    for i=0,7 do words[i+1] = le32(s, 9 + i*4) end
    return set_config_value(BEND_CFG, words)
end

local old_bend = get_config_value(BEND_CFG)
local old_enable = get_config_value(ENABLE_CFG)
local count = 0

-- Do not rewrite CCHDK4.CFG for every preset. The live values still change;
-- autosave is restored after putting the user's original bend back.
set_config_autosave(0)
set_config_value(ENABLE_CFG, 1)
for slot=0,99 do
    if load_bend(slot) then
        count = count + 1
        print(string.format("BEND%02d  shot %d", slot, count))
        shoot()
        if a > 0 then sleep(a * 1000) end
    end
end

set_config_value(BEND_CFG, old_bend)
set_config_value(ENABLE_CFG, old_enable)
set_config_autosave(1)
save_config_file(1000) -- core configuration (CCHDK4.CFG)
print(string.format("done: %d shots", count))
