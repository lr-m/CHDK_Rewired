--[[
@title ND control test
@chdk_version 1.5.0.5104
#ui_shots_per_step=3 "Shots per state"
#ui_start_delay=1 "Start delay, sec"
@subtitle Shooting modes
#ui_shoot_single=true " Single"
#ui_shoot_quick=true " Quick"
#ui_shoot_cont=true " Continuous"
@subtitle Advanced
#ui_nd_shot_ev96=0 "ND value EV96 (0=auto)"
#ui_nd_set_av=1 "Set AV prop on ND-only cam" {Auto Quick Always No} table
#ui_nd_set_shoot=1 "Set ND in shoot hook" {Auto Quick Always No} table
#ui_set_sv_in_manual=false "Set ISO in non-Auto ISO"
#ui_shooting_ready_delay=100 "Delay after get_shooting (ms)" 
#ui_use_raw_e=2 "Save CHDK raw" {Default Yes No}
#ui_log_append=true "Append log"
]]
--[[
Script to test ND control and get the approximate exposure value of the ND filter

Usage:
Put the camera on a tripod or other stable surface, aimed at a scene with little
lighting variation or movement and run the script. A relatively uniform, neutral
scene like a blank wall or sheet of paper will give better results.
The camera should be in P or M mode, flash off, review off.
Continuous mode should be enabled in the Canon firmware for complete coverage.

Cameras with both ND and iris and manual Canon firmware ND control must have
ND set to out before running. The script will exit with the error
"Canon native ND in" if this is not done.

For Cameras with only an ND filter, the scene and settings must be such that
Canon auto exposure does not put in the ND. The script will exit with the error
"AE used ND, try darker scene" if the Canon firmware put in the ND.

Results:
The script prints status on completion:
"ALL PASSED" = ND functionality is fully working
"N warnings - check logs" = possible problems with ND implementation or scene
"N failed - check logs" = problems with ND implementation

Detailed results are logged to A/ndtest.csv.
Failure and warning messages are logged in the fail and warn columns.
Other messages are logged in the desc column

The default options are suitable for testing a port.

Options:
* Shots per state - Number of shots to with ND filter on and off
* Start delay - Seconds to wait before shootings start. Use to avoid operator shadow
  affecting exposure
* ND value ev96 - Expected ND value, used to keep the exposure relatively similar so
  neither state will be over or under exposed. The default is to use camera firmware
  functions to obtain the ND value.
* Shooting modes
 * Single - Do full half press, auto focus etc between each shots
 * Quick - Hold down shutter half way, click full for each shot
 * Cont - Shoot in continuous mode. Requires continuous mode enabled in the Canon UI
* Set Av prop on ND-only - Set the aperture propcase to reflect ND state on ND only
  cameras, like Canon firmware.
  * Auto - AV propcase is not set by default, failed tests are retried with it set
    In this mode, the script identifies if CAM_ND_SET_AV_VALUE is required
  * Quick - AV propcase is only set in "quick" mode
  * Always - AV propcase always set to match ND state
  * No - AV propcase not set 
* Set ND in shoot hook - Sets the ND state immediately before exposure start, rather
  than using normal override process.
  * Auto - If the ND is not in the expected state, set and generate warning
  * Quick - ND state is set only in "quick" mode
  * Always - ND state is set in all modes
  * No - ND state is not set in shoot hook
* Set ISO in non-Auto ISO - Override ISO for each shot, even if in non AUTO ISO mode
  By default, ISO is overridden if AUTO ISO enabled (to prevent changes between shots)
* Delay after get_shooting - Milliseconds to wait between get_shooting going true and 
  pressing shoot_full
* Use raw - by default, the script disables CHDK raw for quicker testing. Set to "Yes"
  to force raw saving, or "Default" to use the CHDK menu setting
* Append log - If checked, each run adds to the existing log. Otherwise the log is
  replaced each run.


License: GPL

Copyright 2017-2019 reyalp (at) gmail.com

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
capmode=require'capmode'

ndtest_version="1.7"

-- utility functions
function printf(...)
	print(string.format(...))
end

-- return av formatted as a decimal number, optionally specifying number of decimal places, default 2
function fmt_av(av96,dp)
	if not dp then
		dp=2
	elseif dp > 3 then
		dp=3
	end
	local av1k=av96_to_aperture(av96)
	if dp==0 then
		return tostring(av1k/1000)
	end
	return string.format('%d.%0'..dp..'d', av1k/1000,(av1k%1000)/(10^(3-dp)))
end

-- return tv formatted as a decimal number, optionally specifying number of decimal places, default 4
function fmt_tv(tv96,fmt)
	if not fmt then
		fmt="%d.%04d"
	end
	if not fmt:find('%.%%') then
		return fmt:format(tvus/1000000)
	end
	if not dp then
		dp=4
	elseif dp > 6 then
		dp=6
	end
	local tvus=tv96_to_usec(tv96)
	if dp==0 then
		return tostring(tvus/1000000)
	end
	return string.format('%d.%0'..dp..'d',tvus/1000000,(tvus%1000000)/(10^(6-dp)))
end

function frac_to_pct(v,scale)
	return string.format("%d.%04d",v/(scale/100),v%(scale/100))
end

function ensure_rec()
	local rec, vid = get_mode()
	if not rec then
		print("switching to rec")
		sleep(1000)
		set_record(true)
		local i=0
		repeat sleep(10) i=i+1 until get_mode() or i > 500
		sleep(500)
		rec, vid = get_mode()
	end
	if not rec then
		error('rec switch failed')
	end
	if vid then
		error('not in still mode')
	end
end

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
	if raw_enable_save then
		set_raw(raw_enable_save)
	end
	if saved_iso_mode then
		set_iso_mode(saved_iso_mode)
	end
	set_nd_filter(0)
	log:close()
end
-- restore, then error
function rerror(msg)
	if restore then
		restore()
	end
	error(msg,2)
end

log:init{
	name="A/ndtest.csv",
	append=ui_log_append,
	-- column names
	cols={
		'operation',
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
		'tv',
		'tv96',
		'av',
		'av96',
		'av96_cur',
		'nd',
		'nd_cur',
		'nd_av_match',
		'over_pct',
		'under_pct',
		'meter',
		'meter96',
		'fail',
		'warn',
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
		fail=' / ',
		warn=' / ',
	},
}
logdesc=log:text_logger('desc')


local ndtest={
	need_CAM_ND_SET_AV_VALUE=false,
	quick_need_CAM_ND_SET_AV_VALUE=false,
	quick_need_nd_in_shoot_hook=false,
	fail_logger=log:text_logger('fail'),
	warn_logger=log:text_logger('warn'),
	warn_count=0,
	test_warn_count=0,
	fail_count=0,
	test_fail_count=0,
	meter_size=1200,
	meter_step=5,
	meter_warn_var=12, -- warning level for variation of shots in a given state
	meter_warn_under=-96*5, -- warning level for under exposure (APEX*96 below neutral)
	meter_warn_over=24, -- warning level for over exposure (APEX*96 below white level
	histo_warn_over_pct=5,
	histo_warn_under=-96*6, -- warning level for histogram under exposure (APEX*96 below neutral)
	histo_warn_over=16, -- warning level for over exposure (APEX*96 below white level
	histo_warn_under_pct=10,
	shooting_ready_delay=ui_shooting_ready_delay, -- minimum delay between get_shooting going true and press full
	nd_val_err_max=32, -- failure level for calc nd value not matching expected
	nd_val_total=0, -- ND value across all tests
	test_count=0,
	results={}, -- table of results by mode
}


function ndtest:warn(fmt,...)
	self.warn_logger(fmt,...)
	printf('WARN '..fmt,...)
	self.warn_count = self.warn_count+1
	self.test_warn_count = self.test_warn_count+1
end
function ndtest:fail(fmt,...)
	self.fail_logger(fmt,...)
	printf('FAIL '..fmt,...)
	self.fail_count = self.fail_count+1
	self.test_fail_count = self.test_fail_count+1
end

function ndtest:state_results(name,r)
	local t=r.meter
	r.sum = 0
	r.min = t[1]
	r.max = t[1]
	for i=1,#t do
		if t[i] > r.max then
			r.max = t[i]
		end
		if t[i] < r.min then
			r.min = t[i]
		end
		r.sum = r.sum + t[i]
	end
	r.avg=r.sum/#t
	r.min96=rawop.raw_to_ev(r.min)
	r.max96=rawop.raw_to_ev(r.max)
	r.avg96=rawop.raw_to_ev(r.avg)
	-- warn if too much variation between shots
	if r.max96 - r.min96 > self.meter_warn_var then
		self:warn('%s variation=%d',name,r.max96-r.min96)
	end
	logdesc('%s:avg=%d (%d) min=%d (%d) max=%d (%d)',name,r.avg,r.avg96,r.min,r.min96,r.max,r.max96)
end

function ndtest:calc_results()
	nond=self.results[self.shoot_mode]['out']
	nd=self.results[self.shoot_mode]['in']

	self:state_results('ND-out',nond)
	self:state_results('ND-in',nd)

	local v=nond.avg96 - nd.avg96 + self.nd_shot_ev96
	logdesc('calc ND value=%d fw=%d',v,get_nd_value_ev96())
	self.nd_val_total = self.nd_val_total + v
	-- check firmware value, not possible UI override
	local v_err = math.abs(v - get_nd_value_ev96())
	if v_err > self.nd_val_err_max then
		self:fail('ND value mismatch %d',v_err)
	end
	local retry_str=''
	if self.nd_set_av_retry then
		retry_str=' (retry)'
	end
	if self.test_warn_count == 0 and self.test_fail_count == 0 then
		logdesc('%s%s PASSED',self.shoot_mode,retry_str)
	else
		if self.test_warn_count > 0 then
			logdesc('%s mode %d warnings',self.shoot_mode,self.test_warn_count)
		end
		if self.test_fail_count > 0 then
			logdesc('%s mode %d failed',self.shoot_mode,self.test_fail_count)
		end
	end

end

function ndtest:final_results()
	log:set{operation='overall result'}
	if self.warn_count == 0 and self.fail_count == 0 then
		print('ALL PASSED')
		logdesc('ALL PASSED')
	else 
		if self.warn_count > 0 then
			printf('%d warnings - check log',self.warn_count)
			logdesc('%d warnings',self.warn_count)
		end
		if self.fail_count > 0 then
			if self.test_result == 'fatal' then
				printf('FATAL ERROR - test incomplete')
			end
			printf('%d failed - check log',self.fail_count)
			logdesc('%d failed',self.fail_count)
		end
	end
	if #self.shoot_modes > 1 then
		logdesc('avg calc ND value=%d fw=%d',self.nd_val_total/self.test_count,get_nd_value_ev96())
	end
	if self.need_CAM_ND_SET_AV_VALUE then
		logdesc('port needs CAM_ND_SET_AV_VALUE all modes')
	elseif self.quick_need_CAM_ND_SET_AV_VALUE then
		logdesc('port needs CAM_ND_SET_AV_VALUE for quick')
	end
	if self.quick_need_nd_in_shoot_hook then
		logdesc('port needs ND in shoot hook for quick')
	end
	log:write()
end


-- set AV to match ND state for ND only cams
function ndtest:do_nd_av(nd_state)
	if get_nd_present() == 1 and self.nd_set_av then
		local new_av = get_prop(props.MIN_AV)
		if nd_state then
			new_av = new_av + self.nd_shot_ev96
		end
		set_prop(props.AV,new_av)
		if props.AV2 then
			set_prop(props.AV2,new_av)
		end
	end
end

function ndtest:add_state_result(name,val)
	table.insert(self.results[self.shoot_mode][self.state_name][name],val)
end

function ndtest:meter()
	local meter = rawop.meter(self.meter_left,self.meter_top,
							self.meter_count,self.meter_count,
							self.meter_step,self.meter_step)
	local meter96=rawop.raw_to_ev(meter)

	if meter96 < self.meter_warn_under then
		self:warn('BAD SCENE meter underexp')
	end
	if meter96 > rawop.raw_to_ev(rawop.get_white_level()) - self.meter_warn_over then
		self:warn('BAD SCENE meter overexp')
	end

	self.histo:update(self.meter_left,self.meter_top,
					self.meter_left+self.meter_size,
					self.meter_top+self.meter_size,
					self.meter_step,self.meter_step,10)

	local histo_scale=10000
	local white_level=rawop.get_white_level()
	local over_histo_min=bitshru(rawop.ev_to_raw(rawop.raw_to_ev(white_level)-self.histo_warn_over),rawop.get_bits_per_pixel() - 10)

	local over_frac = self.histo:range(over_histo_min,1023,histo_scale)
	if over_frac > self.histo_warn_over_pct*(histo_scale/100) then
		self:warn('BAD SCENE histo overexp')
	end

	local under_histo_max = bitshru(rawop.ev_to_raw(self.histo_warn_under),rawop.get_bits_per_pixel() - 10)
	-- ignore much lower than black, typically bad pixels
	local under_frac = self.histo:range(8,under_histo_max,histo_scale)
	if under_frac > self.histo_warn_under_pct*(histo_scale/100) then
		self:warn('BAD SCENE histo underexp')
	end

	log:set{
		meter=meter,
		meter96=meter96,
		over_pct=frac_to_pct(over_frac,histo_scale),
		under_pct=frac_to_pct(under_frac,histo_scale),
	}
	self:add_state_result('meter',meter)
end

function ndtest:do_state_shoot_hook(nd_state)
	if nd_state then
		local try_nd_set_shoot
		if get_nd_current_ev96() == 0 then
			-- nd only cams
			if get_nd_present() == 1 then
				if self.nd_set_av then
					self:fail('hook_shoot ND out')
				else
					if self.nd_set_av_mode == 'auto' then
						self.nd_set_av_retry = true
						self.test_result = 'retry'
						if self.shoot_mode == 'quick' then
							self:warn('hook_shoot ND out')
						else
							-- fail if ND set av needed in non quick modes (=overrides don't work without)
							self:fail('hook_shoot ND out')
						end
					else
						self:fail('hook_shoot ND out')
					end
				end
			else
				if self.shoot_mode == 'quick' then
					self:warn('hook_shoot ND out')
				else
					self:fail('hook_shoot ND out')
				end
			end
			if self.nd_set_shoot_mode == 'auto' then
				try_nd_set_shoot=true
			end
		else
			-- nd in successfully, check if in retry
			if self.nd_set_av_retry then
				if self.shoot_mode == 'quick' then
					self.quick_need_CAM_ND_SET_AV_VALUE=true
				else
					self.need_CAM_ND_SET_AV_VALUE=true
				end
			end
		end
		if self.nd_set_shoot or try_nd_set_shoot then
			set_nd_filter(1)
			if get_nd_current_ev96() == 0 then
				self:fail('nd_set_shoot IN fail')
			else
				if try_nd_set_shoot then
					if get_nd_present() == 2 and self.shoot_mode == 'quick' then
						self.quick_need_nd_in_shoot_hook = true
					end
					logdesc('try_nd_set_shoot OK')
				end
			end
		end
	else
		if get_nd_current_ev96() > 0 then
			-- unexpectedly in is always a fail
			-- TODO could apply auto logic here, but not expected failure mode
			self:fail('hook_shoot ND in')
		end
		if self.nd_set_shoot then
			set_nd_filter(2)
			if get_nd_current_ev96() > 0 then
				self:fail('nd_set_shoot OUT fail')
			end
		end
	end
end

function ndtest:init()
	local bi=get_buildinfo()
	log:set{operation='init'}
	logdesc("ndtest v:%s",ndtest_version);
	logdesc("platform:%s-%s-%s-%s %s %s",
						bi.platform,bi.platsub,bi.build_number,bi.build_revision,
						bi.build_date,bi.build_time)

	if get_nd_present() == 0 then
		error('ND not enabled in port')
	end

	ensure_rec()

	-- save initial ISO mode to avoid unexpected auto after tests
	saved_iso_mode=get_prop(props.ISO_MODE)

	self.meter_left = rawop.get_raw_width()/2 - self.meter_size/2
	self.meter_count = self.meter_size/self.meter_step
	self.meter_top = rawop.get_raw_height()/2 - self.meter_size/2


	-- turn off CHDK raw by default, not needed
	self.use_raw=({false,1,0})[ui_use_raw_e + 1]
	if self.use_raw then
		raw_enable_save = get_raw()
		set_raw(self.use_raw)
	end
	self.nd_set_av_mode=string.lower(ui_nd_set_av.value)
	self.nd_set_shoot_mode=string.lower(ui_nd_set_shoot.value)
	self.shots_per_step=ui_shots_per_step
	self.shoot_modes={}
	if ui_shoot_single then
		table.insert(self.shoot_modes,'single')
	end
	if ui_shoot_quick then
		table.insert(self.shoot_modes,'quick')
	end
	if ui_shoot_cont then
		if get_prop(props.DRIVE_MODE) ~= 1 then
			self:warn('cont not enabled in Canon FW - skipping')
		else
			table.insert(self.shoot_modes,'cont')
		end
	end
	if ui_nd_shot_ev96 == 0 then
		self.nd_shot_ev96 = get_nd_value_ev96()
	else
		self.nd_shot_ev96 = ui_nd_shot_ev96 
	end
	if get_nd_present() == 2 and self.nd_set_av_mode ~= 'auto' and self.nd_set_av_mode ~= 'no' then
		self:warn('ignoring nd_set_av on ND+iris cam')
		self.nd_set_av_mode='no'
	end
	if #self.shoot_modes == 0 then
		rerror('no valid test modes enabled')
	end

	if saved_iso_mode > 40 and not ui_set_sv_in_manual then
		self.do_set_sv = false
	else
		self.do_set_sv = true
	end

	logdesc('capmode=%s isomode=%d flashmode=%d fw nd=%d nd_shot_ev96=%d modes=%s shots_step=%d nd_set_av=%s nd_set_shoot=%s set_sv=%s ready_delay=%d',
		capmode.get_name(),
		get_prop(props.ISO_MODE),
		get_prop(props.FLASH_MODE),
		get_nd_value_ev96(),
		self.nd_shot_ev96,
		table.concat(self.shoot_modes,':'),
		self.shots_per_step,
		self.nd_set_av_mode,
		self.nd_set_shoot_mode,
		tostring(self.do_set_sv),
		self.shooting_ready_delay)

	-- known NDs ~1-3, warn on unexpected range
	if get_nd_value_ev96() < 96 or get_nd_value_ev96() > 96*4 then
		self:warn('unexpected ND value %d',get_nd_value_ev96())
	end

	log:write()
	sleep(ui_start_delay*1000)
	self.histo=rawop.create_histogram()
end

function ndtest:set_sv96(sv96)
	if self.do_set_sv then
		set_sv96(sv96)
	end
end

function ndtest:run_test(shoot_mode)
	if shoot_mode == 'cont' and get_prop(props.DRIVE_MODE) ~= 1 then
		rerror('Canon cont mode not set')
	end
	self.test_result=true
	self.test_warn_count=0
	self.test_fail_count=0

	self.shoot_mode = shoot_mode
	self.state_name='out'

	self.results[self.shoot_mode] = {
		['in']={meter={}}, -- in is a keyword
		out={meter={}},
	}

	-- ensure ISO mode is restored to initial state, since CHDK override will set to AUTO
	set_iso_mode(saved_iso_mode)

	if self.nd_set_av_mode == 'always' then
		self.nd_set_av=true
	elseif self.nd_set_av_mode == 'no' then
		self.nd_set_av=false
	elseif self.nd_set_av_mode == 'quick' then
 		self.nd_set_av=(shoot_mode == 'quick')
	elseif self.nd_set_av_mode == 'auto' then
		if self.nd_set_av_retry then
			logdesc('retry %s nd_set_av=true',self.shoot_mode)
			self.nd_set_av=true
		else
			self.nd_set_av=false
		end
	else
		rerror('invalid nd_set_av_mode')
	end

	if self.nd_set_shoot_mode == 'always' then
		self.nd_set_shoot=true
	elseif self.nd_set_shoot_mode == 'no' then
		self.nd_set_shoot=false
	elseif self.nd_set_shoot_mode == 'quick' then
 		self.nd_set_shoot=(shoot_mode == 'quick')
	elseif self.nd_set_shoot_mode == 'auto' then
		-- will be auto set in 
 		self.nd_set_shoot=false
	else
		rerror('invalid nd_set_shoot_mode')
	end


	-- preshoot to meter
	press'shoot_half'
	repeat sleep(10) until get_shooting()
	local focus_ok=get_focus_ok()
	release'shoot_half'

	-- ensure shooting has gone back to false so initial overrides will be SET_LATER
	repeat sleep(10) until not get_shooting()
	sleep(50)

	logdesc('start shoot_mode=%s tv=%d sv=%d av=%d focus_ok=%s',
		shoot_mode,
		get_prop(props.TV),
		get_prop(props.SV),
		get_prop(props.AV),
		tostring(focus_ok))
	local sv96=get_prop(props.SV)
	local tv96=get_prop(props.TV)
	local av96=get_prop(props.AV)
	sleep(100)
	-- ND only, check if ND was in on auto exposure
	if get_nd_present() == 1 and get_prop(props.AV) ~= get_prop(props.MIN_AV) then
		rerror('AE used ND, try darker scene')
	elseif get_nd_present() == 2 and props.ND_FILTER_STATE and get_prop(props.ND_FILTER_STATE) == 1 then
		rerror('Canon native ND in')
	end

	set_tv96_direct(tv96)
	self:set_sv96(sv96)
	if get_nd_present() ~= 1 then
		set_av96_direct(av96)
	end
	set_nd_filter(2)

	hook_shoot.set(10000)
	hook_raw.set(10000)

	if shoot_mode ~= 'single' then
		press('shoot_half')
		repeat sleep(10) until get_shooting()
		sleep(self.shooting_ready_delay)
	end

	if shoot_mode == 'cont' then
		press('shoot_full_only')
	end

	local shots=self.shots_per_step*2
	local nd_state=false
	for i=1,shots do
		printf('%s %d/%d %s',shoot_mode,i,shots,self.state_name)
		log:set{operation='test '..shoot_mode}

		wait_click(10)
		if is_key('menu') then
			logdesc('user exit')
			log:write()
			self.test_result = 'break'
			break
		end
		if shoot_mode == 'single' then
			repeat sleep(10) until not get_shooting()
			sleep(50)
			self:set_sv96(sv96)
			set_tv96_direct(tv96)
			if get_nd_present() ~= 1 then
				set_av96_direct(av96)
			end
			if nd_state then
				set_nd_filter(1)
			end

			sleep(50)
			press('shoot_half')
			repeat sleep(10) until get_shooting()
			-- for single, this has to be done after get_shooting to mimic override behavior
			self:do_nd_av(nd_state)
			-- get av96 set for shoot, for hook comparison
			av96_shot = get_prop(props.AV)
			sleep(self.shooting_ready_delay)
		else
			-- get av96 set for shoot, for hook comparison
			av96_shot = get_prop(props.AV)
		end

		if shoot_mode ~= 'cont' then
			press('shoot_full_only')
		end
		-- wait until the hook is reached
		if not hook_shoot.wait_ready({timeout_error=false}) then
			self:fail('hook_shoot timeout')
			log:write()
			self.test_result = 'fatal'
			break
		end
		
		-- overrides appear to fail in some cases
		if get_prop(props.TV) ~= tv96 then
			self:warn('hook_shoot tv mismatch')
		end
		if get_prop(props.AV) ~= av96_shot then
			self:warn('hook_shoot av mismatch')
		end

		-- for ND only cams, record if AV value in shoot hook matches ND state
		if get_nd_present() == 1 then
			if nd_state then
				log:set{nd_av_match=(av96_shot == get_prop(props.MIN_AV) + get_nd_value_ev96())}
			else
				log:set{nd_av_match=(av96_shot == get_prop(props.MIN_AV))}
			end
		end

		-- check if ND state matches expected, set if needed
		self:do_state_shoot_hook(nd_state)

		if shoot_mode ~= 'cont' then
			release('shoot_full_only')
		end
		if shoot_mode == 'single' then
			release('shoot_half')
		end
		av96_cur=get_current_av96()
		-- allow shooting to proceed
		hook_shoot.continue()

		-- wait for the image to be captured
		if not hook_raw.wait_ready({timeout_error=false}) then
			self:fail('hook_raw timeout')
			log:write()
			self.test_result = 'fatal'
			break
		end
		self:meter()

		local tv96_log=get_prop(props.TV)
		local av96_log=get_prop(props.AV)
		log:set{
			sv=sv96_to_iso(sv96_real_to_market(sv96)),
			sv96=sv96,
			av=fmt_av(av96_log),
			av96=av96_log,
			av96_cur=av96_cur,
			tv=fmt_tv(tv96_log),
			tv96=tv96_log,
			nd=nd_state,
			nd_cur=get_nd_current_ev96(),
		}
		-- overrides appear to fail in some cases
		if tv96_log ~= tv96 then
			self:warn('hook_raw tv mismatch')
		end
		if av96_log ~= av96_shot then
			self:warn('hook_raw av mismatch')
		end

		-- check get_nd_current matches
		if nd_state then
			if get_nd_current_ev96() == 0 then
				-- from ixus115, hook_shoot check can pass, but ND out after
				-- give another chance to retry
				if get_nd_present() == 2 or self.nd_set_av then
					self:fail('hook_raw ND out')
				else
					if self.nd_set_av_mode == 'auto' then
						self.nd_set_av_retry = true
						self.test_result = 'retry'
						if self.shoot_mode == 'quick' then
							self:warn('hook_raw ND out')
						else
							-- fail if ND set av needed in non quick modes (=overrides don't work without)
							self:fail('hook_raw ND out')
						end
					else
						self:fail('hook_raw ND out')
					end
				end
			end
		else 
			if get_nd_current_ev96() ~= 0 then
				self:fail('hook_raw ND in')
			end
		end
		if i == self.shots_per_step then
			nd_state=true
			self.state_name='in'
			tv96 = tv96 - self.nd_shot_ev96
			-- save the ND out value of get_current_av96 for later comparison
			av96_cur_nond = av96_cur
		end
		if shoot_mode ~= 'single' and i ~= shots then
			if not get_shooting() then
				self:warn('get_shooting false in override %s',shoot_mode);
			end
			self:set_sv96(sv96)
			set_tv96_direct(tv96)
			if get_nd_present() ~= 1 then
				set_av96_direct(av96)
			end
			if nd_state then
				set_nd_filter(1)
			end
			self:do_nd_av(nd_state)
		end
		if i == shots then
			-- on ND+iris cams, av shouldn't change, probably indicates no real ND
			if get_nd_present() == 2 then
				if av96_cur ~= av96_cur_nond then
					self:fail('av change on ND+iris cam')
				end
			else
				-- on ND only cameras, current_av96 should match ND state
				-- TODO not clear if these should fail, or allow some wiggle room
				if av96_cur_nond ~= get_prop(props.MIN_AV) then
					self:fail('cur_av ND val mismatch')
				end
				if av96_cur ~= get_prop(props.MIN_AV)+get_nd_value_ev96() then
					self:fail('cur_av ND val mismatch')
				end
			end
		end
		hook_raw.continue()
		log:write()
		collectgarbage('step')
		if self.test_result == 'retry' then
			break
		end
	end
	hook_shoot.set(0)
	hook_raw.set(0)
	release('shoot_full')
	-- allow final shot to end
	repeat sleep(10) until not get_shooting()
	sleep(500)
	if self.test_result == true then
		self.test_count = self.test_count + 1
		self:calc_results()
		log:set{operation='result '..shoot_mode}
	elseif self.test_result == 'break' then
		log:set{operation='user exit '..shoot_mode}
	elseif self.test_result == 'fatal' then
		log:set{operation='fatal error '..shoot_mode}
	elseif self.test_result == 'retry' then
		log:set{operation='retry '..shoot_mode}
	end
	log:write()
	-- clear ND prop so it doesn't confuse Canon UI check in later runs
	if props.ND_FILTER_STATE and get_nd_present() == 2 then 
		set_prop(props.ND_FILTER_STATE,0)
	end
end
function ndtest:run_all_tests()
	local i=1
	while i <= #self.shoot_modes do
		self:run_test(self.shoot_modes[i])
		if self.test_result=='break' or self.test_result=='fatal' then
			break
		end
		if self.test_result ~= 'retry' then
			self.nd_set_av_retry=false -- clear retry on success
			i=i+1
		end
	end
	self:final_results()
	-- additional wait to allow shot to finish before restore
	sleep(1000)
	restore()
end
ndtest:init()
ndtest:run_all_tests()
