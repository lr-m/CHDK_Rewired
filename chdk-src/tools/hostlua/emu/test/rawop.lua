--[[
@chdk_version 1.7.0
@title hostlua rawop test
]]
--[[
test rawop functions
usage
from tools/hostlua/emu run
../lua emu.lua test/rawop.lua -modroot=../../../CHDK
]]

serialize_r = function(v,opts,r,seen,depth)
	local vt = type(v)
	if vt == 'nil' or  vt == 'boolean' or vt == 'number' then
		table.insert(r,tostring(v))
		return
	end
	if vt == 'string' then
		local s = ('%q'):format(v)
		if opts.esc_eol then
			s = s:gsub('\\\n','\\n')
		end
		table.insert(r,s)
		return
	end
	if vt == 'table' then
		if not depth then
			depth = 1
		end
		if depth >= opts.maxdepth then
			error('serialize: max depth')
		end
		if not seen then
			seen={}
		elseif seen[v] then
			if opts.err_cycle then
				error('serialize: cycle')
			else
				table.insert(r,'"cycle:'..tostring(v)..'"')
				return
			end
		end
		seen[v] = true
		table.insert(r,'{')
		local maxi=0
		if opts.compact_arrays then
			for k,v1 in ipairs(v) do
				maxi = k
				if k == 1 and opts.pretty then
					table.insert(r,'\n'..(' '):rep(depth))
				end
				serialize_r(v1,opts,r,seen,depth+1)
				table.insert(r,',')
				local tn=type(v[k+1])
				if opts.pretty and tn ~= 'nil' and tn ~= 'table' and type(v1) == 'table' then
					table.insert(r,'\n'..(' '):rep(depth))
				end
			end
		end

		for k,v1 in pairs(v) do
			if not opts.compact_arrays or type(k) ~= 'number' or k < 1 or k > maxi then
				if opts.pretty then
					table.insert(r,'\n'..(' '):rep(depth))
				end
				if not opts.bracket_keys and type(k) == 'string' and k:match('^[_%a][%a%d_]*$') then
					table.insert(r,k)
				else
					table.insert(r,'[')
					serialize_r(k,opts,r,seen,depth+1)
					table.insert(r,']')
				end
				table.insert(r,'=')
				serialize_r(v1,opts,r,seen,depth+1)
				table.insert(r,',')
			end
		end
		if r[#r] == ',' then
			table.remove(r)
		end
		if opts.pretty then
			table.insert(r,'\n'..(' '):rep(depth-1))
		end
		table.insert(r,'}')
		return
	end
	if opts.err_type then
		error('serialize: unsupported type ' .. vt, 2)
	else
		table.insert(r,'"'..tostring(v)..'"')
	end
end
serialize_defaults = {
	maxdepth=10,
	err_type=true,
	err_cycle=true,
--	pretty=false,
	pretty=true,
	bracket_keys=false,
	compact_arrays=true,
	esc_eol=false,
}
function serialize(v,opts)
	if opts then
		for k,v in pairs(serialize_defaults) do
			if opts[k] == nil then
				opts[k]=v
			end
		end
	else
		opts=serialize_defaults
	end
	local r={}
	serialize_r(v,opts,r)
	return table.concat(r)
end

function printf(fmt,...)
	io.stdout:write(fmt:format(...))
end

printf('cfa 0x%08x\n',rawop.get_cfa())
printf('cfa_offsets %s\n',serialize(rawop.get_cfa_offsets()))
printf('bits_per_pixel %d\n',rawop.get_bits_per_pixel())
printf('raw_neutral %d\n',rawop.get_raw_neutral())
printf('raw_black_level %d\n',rawop.get_black_level())
printf('raw_white_level %d\n',rawop.get_white_level())
printf('raw size %dx%d\n',rawop.get_raw_width(),rawop.get_raw_height())
printf('active area %d,%d %dx%d\n',rawop.get_active_left(),rawop.get_active_top(),
										rawop.get_active_width(),rawop.get_active_height())
printf('jpeg area %d,%d %dx%d\n',rawop.get_jpeg_left(),rawop.get_jpeg_top(),
										rawop.get_jpeg_width(),rawop.get_jpeg_height())

assert(rawop.get_pixel(100,100) == rawop.get_raw_neutral())
assert(rawop.get_pixel(99999,99999) == nil)
rawop.set_pixel(0,0,1)
t={rawop.get_pixels_rgbg(100,100)}
assert(t[1] == rawop.get_raw_neutral() and t[3] == rawop.get_raw_neutral())
rawop.set_pixels_rgbg(1,1,1000,1000,1000,1000)
rawop.fill_rect(100,100,1000,1000,2000,1,1)
assert(rawop.meter(100,100,200,200,5,5) == rawop.get_raw_neutral())
-- no pixels
assert(rawop.meter(100,100,0,0,5,5) == nil)
-- size too big
assert(rawop.meter(100,100,16000,16000,100,100) == nil)
-- count too big
assert(rawop.meter(100,100,4000,4000,1,1) == nil)

printf('raw_to_ev(neutral) %d\n',rawop.raw_to_ev(rawop.get_raw_neutral()))
printf('raw_to_ev(black + 1) %d\n',rawop.raw_to_ev(rawop.get_black_level()+1))
printf('raw_to_ev(white) %d\n',rawop.raw_to_ev(rawop.get_white_level()))
assert(rawop.raw_to_ev(rawop.get_black_level()) == rawop.raw_to_ev(rawop.get_black_level()+1))
assert(rawop.raw_to_ev(rawop.get_raw_neutral()) == 0)

printf('ev_to_raw(0) %d\n',rawop.ev_to_raw(0))
assert(rawop.ev_to_raw(0)==rawop.get_raw_neutral())
histo=rawop.create_histogram()
histo:update(100,100,500,500,3,3)
printf('h:bits %d\n',histo:bits())
printf('h:total_pixels %d\n',histo:total_pixels())
printf('h:range(0,%d) %d\n',rawop.get_white_level(),histo:range(0,rawop.get_white_level()))
printf('h:range(0,%d,"count") %d\n',rawop.get_white_level(),histo:range(0,rawop.get_white_level(),'count'))
printf('h:range(0,%d) %d\n',rawop.get_raw_neutral()-1,histo:range(0,rawop.get_raw_neutral()-1))
printf('h:range(%d,%d) %d\n',rawop.get_raw_neutral(),rawop.get_raw_neutral(),histo:range(rawop.get_raw_neutral(),rawop.get_raw_neutral()))
