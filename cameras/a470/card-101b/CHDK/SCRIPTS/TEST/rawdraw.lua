--[[
@title raw drawing test
@chdk_version 1.4.0.4193
#shots=1 "Shots"
#enable_raw=true "Enable raw"
]]

require'hookutil'
require'rawoplib'
props=require'propcase'

function printf(fmt,...)
	print(string.format(fmt,...))
end

-- for ptp file exec
if not shots then 
	shots = 1
end

prev_raw_conf=get_raw()
if enable_raw then
	set_raw(true)
end

function restore()
	set_raw(prev_raw_conf)
end

-- initialized on in raw hook
local min_level
local max_level
fails=0

function check_pixel_and_draw_status(status_x,status_y,x,y,cr,cg1,cb,cg2)
	local r,g1,b,g2 = rawop.get_pixels_rgbg(x,y)
	local sr,sg
	if cr == r and cg1 == g1 and cg2 == g2 and cb == b then
		sr,sg=min_level,max_level
	else
		fails = fails + 1
		sr,sg=max_level,min_level
	end
	rawop.fill_rect_rgbg(status_x,status_y,40,40,sr,sg,min_level)
end

function do_draw()
 	min_level = rawop.get_black_level() + 128	
 	max_level = rawop.get_white_level() - 128
 	-- centered 500 px square
	local meter_size = 500

	local x1 = rawop.get_raw_width()/2 - meter_size/2

	local y1 = rawop.get_raw_height()/2 - meter_size/2

	local t0=get_tick_count()
	local m = rawop.meter(x1,y1,meter_size,meter_size,1,1)

	local r,g1,b,g2 = rawop.meter_rgbg(x1,y1,meter_size/2,meter_size/2,2,2)
	local meter_time= get_tick_count()-t0

	local t0=get_tick_count()
	rawop.rect_rgbg(x1-2,y1-2,meter_size+4,meter_size+4,2,max_level,max_level,max_level)

	rawop.fill_rect_rgbg(x1,y1,16,16,r,min_level,min_level)
	rawop.fill_rect_rgbg(x1 + meter_size - 16,y1,16,16,min_level,g1,min_level)
	rawop.fill_rect_rgbg(x1,y1 + meter_size - 16,16,16,min_level,g2,min_level)
	rawop.fill_rect_rgbg(x1 + meter_size - 16,y1 + meter_size - 16,16,16,min_level,min_level,b)

	local status_x = rawop.get_jpeg_left()+100
	local status_y = rawop.get_jpeg_top()+100

	-- check values from the rects drawn above
	check_pixel_and_draw_status(status_x,status_y,x1,y1,r,min_level,min_level,min_level)
	status_y = status_y + 80
	check_pixel_and_draw_status(status_x,status_y,x1+meter_size - 16,y1,min_level,g1,min_level,g1)
	status_y = status_y + 80
	check_pixel_and_draw_status(status_x,status_y,x1,y1+meter_size - 16,min_level,g2,min_level,g2)
	status_y = status_y + 80
	check_pixel_and_draw_status(status_x,status_y,x1+meter_size - 16,y1+meter_size - 16,min_level,min_level,b,min_level)

	status_y = status_y + 80
	local d_meter = m - (r+g1+b+g2)/4
	if d_meter == 0 then
		rawop.fill_rect_rgbg(status_x,status_y,40,40,min_level,max_level,min_level)
	-- TODO might have rounding issues, allow a little fudge
	elseif math.abs(d_meter) < 4 then
		rawop.fill_rect_rgbg(status_x,status_y,40,40,max_level,max_level,min_level)
	else
		printf("d_meter=%d",m-(r+g1+b+g2)/4)
		fails = fails + 1
		rawop.fill_rect_rgbg(status_x,status_y,40,40,max_level,min_level,min_level)
	end

	-- draw a big rect for timing, with different green levels just for fun
	rawop.fill_rect_rgbg(rawop.get_raw_width()/2-500,rawop.get_raw_height()-800,1000,500,min_level*4,min_level*3,min_level*2,min_level)

	-- TODO should check set pixel, out of bounds, rounding on rgbg funcs

	local status="pass"
	if fails > 0  then
		status = string.format("failed:%d\n",fails)
	end
	printf("%s meter:%d ms draw:%d ms ",status,meter_time,get_tick_count()-t0)
end
-- set hook in raw for drawing
hook_raw.set(10000)

press('shoot_half')

repeat sleep(10) until get_shooting()

for i=1,shots do
	click('shoot_full_only')

	-- wait for the image to be captured
	hook_raw.wait_ready()

	local count,ms =set_yield(-1,-1)
	do_draw()
	set_yield(count,ms)

	hook_raw.continue()
end
release('shoot_full')
restore()
sleep(2000)
