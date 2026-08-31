--[[
module providing functions that work with zoom position and focal length
--]]

local zu={}
--[[
helper to use the same logic for fl and efl
]]
local function get_zoom_for_xfl(fl, fl_fn)
    local zp
    local zpfl_prev

	-- HACK
	-- EOS M cameras don't have zoom pos
	if get_buildinfo().platform:sub(1,1) == 'm' then
		return 0
	end
    for zp = 0, get_zoom_steps()-1 do
        local zpfl = fl_fn(zp)
        if zpfl >= fl then
            -- first zoom point is already larger focal length than requested
            if zp == 0 then
                return zp
            end
            -- check if previous is closer
            if zpfl - fl > fl - zpfl_prev then
                return zp - 1
            else
                return zp;
            end
        end
        zpfl_prev = zpfl
    end
    return get_zoom_steps() - 1;
end

--[[
pos = zu.get_zoom_for_fl(focal_length)
focal_length: desired focal length, expressed as mm*1000
pos: zoom position with focal length closest to specified value
]]
function zu.get_zoom_for_fl(fl)
	return get_zoom_for_xfl(fl, get_focal_length)
end

--[[
pos = zu.get_zoom_for_efl(eff_focal_length)
eff_focal_length: desired 35mm equivalent focal length, expressed as mm*1000
pos: zoom position with 35mm focal length closest to specified value
]]
function zu.get_zoom_for_efl(fl)
	return get_zoom_for_xfl(fl, get_eff_focal_length)
end

--[[
zu.set_zoom_fl(focal_length)
focal_length: desired focal length, expressed as mm*1000
sets zoom to the zoom point closest to the specified fl
]]
function zu.set_zoom_fl(fl)
	return set_zoom(zu.get_zoom_for_fl(fl))
end

--[[
zu.set_zoom_efl(eff_focal_length)
focal_length: desired focal length, expressed as mm*1000
sets zoom to the zoom point closest to the specified efl
]]
function zu.set_zoom_efl(fl)
	return set_zoom(zu.get_zoom_for_efl(fl))
end


local efl_factor

--[[
return the multiplier to convert a focal length value to an 35mm equivalent value, as
an fmath floating point number
]]
function zu.get_efl_factor()
	if not efl_factor then
		efl_factor = fmath.new(get_eff_focal_length(0))/fmath.new(get_focal_length(0))
	end
	return efl_factor
end

--[[
convert a focal length value to 35mm equivalent
return an integer scaled by scale, default 1

note converted values may not precisely match the values returned by
get_eff_focal_length() / get_eff_focal_length()
]]
function zu.fl_to_efl(fl, scale)
	scale = scale or 1
	return (fmath.new(fl)*zu.get_efl_factor()*scale):round()
end

--[[
convert a 35mm equivalent focal length value to focal length
return an integer scaled by scale, default 1

note converted values may not precisely match the values returned by
get_eff_focal_length() / get_eff_focal_length()
]]
function zu.efl_to_fl(fl, scale)
	scale = scale or 1
	return (fmath.new(fl)/zu.get_efl_factor()*scale):round()
end

function zu.get_min_fl()
	return get_focal_length(0)
end

function zu.get_min_efl()
	return get_eff_focal_length(0)
end

function zu.get_max_fl()
	return get_focal_length(get_zoom_steps()-1)
end

function zu.get_max_efl()
	return get_eff_focal_length(get_zoom_steps()-1)
end

return zu
