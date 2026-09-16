--[[
@title dynamic range test
@chdk_version 1.4.0.4241
#overstops=4 "+ stops"
#understops=6 "- stops"
#draw_meter=true "draw meter"
#draw_histo=true "draw histograms"
]]
require'hookutil'
require'rawoplib'

props=require'propcase'
capmode=require'capmode'


save_raw=get_raw()
set_raw(1) 
function restore()
	set_raw(save_raw)
	log:close()
end

function printf(...)
	print(string.format(...))
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
	self.name = opts.name
	self.dummy = opts.dummy
	if opts.buffer_mode then
		self.buffer_mode = opts.buffer_mode
	else
		self.buffer_mode = 'os'
	end
	if self.buffer_mode == 'table' then
		self.lines={}
	elseif self.buffer_mode ~= 'os' and self.buffer_mode ~= 'sync' then
		error('invalid buffer mode '..tostring(self.buffer_mode))
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
	if self.dummy then
		local nop =function() return end
		self.write=nop
		self.write_data=nop
		self.flush=nop
		self.set=nop
	else
		-- TODO name should accept autonumber or date based options
		if not opts.append then
			os.remove(self.name)
		end
		if self.buffer_mode == 'os' then
			self.fh = io.open(self.name,'ab')
			if not self.fh then
				error('failed to open log')
			end
		end
		self:write_data(self.cols)
		self:flush()
	end
end
function log:prepare_write()
	if self.buffer_mode == 'os' then
		return
	end
	-- if self.buffer_mode == 'sync' or self.buffer_mode then
	self.fh = io.open(self.name,'ab')
	if not self.fh then
		error('failed to open log')
	end
end
function log:finish_write()
	if self.buffer_mode == 'os' then
		return
	end
	self.fh:close()
	self.fh=nil
end

function log:write_csv(data)
	-- TODO should handle CSV quoting
	self.fh:write(string.format("%s\n",table.concat(data,',')))
end
function log:write_data(data)
	if self.buffer_mode == 'table' then
		table.insert(self.lines,data)
		return
	end
	self:prepare_write()
	self:write_csv(data)
	self:finish_write()
end

function log:flush()
	if self.buffer_mode == 'os' then
		if self.fh then
			self.fh:flush()
		end
	elseif self.buffer_mode == 'table' then
		if #self.lines == 0 then
			return
		end
		self:prepare_write()
		for i,data in ipairs(self.lines) do
			self:write_csv(data)
		end
		self:finish_write()
		self.lines={}
	end
	-- 'sync' is flushed every line
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
	self:write_data(data)
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
	if self.dummy then
		return function() end
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
	if self.dummy then
		return function() end
	end
	return function(fmt,...)
		table.insert(self.vals[name],string.format(fmt,...))
	end
end

function log:close()
	if self.buffer_mode == 'table' then
		self:flush()
	end
	if self.fh then
		self.fh:close()
	end
end
-- end log module


drtest={}

function drtest:init()
	self.histo=rawop.create_histogram()
	self.evh={}
	self.histo_scale = 100000
-- centered 500 px square
	self.meter_size = 500
end

--[[
update values from rawop per frame
--]]
function drtest:update_rawop_vals()
	local bl=rawop.get_black_level()
	local wl=rawop.get_white_level()
	self.draw_low = bl + bl/2
	self.draw_high = wl - bl
	self.draw_low_thresh = wl - wl/3
	self.cfa_offsets = rawop.get_cfa_offsets()
	self.meter_left = rawop.get_raw_width()/2 - self.meter_size/2
	self.meter_top = rawop.get_raw_height()/2 - self.meter_size/2
end

function meter_bar_width(v)
	return (v * 500)/rawop.get_white_level()
end

function drtest:draw_meter()
	if not draw_meter then
		return
	end
	-- draw color that contrasts with meter
	local c
	if self.m > self.draw_low_thresh then
		c = self.draw_low
	else
		c = self.draw_high
	end
	-- box around meter area
	rawop.rect(self.meter_left - 2,self.meter_top - 2,self.meter_size+4,self.meter_size+4,2,c)

	-- draw max scale
	rawop.fill_rect(100,90,500,4,self.draw_high)
	rawop.fill_rect(100,94,500,4,self.draw_low)
	-- draw levels
	rawop.fill_rect(100,100,meter_bar_width(self.m),20,self.m)
	rawop.fill_rect(100,120,meter_bar_width(self.m),20,c)
	rawop.fill_rect_rgbg(100,200,meter_bar_width(self.r),20,self.r,self.draw_low,self.draw_low)
	rawop.fill_rect_rgbg(100,220,meter_bar_width(self.r),20,self.draw_high,self.draw_low,self.draw_low)
	rawop.fill_rect_rgbg(100,300,meter_bar_width(self.g1),20,self.draw_low,self.g1,self.draw_low)
	rawop.fill_rect_rgbg(100,320,meter_bar_width(self.g1),20,self.draw_low,self.draw_high,self.draw_low)
	rawop.fill_rect_rgbg(100,400,meter_bar_width(self.g2),20,self.draw_low,self.g2,self.draw_low)
	rawop.fill_rect_rgbg(100,420,meter_bar_width(self.g2),20,self.draw_low,self.draw_high,self.draw_low)
	rawop.fill_rect_rgbg(100,500,meter_bar_width(self.b),20,self.draw_low,self.draw_low,self.b)
	rawop.fill_rect_rgbg(100,520,meter_bar_width(self.b),20,self.draw_low,self.draw_low,self.draw_high)
	-- draw blacklevel scale
	rawop.fill_rect(100,550,meter_bar_width(rawop.get_black_level()),4,self.draw_high)
	rawop.fill_rect(100,554,meter_bar_width(rawop.get_black_level()),4,self.draw_low)
end

-- print a 1000 scaled value to decimal
function drtest:f1k_str(v)
	return string.format("%d.%03d",v/1000,math.abs(v)%1000)
end

--[[
get rgb and combined meter
]]
function drtest:do_meter()
	local t0=get_tick_count()
	self.m = rawop.meter(self.meter_left,self.meter_top,self.meter_size,self.meter_size,1,1)
	self.r,self.g1,self.b,self.g2 = rawop.meter_rgbg(self.meter_left,self.meter_top,self.meter_size,self.meter_size,2,2)
	log:set{
		meter_time=get_tick_count()-t0,
		m=self.m,
		m96=self:f1k_str(rawop.raw_to_ev(self.m,96000)),
		r=self.r,
		r96=self:f1k_str(rawop.raw_to_ev(self.r,96000)),
		g1=self.g1,
		g1_96=self:f1k_str(rawop.raw_to_ev(self.g1,96000)),
		g2=self.g2,
		g2_96=self:f1k_str(rawop.raw_to_ev(self.g2,96000)),
		b=self.b,
		b96=self:f1k_str(rawop.raw_to_ev(self.b,96000)),
	}
end

function drtest:draw_histo()
	if not draw_histo then
		return
	end
	-- fill histo area
	local histo_height = #self.evh.all*4
	rawop.fill_rect(100,596,1000,(histo_height+8)*2,rawop.get_raw_neutral())
	-- draw combined in dark values
	self:draw_ev_histo(self.evh.all,100,600,self.draw_low)
	-- draw RGB in corresponding colors
	local top = 600 + histo_height + 8
	self:draw_ev_histo(self.evh.r,100 +  self.cfa_offsets.r.x,top  + self.cfa_offsets.r.y, self.draw_high,2)
	self:draw_ev_histo(self.evh.g1,100 + self.cfa_offsets.g1.x,top + self.cfa_offsets.g1.y, self.draw_high,2)
	self:draw_ev_histo(self.evh.g1,100 + self.cfa_offsets.g2.x,top + self.cfa_offsets.g2.y, self.draw_high,2)
	self:draw_ev_histo(self.evh.b,100 +  self.cfa_offsets.b.x,top +  self.cfa_offsets.b.y, self.draw_high,2)
end

function drtest:draw_ev_histo(vals, left, top, val, step)
	for i,v in ipairs(vals) do
		rawop.fill_rect(left,top+(i-1)*4,v/100,4,val,step)
	end
end

function drtest:make_ev_histo(step,mode)
	if not mode then
		mode = self.histo_scale
	end
	local bl,wl = rawop.get_black_level(), rawop.get_white_level()
	local raw_min = bl
	local r={
		step = step,
		mode = mode,
	}
	local count_max=0
	repeat
		-- TODO should use higher than default precision
		local ev_min = rawop.raw_to_ev(raw_min)
		local raw_max = rawop.ev_to_raw(ev_min + step - 1)
		if raw_max > wl then
			raw_max = wl
		end
		local count=self.histo:range(raw_min,raw_max,mode)
		table.insert(r,count)
		if count >= count_max then
			count_max = count
			r.peak_bin = #r
			r.peak_raw_min = raw_min
			r.peak_raw_max = raw_max
		end
		raw_min = raw_max + 1
	until raw_max == wl

	count_max = 0
	for raw_val=r.peak_raw_min,r.peak_raw_max do
		local count=self.histo:range(raw_val,raw_val,mode)
		if count >= count_max then
			r.peak_raw_val = raw_val
			r.peak_count = count
		end
	end
	return r
end
-- convert histo frac to % string
function drtest:pct_str(v)
	return string.format("%d.%03d",v/(self.histo_scale/100),v%(self.histo_scale/100))
end

function drtest:do_histo()
	-- update combined
	local t0=get_tick_count()
	self.histo:update(self.meter_left,self.meter_top, self.meter_size, self.meter_size, 1,1)
	log:set{histo_update_time=get_tick_count()-t0}

	t0=get_tick_count()
	self.evh.all=self:make_ev_histo(12)
	self.bl_pct,self.wl_pct = self.histo:range(1,rawop.get_black_level(),self.histo_scale),
								self.histo:range(rawop.get_white_level(),rawop.get_white_level(),self.histo_scale)
	log:set{
		histo_calc_time=get_tick_count()-t0,
		peak=rawop.raw_to_ev(self.evh.all.peak_raw_val),
		['peak_bin%']=self:pct_str(self.evh.all[self.evh.all.peak_bin]),
		['bl%']=self:pct_str(self.bl_pct),
		['wl%']=self:pct_str(self.wl_pct),
	}

	-- update r g b channels
	for i,name in ipairs{'r','g1','b'} do
		t0=get_tick_count()
		self.histo:update( self.meter_left + self.cfa_offsets[name].x,
						   self.meter_top  + self.cfa_offsets[name].y,
						   self.meter_size, self.meter_size, 2,2)
		log:set{histo_update_time=get_tick_count()-t0}

		t0=get_tick_count()
		self.evh[name] = self:make_ev_histo(12)
		log:set{
			histo_calc_time=get_tick_count()-t0,
			['peak_'..name]=rawop.raw_to_ev(self.evh[name].peak_raw_val),
			['peak_'..name..'_bin%']=self:pct_str(self.evh[name][self.evh[name].peak_bin]),
		}
	end
end

function drtest:do_draw()
	local t0=get_tick_count()
	self:draw_meter()
	self:draw_histo()
	log:set{draw_time=get_tick_count()-t0}
end

function drtest:doshot(shotdesc)
	log:set{
		shot=shotdesc,
	}
	self:update_rawop_vals()
	self:do_histo()
	self:do_meter()
	self:do_draw()
end

function log_base()
	log:set{shot='base'}
	local bi=get_buildinfo()
	logdesc("%s %s %s %s %s %s %s %s 0x%x",
						bi.platform,bi.platsub,bi.version,bi.build_number,bi.build_revision,
						bi.build_date,bi.build_time,bi.os,bi.platformid)

	-- bv only measured by cam at start
	logdesc("bv96=%d",get_prop(props.BV))
	log:write()
end

if not get_mode() then
	printf("switching to rec")
	set_record(true)
	repeat sleep(10) until get_mode()
	sleep(500)
end

if capmode.get_name() ~= 'P' then
	printf("switching to P")
	if not capmode.set('P') then
		error("set P failed")
	end
	sleep(500)
end

log:init{
	name="A/drtest.csv",
	append=true,
	buffer_mode='table',
	-- column names
	cols={
		'shot',
		'date',
		'time',
		'tick',
		'exp',
		'meter_time',
		'histo_update_time',
		'histo_calc_time',
		'draw_time',
		'free_mem',
		'lua_mem',
		'tsensor',
		'sv96',
		'tv96',
		'av96',
		'm',
		'm96',
		'r',
		'r96',
		'g1',
		'g1_96',
		'g2',
		'g2_96',
		'b',
		'b96',
		'peak',
		'peak_bin%',
		'peak_r',
		'peak_r_bin%',
		'peak_g1',
		'peak_g1_bin%',
		'peak_b',
		'peak_b_bin%',
		'bl%',
		'wl%',
		'desc',
	},
	-- columns automatically set at write time from functions
	funcs={
		date=function()
			return os.date('%m/%d/%Y')
		end,
		time=function()
			return os.date('%H:%M:%S')
		end,
		tick=get_tick_count,
		exp=get_exp_count,
		free_mem=function()
			return get_meminfo().free_size
		end,
		lua_mem=function()
			return collectgarbage('count')
		end,
		tsensor=function()
			return get_temperature(1)
		end,
		tv96=function()
			return get_prop(props.TV)
		end,
		sv96=function()
			return get_prop(props.SV)
		end,
		av96=function()
			return get_prop(props.AV)
		end
	},
	-- columns collected in a table, concatenated at write time
	tables={
		desc=' / ',
		histo_update_time=' / ',
		histo_calc_time=' / ',
	},
}
logdesc=log:text_logger('desc')

drtest:init()

-- TODO should focus at inf, possibly set zoom

hook_raw.set(20000)
press('shoot_half')
repeat sleep(10) until get_shooting()
base_tv=get_tv96()
log_base()

tv=base_tv-96*overstops
for i=-overstops,understops do
	set_tv96_direct(tv)
	ecnt=get_exp_count()
	press("shoot_full_only")
	-- wait for raw data to be ready
	hook_raw.wait_ready()
	release("shoot_full_only")
	drtest:doshot(string.format("%d",-i))
	log:write()
	hook_raw.continue()
	collectgarbage('step')
	tv = tv+96
end
-- allow final raw to save before restoring raw setting
sleep(5000)
restore()
