--[[
 Copyright (C) 2010-2024 <reyalp (at) gmail dot com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License version 2 as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  with chdk. If not, see <http://www.gnu.org/licenses/>.

--]]
--[[
select functions modified from chdkptp trunk r1513
]]
local util={}

--[[
to allow overriding, e.g. for gui and tests
--]]
util.util_stderr = io.stderr
util.util_stdout = io.stdout

-- return version components as numbers
-- _VERSION does not usually contain final (release) number
function util.lua_version()
	if type(_VERSION) ~= 'string' then
		error('missing _VERSION')
	end
	local major,minor = string.match(_VERSION,'(%d+)%.(%d+)')
	return tonumber(major),tonumber(minor)
end

util.lua_ver_major,util.lua_ver_minor = util.lua_version()

function util.is_lua_ver(major,minor)
	return (major == util.lua_ver_major and minor == util.lua_ver_minor)
end

function util.fprintf(f,...)
	local args={...}
	if #args == 0 or type(args[1]) ~= 'string' then
		args[1]=''
	end
--	f:write(string.format(table.unpack(args)))
-- unpack is a global function in 5.1
	f:write(string.format(unpack(args)))
end

function util.printf(...)
	util.fprintf(util.util_stdout,...)
end

function util.warnf(format,...)
	util.fprintf(util.util_stderr,"WARNING: " .. format,...)
end

function util.errf(format,...)
	util.fprintf(util.util_stderr,"ERROR: " .. format,...)
end

--[[
split str delimited by pattern pat, or plain text if opts.plain
pat defaults to spaces ('%s+')
empty string '' splits chars
leading / trailing delimiters generate empty strings
with func, iterate over split strings
]]
function util.string_split(str,pat,opts)
	opts = util.extend_table({
		plain=false,
		start=1,
		empty=true, -- include empty strings from multiple / trailing delimiters (default func only)
	},opts)
	local r = {}
	local pos = opts.start
	local s,e
	if not pat then
		pat = '%s+'
	end
	if not opts.func then
		opts.func = function(v)
			if string.len(v) > 0 or opts.empty then
				table.insert(r,v)
			end
		end
	end
	if string.len(pat) == 0 then
		while true do
			local c = string.sub(str,pos,pos)
			if string.len(c) == 0 then
				return r
			end
			pos = pos+1
			opts.func(c)
		end
	end

	while true do
		s, e = string.find(str,pat,pos,opts.plain)
		if not s then
			opts.func(string.sub(str,pos,-1))
			break
		end
		opts.func(string.sub(str,pos,s-1))
		pos = e + 1
	end

	return r
end

--[[
return string with trailing pattern 'pat' (default %s+, space) removed
]]
function util.string_rtrim(str,pat)
	if not pat then
		pat = '%s+'
	end
	local s, e = str:find(pat..'$')
	if s then
		str = str:sub(1,s-1)
	end
	return str
end

--[[
return string with leading pattern 'pat' (default %s+, space) removed
]]
function util.string_ltrim(str,pat)
	if not pat then
		pat = '%s+'
	end
	local s,e = str:find('^'..pat)
	if s then
		str = str:sub(e+1)
	end
	return str
end

--[[
return string with leading and trailing pattern 'pat' (default %s+, space) removed
]]
function util.string_trim(str,pat)
	return util.string_rtrim(util.string_ltrim(str,pat),pat)
end

util.extend_table_max_depth = 10
local extend_table_r
extend_table_r = function(target,source,seen,depth)
	if not seen then
		seen = {}
	end
	if not depth then
		depth = 1
	end
	if depth > util.extend_table_max_depth then
		error('extend_table: max depth');
	end
	-- a source could have references to the target, don't want to do that
	seen[target]=true
	if seen[source] then
		error('extend_table: cycles');
	end
	seen[source]=true
	for k,v in pairs(source) do
		if type(v) == 'table' then
			if type(target[k]) ~= 'table' then
				target[k] = {}
			end
			extend_table_r(target[k],v,seen,depth+1)
		else
			target[k]=v
		end
	end
	return target
end

--[[
copy members of source into target
by default, not deep so any tables will be copied as references
returns target so you can do x=extend_table({},...)
opts
	deep=bool - copy recursively
	keys={key,key...} - copy only specified subset of keys, if key in source is unset, target is unchanged
	                  - only applied to the top level with deep
	iter=function - iterator function to use instead of pairs. Not used with keys or deep
if deep, cycles result in an error
deep does not copy keys which are themselves tables (the key will be a reference to the original key table)
]]
function util.extend_table(target,source,opts)
	if not opts then
		opts={}
	end
	if type(target) ~= 'table' then
		error('extend_table: target not table')
	end
	if source == nil then -- easier handling of default options
		return target
	end
	if type(source) ~= 'table' then
		error('extend_table: source not table')
	end
	if source == target then
		error('extend_table: source == target')
	end
	if opts.keys then -- copy only specific keys
		for i,k in ipairs(opts.keys) do
			if type(source[k]) == 'table' and opts.deep then
				if type(target[k]) ~= 'table' then
					target[k]={}
				end
				extend_table_r(target[k],source[k])
			elseif source[k] ~= nil then
				target[k] = source[k]
			end
		end
	elseif opts.deep then
		return extend_table_r(target, source)
	else
		local iter = opts.iter or pairs
		for k,v in iter(source) do
			target[k]=v
		end
	end
	return target
end

--[[
does table have value in it ?
]]
function util.in_table(t,value)
	if t == nil then
		return false
	end
	for k,v in pairs(t) do
		if v == value then
			return true
		end
	end
end

return util
