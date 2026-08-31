--[[
Special Lua module that makes drawing much more usefull and easy-to-use.
This module requires function such as: draw_pixel(), draw_rect() and others to be present
]]

--Author:  Outslider
--License: GPL all versions

SCREEN_DRAWINGS={}

draw={}

-- Don't change the order of the color names in this array - array index is mapped to color value
draw.colors={
    "trans","black","white",
    "red","red_dark","red_light",
    "green","green_dark","green_light",
    "blue","blue_dark","blue_light",
    "grey","grey_dark","grey_light",
    "yellow","yellow_dark","yellow_light",
    "grey_trans", "magenta"
}

draw.colors_map={}
for i,v in ipairs(draw.colors) do
	draw.colors_map[v] = i + 255
end

-- convert an integer from get_config_value of a color option to foreground and background colors
-- returns foreground, background as color numbers
draw.unpack_cnf_colors = function(v)
	return bitand(v,0x1ff),bitand(bitshru(v,16),0x1ff)
end

-- turn foreground, background color pair into integer suitable for passing to set_config_value
-- fg and bg may be color numbers or string names
draw.pack_cnf_colors = function(fg,bg)
	if type(fg) == 'string' then
		fg = draw.make_color(fg)
	end
	if type(bg) == 'string' then
		bg = draw.make_color(bg)
	end
	return bitor(bitshl(bitand(bg,0x1ff),16),bitand(fg,0x1ff))
end

draw.make_color = function(c)
    -- return numeric ID for named color, or original value if not matched
	if draw.colors_map[c] then
		return draw.colors_map[c]
	end

    return c
	end

local function _set( n, d_type, p1, p2, p3, p4, p5, p6, p7 )
    if d_type=="pixel" then
        SCREEN_DRAWINGS[n]={"p",p1,p2,draw.make_color(p3)}--x,y,cl
        return n;
        end
    if d_type=="line" then
        SCREEN_DRAWINGS[n]={"l",p1,p2,p3,p4,draw.make_color(p5)}--x1,y1,x2,y2,cl
        return n;
        end
    if d_type=="rect" then
        SCREEN_DRAWINGS[n]={"r",p1,p2,p3,p4,draw.make_color(p5),p6}--x1,y1,x2,y2,cl,th
        return n;
        end
    if d_type=="rectf" then
        SCREEN_DRAWINGS[n]={"rf",p1,p2,p3,p4,draw.make_color(p5),draw.make_color(p6),p7}--x1,y1,x2,y2,clf,clb,th
        return n;
        end
    if d_type=="elps" then
        SCREEN_DRAWINGS[n]={"e",p1,p2,p3,p4,draw.make_color(p5)}--x,y,a,b,cl
        return n;
        end
    if d_type=="elpsf" then
        SCREEN_DRAWINGS[n]={"ef",p1,p2,p3,p4,draw.make_color(p5)}--x,y,a,b,clf
        return n;
        end
    if d_type=="string" then
        SCREEN_DRAWINGS[n]={"s",p1,p2,p3,draw.make_color(p4),draw.make_color(p5),p6,p7} --x,y,string,foreg_cl,backgr_cl,fontsizeX,fontsizeY
        return n;
        end
    return false
    end

draw.add = function( d_type, p1, p2, p3, p4, p5, p6, p7 )
    local n=table.getn(SCREEN_DRAWINGS)+1
    return _set(n, d_type, p1, p2, p3, p4, p5, p6, p7)
    end

draw.replace = function( n, d_type, p1, p2, p3, p4, p5, p6, p7 )
    draw.remove(n)
    return _set(n, d_type, p1, p2, p3, p4, p5, p6, p7)
    end

draw.get_params = function(n)
    local out={nil}
    if SCREEN_DRAWINGS[n][1] == "p" then out[1]="pixel" end
    if SCREEN_DRAWINGS[n][1] == "l" then out[1]="line" end
    if SCREEN_DRAWINGS[n][1] == "r" then out[1]="rect" end
    if SCREEN_DRAWINGS[n][1] == "rf" then out[1]="rectf" end
    if SCREEN_DRAWINGS[n][1] == "e" then out[1]="elps" end
    if SCREEN_DRAWINGS[n][1] == "ef" then out[1]="elpsf" end
    if SCREEN_DRAWINGS[n][1] == "s" then out[1]="string" end
    if (out[1]~=nil) then
        for i=2, table.getn(SCREEN_DRAWINGS[n]) do
            out[i]=SCREEN_DRAWINGS[n][i]
            end
        end
    return out
    end

draw.overdraw = function()
    for i=1,table.getn(SCREEN_DRAWINGS) do
        local p = SCREEN_DRAWINGS[i]
        if (p ~= nil) then
            local t=p[1]
            local x=p[2]
            local y=p[3]
            if t=="p" then
                draw_pixel(x,y,p[4])
                end
            if t=="l" then
                draw_line(x,y,p[4],p[5],p[6])
                end
            if t=="r" then
                draw_rect(x,y,p[4],p[5],p[6],p[7])
                end
            if t=="rf" then
                draw_rect_filled(x,y,p[4],p[5],p[6],p[7],p[8])
                end
            if t=="e" then
                draw_ellipse(x,y,p[4],p[5],p[6])
                end
            if t=="ef" then
                draw_ellipse_filled(x,y,p[4],p[5],p[6])
                end
            if t=="s" then
                draw_string(x,y,p[4],p[5],p[6],p[7],p[8])
                end
            end
        end
    end

draw.redraw = function()
    draw_clear()  --note: it's not "draw.clear()" from this module but "draw_clear()" - a lua command!
    draw.overdraw()
    end

draw.remove = function(n)
    if (n<=table.getn(SCREEN_DRAWINGS)) then
        for i=1,table.getn(SCREEN_DRAWINGS[n]) do
            SCREEN_DRAWINGS[n][i]=nil
            end
        end
    end

draw.clear = function()
    for i=1, table.getn(SCREEN_DRAWINGS) do
        draw.remove(i)
        end
    draw_clear()
    end
