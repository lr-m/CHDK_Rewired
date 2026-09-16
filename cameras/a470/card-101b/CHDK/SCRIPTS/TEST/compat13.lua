--[[
@title test 1.3 compat wrappers
@chdk_version 1.3
]]
function check_num(name,...)
	local r=_G[name](...)
	if r ~= 1 and r ~= 0 then
		error('bad ret from '..tostring(name))
	end
end
-- check simple bools
for i,name in ipairs{'get_raw','get_video_button','get_focus_ok'} do
	check_num(name)
end
-- should fail if not in rec, but we're just checking return value
check_num('set_mf',0)
local core=require("gen/cnf_core")
check_num('save_config_file',core._config_id,'A/TEST13.CFG')
check_num('load_config_file',core._config_id,'A/TEST13.CFG')
if not os.remove('A/TEST13.CFG') then
	error('remove A/TEST13.CFG failed')
end
print'ok'
