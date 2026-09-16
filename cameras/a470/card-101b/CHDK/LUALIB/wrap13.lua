local function ret_bool_to_num(names)
	for i,name in ipairs(names) do
		local f=_G[name]
		if type(f) ~= 'function' then
			error('wrapper for non-function '..tostring(name))
		end
		_G[name]=function(...)
			if f(...) then
				return 1
			end
			return 0
		end
	end
end
ret_bool_to_num{
	'get_raw',
	'get_video_button',
	'get_focus_ok',
	'save_config_file',
	'load_config_file',
	'set_mf',
}
