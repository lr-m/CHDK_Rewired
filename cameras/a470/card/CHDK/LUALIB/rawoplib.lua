--[[
utility functions for 'rawop' code
]]
if type(rawop) ~= 'table' then
	error('rawop not supported')
end

-- assumes CFA won't change on the fly (currently true)
local cfa_offsets=rawop.get_cfa_offsets()
-- locals to avoid a bunch of nested table lookups on every call
local cfa_r_x  = cfa_offsets.r.x
local cfa_r_y  = cfa_offsets.r.y
local cfa_g1_x = cfa_offsets.g1.x
local cfa_g1_y = cfa_offsets.g1.y
local cfa_g2_x = cfa_offsets.g2.x
local cfa_g2_y = cfa_offsets.g2.y
local cfa_b_x  = cfa_offsets.b.x
local cfa_b_y  = cfa_offsets.b.y

local fill_rect = rawop.fill_rect
local meter = rawop.meter

-- temp for backward compatibility
rawop.raw_to_ev96 = rawop.raw_to_ev
rawop.ev96_to_raw = rawop.ev_to_raw

--[[
filled RGB rectangle
]]
function rawop.fill_rect_rgbg(x,y,width,height,r,g1,b,g2)
	-- clamp to even values, bounds handled by C code
	x = bitand(x,0xFFFFFFFE)
	y = bitand(y,0xFFFFFFFE)
	width = bitand(width,0xFFFFFFFE)
	height = bitand(height,0xFFFFFFFE)
	-- default g2 to g1 if not specified
	if not g2 then
		g2 = g1
	end
	-- fill rects with step=2 and cfa offsets
	fill_rect(x+cfa_r_x,y+cfa_r_y,width,height,r,2)
	fill_rect(x+cfa_g1_x,y+cfa_g1_y,width,height,g1,2)
	fill_rect(x+cfa_b_x,y+cfa_b_y,width,height,b,2)
	fill_rect(x+cfa_g2_x,y+cfa_g2_y,width,height,g2,2)
end

local fill_rect_rgbg = rawop.fill_rect_rgbg

--[[
hollow rgb rectangle
]]
function rawop.rect_rgbg(x,y,width,height,linewidth,r,g1,b,g2)
	if width < 2*linewidth or height < 2*linewidth or linewidth < 2 then
		return
	end
	fill_rect_rgbg(x,y,width,linewidth,r,g1,b,g2)
	fill_rect_rgbg(x,y + height-linewidth,width,linewidth,r,g1,b,g2)
	fill_rect_rgbg(x,y+linewidth,linewidth,height-2*linewidth,r,g1,b,g2)
	fill_rect_rgbg(x + width - linewidth,y+linewidth,linewidth,height-2*linewidth,r,g1,b,g2)
end
--[[
hollow single value rectangle
]]
function rawop.rect(x,y,width,height,linewidth,v,xstep,ystep)
	if width < 2*linewidth or height < 2*linewidth or linewidth < 1 then
		return
	end
	fill_rect(x,y,width,linewidth,v,xstep,ystep)
	fill_rect(x,y + height-linewidth,width,linewidth,v,xstep,ystep)
	fill_rect(x,y+linewidth,linewidth,height-2*linewidth,v,xstep,ystep)
	fill_rect(x + width - linewidth,y+linewidth,linewidth,height-2*linewidth,v,xstep,ystep)
end

--[[
meter in RGB
x, y, start on the nearest even value
xstep and ystep must be even
]]
function rawop.meter_rgbg(x,y,xcount,ycount,xstep,ystep)
	x = bitand(x,0xFFFFFFFE)
	y = bitand(y,0xFFFFFFFE)
	if bitand(xstep,1) == 1 or bitand(ystep,1) == 1 then
		return
	end
	return
	 meter(x+cfa_r_x,y+cfa_r_y,xcount,ycount,xstep,ystep),
	 meter(x+cfa_g1_x,y+cfa_g1_y,xcount,ycount,xstep,ystep),
	 meter(x+cfa_b_x,y+cfa_b_y,xcount,ycount,xstep,ystep),
	 meter(x+cfa_g2_x,y+cfa_g2_y,xcount,ycount,xstep,ystep)
end

--[[
as above, but meter only the first green
]]
function rawop.meter_rgb(x,y,xcount,ycount,xstep,ystep)
	x = bitand(x,0xFFFFFFFE)
	y = bitand(y,0xFFFFFFFE)
	if bitand(xstep,1) == 1 or bitand(ystep,1) == 1 then
		return
	end
	return
	 meter(x+cfa_r_x,y+cfa_r_y,xcount,ycount,xstep,ystep),
	 meter(x+cfa_g1_x,y+cfa_g1_y,xcount,ycount,xstep,ystep),
	 meter(x+cfa_b_x,y+cfa_b_y,xcount,ycount,xstep,ystep)
end
