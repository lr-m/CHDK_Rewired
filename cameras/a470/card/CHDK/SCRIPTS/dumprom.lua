--[[
@title Dump ROM to card
@chdk_version 1.7
Dumps the camera's firmware ROM to A/PRIMARY.BIN on the card.
Defaults suit A470 / A480 (DIGIC III, ROM at 0xFFC00000, 4MB).
--]]

local BASE = 0xFFC00000     -- ROMBASEADDR for a470 / a480
local SIZE = 0x3FFFFC       -- 4MB less the final word, matching CHDK's own dump_rom()
local OUT  = "A/PRIMARY.BIN"
local FLUSH_WORDS = 4096    -- 16KB per write

-- localise for speed: this loop runs ~1M times
local band, shru, chr, concat = bitand, bitshru, string.char, table.concat

local f = io.open(OUT, "wb")
if not f then
    print("cannot open "..OUT)
    return
end

local t0 = get_tick_count()
local buf, n = {}, 0
local done, total = 0, SIZE / 4
local next_report = 5

for addr = BASE, BASE + SIZE - 4, 4 do
    -- peek returns a signed int (LUA_NUMBER is int), so mask rather than compare
    local v = peek(addr, 4)
    n = n + 1
    buf[n] = chr(band(v, 0xFF),
                 band(shru(v, 8), 0xFF),
                 band(shru(v, 16), 0xFF),
                 band(shru(v, 24), 0xFF))   -- ARM is little-endian
    if n == FLUSH_WORDS then
        f:write(concat(buf))
        buf, n = {}, 0
        done = done + FLUSH_WORDS
        local pct = (done * 100) / total
        if pct >= next_report then
            print(pct.."%")
            next_report = pct + 5
        end
    end
end

if n > 0 then f:write(concat(buf)) end
f:close()

print("done, "..((get_tick_count() - t0) / 1000).."s")
