--[[
adds wait_ready method for shoot hooks
usage:
status=hook_name.wait_ready([opts])
opts is an optional table which may contain
timeout: number of ms to wait for hook to be ready, default 10 seconds
timeout_error: boolean - if true (default), throw an error if timeout expires
               otherwise unset hook and return false
raw_adj_tv: boolean - if true (default) add shutter time to timeout (raw hook only)
raw_adj_dfs: boolean - if true (default) double shutter time wait if dark frame likely (raw hook only)
--]]
if type(hook_shoot) ~= 'table' then
	error('build does not support shoot hooks')
end

local m={}

m.wait_ready_defaults={
	timeout=10000, -- wait for hook to be reached, in msec
	timeout_error=true, -- call error on timeout
	raw_adj_tv=true, -- add shutter time to timeout (raw hook only)
	raw_adj_dfs=true, -- double shutter time wait if dark frame likely
}

-- return timeout adjustment for raw hook to ensure long shutter accounted for
function m.raw_timeout_tv_adjust(opts)
	local tv_ms=tv96_to_usec(get_tv96())/1000
	if opts.raw_adj_dfs then
		-- if NR forced on, or auto and shutter > 1 sec assume dark frame active
		-- note exact canon threshold may not be 1 sec
		if get_raw_nr() == 2 or (get_raw_nr() == 0 and tv_ms >= 1000) then
			return 2*tv_ms
		end
		return tv_ms
	elseif opts.raw_adj_tv then
		return tv_ms
	end
	return 0
end

for i,name in ipairs{'hook_preshoot','hook_shoot','hook_raw'} do
	local hook =_G[name]
	-- add wait_ready to existing hook tables
	hook.wait_ready=function(opts)
		if not opts then
			opts = m.wait_ready_defaults
		else
			for k,v in pairs(m.wait_ready_defaults) do
				if opts[k] == nil then
					opts[k] = m.wait_ready_defaults[k]
				end
			end
		end
		local timeout = opts.timeout + get_tick_count()
		if name == 'hook_raw' then
			timeout = timeout + m.raw_timeout_tv_adjust(opts)
		end
		while not hook.is_ready() do
			if get_tick_count() > timeout then
				if opts.timeout_error then
					error(name..' wait_ready timeout')
				end
				hook.set(0) -- clear hook, might hit it just after check
				return false
			end
			sleep(10)
		end
		return true
	end
end
return m
