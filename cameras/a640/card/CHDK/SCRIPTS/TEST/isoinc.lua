--[[
@title ISO increment test
@chdk_version 1.4.0.4241
#ui_iso_start=371 "Start value"
#ui_iso_end=411 "End value"
#ui_input_mode_t=1 "ISO input type" {SV96 ISO} table
#shots_per_step=1 "Shots per step"
#const_exp=false "Keep exposure constant"
#start_delay=1 "Start delay, sec"
#start_ev_shift=-2 "Initial ev shift"
#ui_shoot_mode_t=1 "Shoot mode" {Single Quick Cont} table
#ui_iso_mode_t=1 "ISO set mode" {CHDK Script} table
#ui_log_append=false "Append log"
]]
--[[
Script to test ISO control
See https://chdk.setepontos.com/index.php?topic=12165.0

License: GPL

Copyright 2015-2019 reyalp (at) gmail.com

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
with CHDK. If not, see <http://www.gnu.org/licenses/>.
]]
require'hookutil'
require'rawoplib'
props=require'propcase'

isoinc_version="1.0"

-- log module
log={}
function log:init(opts)
	if not opts then
		error('missing opts');
	end
	self.cols={unpack(opts.cols)}
	self.vals={}
	self.funcs={}
	self.tables={}
	if opts.funcs then
		for n,f in pairs(opts.funcs) do
			if type(f) ~= 'function' then
				error('expected function')
			end
			self.funcs[n] = f
		end
	end
	-- TODO may accept other options than sep later
	if opts.tables then
		for n,sep in pairs(opts.tables) do
			self.tables[n] = {sep=sep}
		end
	end
	self:reset_vals()
	-- checks after vals initialized
	for n, v in pairs(self.funcs) do
		if not self.vals[n] then
			error('missing func col '.. tostring(n))
		end
	end
	for n, v in pairs(self.tables) do
		if not self.vals[n] then
			error('missing table col '.. tostring(n))
		end
	end
	-- TODO name should accept autonumber or date based options
	if opts.append then
		self.fh = io.open(opts.name,'ab')
	else
		self.fh = io.open(opts.name,'wb')
	end
	if not self.fh then
		error('failed to open log')
	end
	self:write_csv(self.cols)
end
function log:write_csv(data)
	-- TODO should handle CSV quoting
	self.fh:write(string.format("%s\n",table.concat(data,',')))
end
function log:write()
	local data={}
	for i,name in ipairs(self.cols) do
		local v
		if self.funcs[name] then
			v=tostring(self.funcs[name]())
		elseif self.tables[name] then
			v=table.concat(self.vals[name],self.tables[name].sep)
		else
			v=self.vals[name]
		end
		table.insert(data,v)
	end
	self:write_csv(data)
	self:reset_vals()
end
function log:reset_vals()
	for i,name in ipairs(self.cols) do
		if self.tables[name] then
			self.vals[name] = {}
		else
			self.vals[name] = ''
		end
	end
end
function log:set(vals)
	for name,v in pairs(vals) do
		if not self.vals[name] then
			error("unknown log col "..tostring(name))
		end
		if self.funcs[name] then
			error("tried to set func col "..tostring(name))
		end
		if self.tables[name] then
			table.insert(self.vals[name],v)
		else
			self.vals[name] = tostring(v)
		end
	end
end
--[[
return a function that records time offset from col named base_name
if name is not provided, function expects target aname as arg
]]
function log:dt_logger(base_name,name)
	if not self.vals[base_name] then
		error('invalid base field name')
	end
	if not name then
		return function(name)
			if not self.vals[name] then
				error('invalid col name')
			end
			self.vals[name]=get_tick_count() - self.vals[base_name]
		end
	end
	if not self.vals[name] then
		error('invalid col name')
	end
	return function()
		self.vals[name]=get_tick_count() - self.vals[base_name]
	end
end

--[[
return a printf-like function that appends to table col
]]
function log:text_logger(name)
	if not self.vals[name] then
		error('invalid col name')
	end
	if not self.tables[name] then
		error('text logger must be table field '..tostring(name))
	end
	return function(fmt,...)
		table.insert(self.vals[name],string.format(fmt,...))
	end
end

function log:close()
	if self.fh then
		self.fh:close()
	end
end
-- end log module

function restore()
	log:close()
end

log:init{
	name="A/isoinc.csv",
	append=ui_log_append,
	-- column names
	cols={
		'date',
		'tick',
		'exp',
		'vbatt',
		'tsensor',
		'topt',
		'free_mem',
		'lua_mem',
		'sd_space',
		'sv',
		'sv96',
		'p_sv',
		'p_sv_market',
		'p_delta_sv',
		'p_delta_sv_next',
		'p_iso_mode',
		'tv',
		'tv96',
		'av',
		'av96',
		'meter',
		'meter96',
		'desc',
	},
	-- columns automatically set at write time from functions
	funcs={
		date=function()
			return os.date('%m/%d/%Y %H:%M:%S')
		end,
		tick=get_tick_count,
		exp=get_exp_count,
		vbatt=get_vbatt,
		tsensor=function()
			return get_temperature(1)
		end,
		topt=function()
			return get_temperature(0)
		end,
		free_mem=function()
			return get_meminfo().free_size
		end,
		lua_mem=function()
			return collectgarbage('count')
		end,
		sd_space=get_free_disk_space,
	},
	-- columns collected in a table, concatenated at write time
	tables={
		desc=' / ',
	},
}
logdesc=log:text_logger('desc')

meter_size=1200
meter_step=5
meter_left = rawop.get_raw_width()/2 - meter_size/2
meter_count = meter_size/meter_step
meter_top = rawop.get_raw_height()/2 - meter_size/2

function run()
	local bi=get_buildinfo()
	logdesc("isoinc v:%s",isoinc_version)
	logdesc("platform:%s-%s-%s-%s %s %s",
						bi.platform,bi.platsub,bi.build_number,bi.build_revision,
						bi.build_date,bi.build_time)

	local shoot_mode = string.lower(ui_shoot_mode_t.value)
	logdesc('shoot_mode=%s',shoot_mode)
	if shoot_mode == 'cont' and get_prop(props.DRIVE_MODE) ~= 1 then
		error('Canon cont mode not set')
	end
	if string.lower(ui_input_mode_t.value) == 'iso' then
		sv96_start = sv96_market_to_real(iso_to_sv96(ui_iso_start))
		sv96_end = sv96_market_to_real(iso_to_sv96(ui_iso_end))
	else
		sv96_start = ui_iso_start
		sv96_end = ui_iso_end
	end
	logdesc('input=%s %d-%d range=%d-%d',string.lower(ui_input_mode_t.value),ui_iso_start,ui_iso_end,sv96_start,sv96_end)

	iso_method = string.lower(ui_iso_mode_t.value)
	logdesc('iso_method=%s',iso_method)
	if iso_method == 'script' then
		if shoot_mode == 'single' then
			error("can't use script ISO method with single")
		end
		-- override set_sv96 function
		function set_sv96_my(v)
				-- from http://chdk.setepontos.com/index.php?topic=11081.100
				-- delta sv = (sv change) + old DELTA_SV
				local dsv = v - get_prop(props.SV) + get_prop(props.DELTA_SV)
				set_prop(props.DELTA_SV,dsv)
				set_prop(props.SV,v)
				log:set{p_delta_sv_next=dsv}
				-- SV_MARKET not set
		end
		-- probably required to make delta_sv work, called implicitly by first set
		-- set_iso_mode(0)
	else
		set_sv96_my = set_sv96
	end


	sleep(start_delay*1000)
	local shots=(sv96_end-sv96_start+1)*shots_per_step
	local sv96=sv96_start
	-- preshoot to meter
	press'shoot_half'
	repeat sleep(10) until get_shooting()
	release'shoot_half'

	-- ensure shooting has gone back to false so initial overrides will be SET_LATER
	repeat sleep(10) until not get_shooting()
	sleep(50)

	logdesc('start tv96=%s sv96=%s ev_shift=%d',get_prop(props.TV),get_prop(props.SV),start_ev_shift)
	local sv96_meter=get_prop(props.SV)
	local d_sv96 = sv96 - sv96_meter 
	local tv96 = get_prop(props.TV) + d_sv96 - start_ev_shift*96
	set_tv96_direct(tv96)
	set_sv96(sv96)
	sleep(100)


	-- set shoot hook for cont mode
	hook_shoot.set(10000)
	-- set hook in raw for exposure
	hook_raw.set(10000)

	if shoot_mode ~= 'single' then
		press('shoot_half')
		repeat sleep(10) until get_shooting()
	end

	if shoot_mode == 'cont' then
		press('shoot_full_only')
	end

	local step_shots = 0
	for i=1,shots do
		print("shot",i,"/",shots)

		wait_click(10)
		if is_key('menu') then
			logdesc('user exit')
			log:write()
			break
		end
		if shoot_mode == 'single' then
			repeat sleep(10) until not get_shooting()
			sleep(50)
			set_sv96(sv96)
			set_tv96_direct(tv96)
			sleep(50)
			press('shoot_half')
			repeat sleep(10) until get_shooting()
			sleep(50)
		end

		if shoot_mode ~= 'cont' then
			press('shoot_full_only')
		end
		-- wait until the hook is reached
		hook_shoot.wait_ready()
		if shoot_mode ~= 'cont' then
			release('shoot_full_only')
		end
		if shoot_mode == 'single' then
			release('shoot_half')
		end
		-- allow shooting to proceed
		hook_shoot.continue()

		-- wait for the image to be captured
		hook_raw.wait_ready()
		local bl = rawop.get_black_level()
		-- log blacklevel changes (g1x)
		if bl ~= bl_prev then
			logdesc("black_level:%d",bl)
			bl_prev = bl
		end
		local meter = rawop.meter(meter_left,meter_top,
								meter_count,meter_count,
								meter_step,meter_step)

		tv96=get_prop(props.TV)
		local meter96 = rawop.raw_to_ev(meter)
		local av96=get_prop(props.AV)
		local tvus=tv96_to_usec(tv96)
		local av1k=av96_to_aperture(av96)
		log:set{
			meter=meter,
			meter96=meter96,
			sv=sv96_to_iso(sv96_real_to_market(sv96)),
			sv96=sv96,
			p_sv=get_prop(props.SV),
			p_sv_market=get_prop(props.SV_MARKET),
			p_delta_sv=get_prop(props.DELTA_SV),
			p_iso_mode=get_prop(props.ISO_MODE),
			av=string.format("%d.%03d",av1k/1000,((av1k%1000))),
			av96=av96,
			tv=string.format("%d.%06d",tvus/1000000,((tvus%1000000))),
			tv96=tv96,
		}
		step_shots = step_shots + 1
		if step_shots == shots_per_step then
			sv96 = sv96+1
			if const_exp then
				tv96 = tv96 + 1
			end
			step_shots = 0
		end
		if shoot_mode ~= 'single' and i ~= shots then
			set_sv96_my(sv96)
			set_tv96_direct(tv96)
		end
		hook_raw.continue()

		log:write()
		collectgarbage('step')
	end
	hook_shoot.set(0)
	hook_raw.set(0)
	release('shoot_full')
end
run()
-- allow final shot to end before restore
repeat sleep(10) until not get_shooting()
sleep(1000)
restore()
