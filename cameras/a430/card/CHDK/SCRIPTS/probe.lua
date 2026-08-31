--[[
@title Boot image probe
Dumps chdk_startup_probe AND the live buffer contents to A/PROBE.TXT.
PROBE is build-specific: arm-none-eabi-nm main.elf | grep chdk_startup_probe
--]]
local PROBE = 0x000c89d8
local TABLE = 0x812d8

-- words that tell Canon's image apart from ours, deep in the scan data
local OURS  = { [4000]=0x7e272684, [6000]=0xfc078e0d,
                [8000]=0x5a6861c6, [10000]=0xf41a3f52 }
local STOCK = { [4000]=0x34f000ff, [6000]=0x3996b2c6,
                [8000]=0xfcbfb66e, [10000]=0xaca8bb0b }

local f = io.open("A/PROBE.TXT", "wb")
f:write(string.format("probe @ 0x%08x\n\n", PROBE))
local names = { "hook calls","entry0 buf","entry0 size","entry0 slot",
                "entry0 first4","hook writes","call# 1st write","slot preset",
                "slot as first seen","TASK: buf ptr","TASK: wrote","TASK: ran (0xa5)" }
for i = 0, 11 do
    f:write(string.format("[%2d] 0x%08x  %s\n", i, peek(PROBE + i*4), names[i+1]))
end

local buf = peek(TABLE)
f:write(string.format("\nentry0 buffer = 0x%08x\n", buf))
f:write("first 16 bytes:\n  ")
for i = 0, 3 do f:write(string.format("%08x ", peek(buf + i*4))) end
f:write("\n\nwhose image is in the buffer NOW?\n")
for _, off in ipairs({4000, 6000, 8000, 10000}) do
    local v = peek(buf + off)
    local who = "?????"
    if v == OURS[off]  then who = "OURS"  end
    if v == STOCK[off] then who = "CANON" end
    f:write(string.format("  +%-6d 0x%08x  %s\n", off, v, who))
end
f:close()
