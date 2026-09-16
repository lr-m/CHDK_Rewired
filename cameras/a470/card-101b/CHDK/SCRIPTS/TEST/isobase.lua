--[[
@title ISO base check
@chdk_version 1.3
Check if CAM_MARKET_ISO_BASE is required in platform_camera.h
]]
capmode=require'capmode'
props=require'propcase'

function printf(...)
	print(string.format(...))
end
local rec, vid = get_mode()
if not rec then
	print("switching to rec")
	sleep(1000)
	set_record(true)
	repeat sleep(10) until get_mode()
	sleep(500)
	rec, vid = get_mode()
end
if vid then
	error('not in still mode')
end

if capmode.get_name() ~= 'P' then
	printf("switching to P")
	if not capmode.set('P') then
		error("set P failed")
	end
	sleep(500)
end

-- set to auto iso
set_iso_mode(0)

press('shoot_half')
repeat sleep(10) until get_shooting()
release('shoot_half')

sv96_base=get_prop(props.SV_MARKET)
if sv96_base==480 then
	printf('BASE=100')
	printf('no #define needed')
elseif sv96_base==576 then
	printf('BASE=200, set')
	printf('CAM_MARKET_ISO_BASE 200')
else
	printf('BASE=%d (%d) - Unknown value',sv96_to_iso(sv96_base),sv96_base)
	printf('investigate!')
end
printf('press any key to exit')

wait_click()
