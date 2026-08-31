--[[
 Copyright (C) 2021-2024 <reyalp (at) gmail dot com>

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
module for loading / manipulating camera scripts and saved script settings
https://chdk.fandom.com/wiki/CHDK_Script_Header

adapted from chdkptp r1514
--]]

local errlib=require'errlib_compat'
local util=require'util'
local fsutil=require'fsutil'

local m={
	header_scan_len = 4096, -- CHDK scans the first 4k only
	header_keywords = {
		'title',
		'chdk_version',
		'subtitle',
		'param',
		'default',
		'range',
		'values',
	},
}
local header_methods = {}

m.header_methods = header_methods

local item_methods = {}

m.item_methods = item_methods

-- allow override for tests
m.warnf = util.warnf


local item_type_methods = {
	subtitle={},
	short={},
	long={},
	table={},
	enum={},
	bool={},
}
m.item_type_methods = item_type_methods

--[[
parse a saved settings file (CHDK/DATA/script.N) into a table of name=val
format is just
#name=decimal number
]]
function m.parse_saved_cfg(str)
	local r={}
	for lnum,line in ipairs(util.string_split(str,'[\r\n]',{empty=false})) do
		local name, val = line:match('#([%a][%a%d_]*)=([%d-]+)$')
		if not name then
			errlib.throw{etype='parse',msg=('failed to parse line %s: %s'):format(lnum,line)}
		end
		val = tonumber(val)
		if not val then
			errlib.throw{etype='parse',msg=('failed to parse value line %s: %s'):format(lnum,line)}
		end
		r[name] = val
	end
	return r
end

function item_methods:merge(item_init)
	util.extend_table(self,item_init)
	if self.defval ~= nil then
		self.val = self.defval
	end
	if self.header.cur_line then
		table.insert(self.orig_lines,self.header.cur_line)
	end
end

--[[
old style params are over multiple lines in arbitrary order
so add type specific methods after parse
]]
function item_methods:finalize_type()
	-- old style enums aren't recognized at parse time
	if self.deftype == '@' then
		if self.items then
			self.paramtype = 'enum'
		end
		-- a param with no default will not have type or value
		if self.defval == nil and self.paramtype == nil then
			self.header:warnf('item %s missing default',self.name)
			self.paramtype = 'short'
			-- CHDK defaults such value to 0
			self.defval = 0
			self.val = 0
		end
		if not self.desc then
			-- TODO this breaks the script menu so should be fatal
			self.header:warnf('item %s missing description',self.name)
			self.desc = ''
		end
	end
	if not item_type_methods[self.paramtype] then
		errlib.throw{etype='internal_error',msg=('%s invalid item type %s'):format(tostring(self.name),tostring(self.paramtype))}
	end
	util.extend_table(self,item_type_methods[self.paramtype])
end

function item_methods:format_glue(opts)
	opts = util.extend_table({},opts)
	if util.in_table({'short','long','bool','enum'},self.paramtype) then
		return ('%s=%s'):format(self.name,tostring(self.val))
	end
	if self.paramtype == 'table' then
		return ('%s={\n index=%s,\n value="%s"\n}'):format(
			self.name,self.val,tostring(self.items[self.val]))
	end
	if opts.subtitles then
		if self.paramtype == 'subtitle' then
			return ('-- @subtitle %s'):format(self.val)
		end
	end
end

function item_type_methods.subtitle:format()
	if self.val == '' then
		return '@subtitle\n'
	end
	return ('@subtitle %s\n'):format(self.val)
end

function item_type_methods.subtitle:format_shorthand()
	return self:format()
end

function item_type_methods.subtitle:format_old()
	return self:format()
end

function item_methods:format_shorthand()
	-- item descriptions can be quoted with ' or ", and may contain the other
	local qchar = '"'
	if self.desc:match('"') then
		qchar = "'"
	end
	local r=('#%s=%s %s%s%s'):format(self.name,tostring(self.val),qchar,self.desc,qchar)
	if self.paramtype == 'short' then
		if self.range then
			r = ('%s [%s %s]'):format(r,self.range[1],self.range[2])
		end
	elseif self.paramtype == 'long' then
		r = r..' long'
	elseif self.items then
		r = ('%s {%s}'):format(r,table.concat(self.items,' '))
		if self.paramtype == 'table' then
			r = r..' table'
		end
	-- bool doesn't need any addition
	elseif self.paramtype ~= 'bool' then
		self.header:err('internal_error',tostring(self.name)..' unexpected item type '..tostring(self.paramtype))
	end
	return r..'\n'
end

function item_methods:format_old()
	-- these don't have equivalent in @ format
	if self.paramtype == 'table' or self.paramtype == 'long' then
		return self:format_shorthand()
	end
	local r=([[
@param %s %s
@default %s %s
]]):format(self.name,self.desc,self.name,tostring(self.val))
	if self.range then
		r = r..('@range %s %s %s\n'):format(self.name,self.range[1],self.range[2])
	end
	if self.paramtype == 'enum' then
		r = r..('@values %s %s\n'):format(self.name,table.concat(self.items,' '))
	end
	return r
end

--[[
get the numeric value that would appear CHDK saved parameters cfg
]]
function item_methods:get_cfg_val()
	return self.val
end

-- table indexes stored 0 based
function item_type_methods.table:get_cfg_val()
	return self.val - 1
end

-- bools are stored as integers
function item_type_methods.bool:get_cfg_val()
	return ({[false]=0,[true]=1})[self.val]
end

function item_type_methods.subtitle:get_cfg_val()
	return -- nil
end


function item_type_methods.subtitle:format_saved_cfg()
	return ''
end

function item_methods:format_saved_cfg()
	return ('#%s=%d\n'):format(self.name,self:get_cfg_val())
end

function item_type_methods.subtitle:check_value(val)
	if type(val) ~= 'string' then
		return false,('expected string, not %s'):format(type(val))
	end
	return true
end

function item_type_methods.short:check_value(val)
	if type(val) ~= 'number' then
		return false,('item %s expected number, not %s'):format(self.name,type(val))
	end
	if self.range and #self.range > 0 and (val < self.range[1] or val > self.range[2]) then
		return false,('item %s value %s out of range %s %s'):format(self.name,val,self.range[1],self.range[2])
	end
	if val < -9999  or val > 99999 then
		return false,('item %s invalid value %s'):format(self.name,val)
	end
	return true
end

function item_type_methods.long:check_value(val)
	if type(val) ~= 'number' then
		return false,('item %s expected number, not %s'):format(self.name,type(val))
	end
	if val < 0  or val > 9999999 then
		return false,('item %s invalid value %s'):format(self.name,val)
	end
	return true
end

function item_type_methods.table:check_value(val)
	if type(val) ~= 'number' then
		return false,('item %s expected number, not %s'):format(self.name,type(val))
	end
	-- assume value is already adjusted from 0 based
	if val < 1  or val > #self.items then
		return false,('item %s table value %s out of range %s %s'):format(self.name,val,1,#self.items)
	end
	return true
end

function item_type_methods.enum:check_value(val)
	if type(val) ~= 'number' then
		return false,('item %s expected number, not %s'):format(self.name,type(val))
	end
	if val < 0  or val >= #self.items then
		return false,('item %s enum value %s out of range %s %s'):format(self.name,val,0,#self.items-1)
	end
	return true
end

function item_type_methods.bool:check_value(val)
	if type(val) ~= 'boolean' then
		return false,('item %s expected boolean, not %s'):format(self.name,type(val))
	end
	return true
end

--[[
check current value and warn or error
]]
function item_methods:validate_value(val,err_mode)
	local status, err = self:check_value(val)
	if not status then
		if err_mode == 'error' then
			errlib.throw{etype='bad_value',msg=err}
		elseif err_mode == 'warn' then
			self.header:warnf(err)
		end
	end
	return status,err
end

function item_methods:validate_current_value()
	return self:validate_value(self.val,'warn')
end

function item_methods:set_value(val,opts)
	opts = util.extend_table({err_mode = 'error'},opts)
	self:validate_value(val,opts.err_mode)
	self.val = val
end

function item_methods:set_value_str(sval)
	local v = tonumber(sval)
	if not v then
		errlib.throw{etype='bad_value',msg=('item %s expected number, not %s'):format(self.name,sval)}
	end
	self:set_value(v)
end

function item_type_methods.bool:set_value_str(sval)
	local v = ({['true']=true,['false']=false,['1']=true,['0']=false})[sval:lower()]
	if v == nil then
		errlib.throw{etype='bad_value',msg=('item %s expected boolean, not %s'):format(self.name,sval)}
	end
	self:set_value(v)
end

--[[
set value by enum / table value string
]]
function item_methods:set_value_by_item(val)
	if self.paramtype ~= 'table' and self.paramtype ~= 'enum' then
		errlib.throw{etype='bad_value',msg=('item %s not a table or enum'):format(self.name)}
	end
	for i,v in ipairs(self.items) do
		if v == val then
			if self.paramtype == 'enum' then
				self:set_value(i - 1)
			else
				self:set_value(i)
			end
			return
		end
	end
	errlib.throw{etype='bad_value',msg=('item %s value %s not found'):format(self.name,val)}
end

--[[
throw error with automatic line number + line when available
]]
function header_methods:err(etype,msg)
	if self.cur_line then
		msg=('%s line %s: %s'):format(msg,self.cur_lnum,self.cur_line)
	end
	errlib.throw({etype=etype,msg=msg},3)
end

--[[
warn with automatic line number + line when available
]]
function header_methods:warn(msg)
	if self.cur_line then
		m.warnf('%s line %s: %s\n',msg,self.cur_lnum,self.cur_line)
	else
		m.warnf('%s\n',msg)
	end
end

function header_methods:warnf(fmt,...)
	self:warn(string.format(fmt,...))
end

function header_methods:new_item(item_init)
	local item = util.extend_table({
		orig_lines = {},
		header = self,
	},m.item_methods)
	table.insert(self.items,item)
	self.by_name[item_init.name] = item
	item:merge(item_init)
	return item
end

function header_methods:add_item(item)
	-- CHDK probably accepts dupes, specific behavior not tested
	if self.by_name[item.name] then
		self:err('bad_arg','duplicate item '..item.name)
	end
	self:merge_item(item)
end

function header_methods:merge_item(item_init)
	-- @ params can have multiple in lines referring to the same item
	-- in any order, so merge creates if non-existing
	local item = self.by_name[item_init.name]
	if not item then
		self:new_item(item_init)
	else
		item:merge(item_init)
	end
end

--[[
parse a number, for value or range
chdk uses strtol which (if standard) accepts 0x<hex> 0<octal> or dec
tonumber would also accept floats, but '.' should not be matched by
patterns that extract numbers
]]
function header_methods:parse_number(str)
	str = util.string_trim(str)
	if str:match('^0[xX]') then -- hex
		return tonumber(str)
	elseif str:sub(1,1) == '0' then
		return tonumber(str,8) -- octal
	else
		return tonumber(str) -- dec
	end
end

--[[
parse a value default, for both new and old style
CHDK code uses same function for both
returns name, value, rest
]]
function header_methods:parse_default(str)
	-- CHDK allows space between # and item name, and multiple spaces and = between name and value
	-- value can be decimal, hex, octal, 'true', or 'false'
	return str:match('([%a][%a%d_]*)[%s=]+([%a%d-]+)(.*)')
end

--[[
extract keywords specified in keywords array from the start of str
returns keyword, rest
if not found, keyword is nil and rest is the entire string
spaces are trimmed from rest
roughly mirrors strncmp logic used in CHDK, which does not
require trailing space after most recognized keywords
]]
function header_methods:parse_keyword(str,keywords)
	for i, kw in ipairs(keywords) do
		if str:sub(1,kw:len()) == kw then
			return kw,util.string_trim(str:sub(kw:len()+1))
		end
	end
	return nil,util.string_trim(str)
end

function header_methods:parse_item_new()
	local line = self.cur_line
	-- CHDK allows space between # and variable name, trim
	local rest = util.string_ltrim(line,'#%s*')

	local name, defval, rest = self:parse_default(rest)
	if not name then
		-- TODO warn / error? #foo can appear in lua code but unlikely to match
		return
	end
	rest = util.string_trim(rest)
	-- quoted description is required
	-- either ' or " can be used, and the other can appear in content
	-- space is not required after the close quote
	local desc
	if rest:sub(1,1) == "'" then
		desc, rest = rest:match("^'([^']+)'%s*(.*)")
	elseif rest:sub(1,1) == '"' then
		desc, rest = rest:match('^"([^"]+)"%s*(.*)')
	end

	if not desc then
		self:err('parse','failed to parse description')
	end
	local paramtype
	local val
	local range
	local items
	if defval == 'true' or defval == 'false' then
		paramtype = 'bool'
		val = (defval == 'true')
		local kw, rest = self:parse_keyword(rest,{'bool'}) -- redundant 'bool' is allowed
		if rest ~= '' then
			self:warn('unexpected content after bool')
		end
	else
		val = self:parse_number(defval)
		if not val then
			self:err('parse','failed to parse value')
		end
		local c = rest:sub(1,1)
		if c == '[' then
			local minstr, maxstr, rest = rest:match('%[%s*([%dA-Fa-fxX-]+)%s+([%dA-Fa-fxX-]+)%s*%](.*)$')
			if not minstr then
				self:err('parse','failed to parse range')
			end
			range = {self:parse_number(minstr),self:parse_number(maxstr)}
			if #range < 2 then
				self:err('parse','failed to parse range')
			end
			if rest ~= '' then
				self:warn('unexpected content after range')
			end
			paramtype = 'short'
		elseif c == '{' then
			local enumstr, rest = rest:match('%{([^}]*)%}(.*)$')
			if not enumstr then
				self:err('parse','failed to parse enum / table')
			end
			rest = util.string_trim(rest)
			local kw, rest = self:parse_keyword(rest,{'table'})
			if kw == 'table' then
				paramtype = 'table'
			else
				paramtype = 'enum'
			end
			if rest ~= '' then
				self:warn('unexpected content after table')
			end
			items = util.string_split(enumstr,'%s+',{empty=false})
		else
			local kw, rest = self:parse_keyword(rest,{'long','bool'})
			if kw == 'long' then
				paramtype = 'long'
			elseif kw == 'bool' then
				paramtype = 'bool'
				val = (val ~= 0)
			else
				paramtype = 'short'
			end
			if rest ~= '' then
				self:warn('unexpected trailing content')
			end
		end
	end
	self:add_item({
		deftype = '#',
		desc = desc,
		paramtype = paramtype,
		name = name,
		defval = val,
		range = range,
		items = items,
	})

end

function header_methods:parse_item_old()
	local line = self.cur_line
	local rest = line:sub(2) -- remove @
	local keyword,rest = self:parse_keyword(rest,m.header_keywords)
	if not keyword then
		self:warn('unknown @keyword')
		return
	end
	local val
	local paramtype
	if keyword == 'title' then
		self.title = rest
		return
	end
	if keyword == 'chdk_version' then
		self.chdk_version = rest
		local ver_parts = util.string_split(rest,'.',{empty=false,plain=true})
		if #ver_parts < 2 or #ver_parts > 4 then
			self:warn('failed to parse version %s',rest)
			return
		end
		local vmap = {'major','minor', 'sub', 'build'}
		for i, s in ipairs(ver_parts) do
			local v = tonumber(s)
			if not v then
				self:warn('failed to parse version item %d %s',i,s)
				v = 0
			end
			self.chdk_version_req[vmap[i]] = v
		end
		return
	end
	if keyword == 'subtitle' then
		self.subtitle_count = self.subtitle_count + 1
		-- subtitles are one line, have unique generated names, so no merge
		self:add_item({
			deftype = '@',
			paramtype = 'subtitle',
			name = 'subtitle'..self.subtitle_count,
			defval = rest,
			desc = '',
		})
		return
	end

	local desc, name, paramtype, range, val, items
	if keyword == 'default' then
		name, val, rest = self:parse_default(rest)
		if not name then
			self:err('parse','failed to parse param')
		end

		if val == 'true' or val == 'false' then
			val = (val == 'true')
			paramtype = 'bool'
		else
			val = self:parse_number(val)
			if not val then
				self:err('parse','failed to parse default value')
			end
			paramtype = 'short'
		end
	else
		name, rest=rest:match('^([%a%d_]+)%s(.*)$')
		if not name then
			self:err('parse','failed to parse param')
		end
		rest = util.string_trim(rest)
		if keyword == 'param' then
			desc = rest
		elseif keyword == 'range' then
			local parts = util.string_split(rest,'%s+',{empty=false})
			if #parts ~= 2 then
				self:err('parse','failed to parse range')
			end
			range = {self:parse_number(parts[1]),self:parse_number(parts[2])}
			if #range < 2 then
				self:err('parse','failed to parse range')
			end
			paramtype = 'short'
		elseif keyword == 'values' then
			items = util.string_split(rest,'%s+',{empty=false})
		end
	end
	-- @ syntax appears to allow keywords in any order
	-- so all are merged
	self:merge_item({
		deftype = '@',
		name = name,
		desc = desc,
		paramtype = paramtype,
		range = range,
		defval = val,
		items = items,
	})
end

function header_methods:merge_saved_cfg(saved)
	if type(saved) == 'string' then
		saved = m.parse_saved_cfg(saved)
	end
	for i, item in ipairs(self.items) do
		if item.paramtype ~= 'subtitle' then
			if saved[item.name] then
				local v = saved[item.name]
				-- table values are 1 based, but stored 0 based
				if item.paramtype == 'table' then
					v = v + 1
				-- convert bool lua boolean
				elseif item.paramtype == 'bool' then
					v = (v ~= 0)
				end
				item.val = v
				-- warn if value invalid
				-- TODO may want to optionally ignore out of range
				item:validate_current_value()
			else
				self:warnf('saved settings missing %s',item.name)
			end
		end
	end
	for name,val in pairs(saved) do
		if not self.by_name[name] then
			self:warnf('saved setting %s missing in header',name)
		end
	end
end

function header_methods:parse(str)
	-- empty allows line numbers to match
	for lnum,line in ipairs(util.string_split(str,'[\r\n]',{empty=true})) do
		line = util.string_trim(line)
		local c = line:sub(1,1)
		-- for availability in errors/warnings
		self.cur_line = line
		self.cur_lnum = lnum
		if c == '#' then
			self:parse_item_new()
		elseif c == '@' then
			self:parse_item_old()
		end
	end
	self.line = nil
	self.cur_line = nil
	for i, item in ipairs(self.items) do
		-- @ params are defined over multiple lines, try
		-- to fix up enum values and sanity check
		item:finalize_type()
		item:validate_current_value()
	end
end

function header_methods:get_glue_content(opts)
	opts = util.extend_table({
		comment=true,
	},opts)
	local r={}
	if opts.comment then
		if self.title then
			table.insert(r,('-- @title %s'):format(self.title))
		end
		if self.chdk_version then
			table.insert(r,('-- @chdk_version %s'):format(self.chdk_version))
		end
	end
	for i, item in ipairs(self.items) do
		if opts.comment then
			if #item.orig_lines > 0 then
				table.insert(r,'-- '..table.concat(item.orig_lines,'\n-- '))
			else
				table.insert(r,'-- %s desc',item.name,item.desc)
			end
		end
		local s = item:format_glue()
		if s then
			table.insert(r,s)
		end
	end
	return table.concat(r,'\n') .. '\n'
end

--[[
make glue file content from a template and camera side script. Templates can use the
following tokens, which must appear on a line with only leading/trailing whitespace
--[!glue:start] causes everything preceding to discarded, allowing
--              documentation to appear in the template without appearing in output
--[!glue:vars] is replaced by the menu glue content, which sets CHDK menu variables
--[!glue:body] is replaced by the content of the script or a loadfile
               line if opts.camfile is set

If either token is not present, the corresponding content is appended,
so an empty gluetpl is the glue followed by the script.

opts: {
	camfile:string  -- Camera side filename to run with loadfile()
					-- If a directory is not included, A/CHDK/SCRIPTS prepended
					-- if not set, script is included directly
	comment_glue:bool -- use comments to delimit menu and body glue
	load_compat:bool -- add require line based on chdk_version, if needed
	other passed to get_glue_content
}
]]
function header_methods:make_glue_tpl(gluetpl,opts)
	gluetpl = gluetpl or ''
	opts = util.extend_table({
		comment_glue=true,
		load_compat=true,
	},opts)
	-- will join with \n later, ensure trailing \n removed
	local glue_content = util.string_rtrim(self:get_glue_content(opts),'\n')
	if opts.comment_glue then
		glue_content = ('-- BEGIN menu glue\n%s\n-- END menu glue'):format(glue_content)
	end
	local glue_body
	if opts.camfile then
		if not fsutil.dirname_cam(opts.camfile) then
			glue_body = fsutil.joinpath_cam('A/CHDK/SCRIPTS',opts.camfile)
		else
			glue_body = fsutil.make_camera_path(opts.camfile)
		end
		glue_body = ('loadfile("%s")()\n'):format(glue_body)
	else
		glue_body = self.script
	end
	glue_body = util.string_rtrim(glue_body,'\n')
	-- this would break if the camera was running CHDK < 1.4, but should be rare
	if opts.load_compat and self.chdk_version_req.major == 1 and self.chdk_version_req.minor <= 3 then
		glue_body = ('require"wrap13"\n%s'):format(glue_body)
	end
	if opts.comment_glue then
		glue_body = ('-- BEGIN glued script\n%s\n-- END glued script'):format(glue_body)
	end
	-- parse template by lines because lua patterns make it hard to match in one gsub
	local parts = {}
	local glue_done, body_done
	for lnum,line in ipairs(util.string_split(gluetpl,'[\r\n]',{empty=false})) do
		local s = util.string_trim(line)
		if s == '--[!glue:start]' then
			if glue_done or body_done then
				errlib.throw{etype='parse',msg=('line %d: glue:start after glue:vars or glue:body'):format(lnum)}
			end
			parts = {} -- discard everything up to start
		elseif s == '--[!glue:vars]' then
			if glue_done then
				errlib.throw{etype='parse',msg=('line %d: multiple glue:vars'):format(lnum)}
			end
			table.insert(parts,glue_content)
			glue_done = true
		elseif s == '--[!glue:body]' then
			if body_done then
				errlib.throw{etype='parse',msg=('line %d: multiple glue:body'):format(lnum)}
			end
			if not glue_done then
				errlib.throw{etype='parse',msg=('line %d: glue:body before glue:vars'):format(lnum)}
			end
			table.insert(parts,glue_body)
			body_done = true
		else
			table.insert(parts,line)
		end
	end
	-- default to append
	if not glue_done then
		table.insert(parts,glue_content)
	end
	if not body_done then
		table.insert(parts,glue_body)
	end
	return table.concat(parts,'\n') .. '\n'
end

function header_methods:make_glue_file(tplfile,dest,opts)
	local tpl
	--tplfile is optional, if not specified, output is equivalent to a file containing
	-- --[!glue:vars]
	-- --[!glue:body]
	if tplfile then
		tpl = fsutil.readfile(tplfile)
	end
	glue = self:make_glue_tpl(tpl,opts)
	fsutil.writefile(dest,glue,{bin=true})
end

--[[
return string with values in the format of CHDK/DATA/SCRIPTNAME.N
]]
function header_methods:make_saved_cfg()
	local r={}
	for i, item in ipairs(self.items) do
		if item.paramtype ~= 'subtitle' then
			table.insert(r,item:format_saved_cfg())
		end
	end
	return table.concat(r)
end

--[[
return string of defined values as script header
opts {
	format:string|nil -- '#' all in shorthand format
					  -- '@' all in @ format, except where no @ format is defined
					  -- otherwise, as originally defined
]]
function header_methods:make_header(opts)
	opts=util.extend_table({},opts)
	local r={}
	if self.title then
		table.insert(r,('@title %s\n'):format(self.title))
	end
	if self.chdk_version then
		table.insert(r,('@chdk_version %s\n'):format(self.chdk_version))
	end
	local fmt = opts.format
	for i, item in ipairs(self.items) do
		if not opts.format then
			fmt = item.deftype
		end
		if fmt == '#' then
			table.insert(r,item:format_shorthand())
		else
			table.insert(r,item:format_old())
		end
	end
	return table.concat(r)
end

--[[
set values from an array of {name,value}
for easy use from CLI commands
if name ends in .value, set_value_by_item is used
if value is a string, set_value_str is used
throws on error
]]
function header_methods:set_values(vals)
	for i, vdef in ipairs(vals) do
		local name = vdef[1]
		local val = vdef[2]
		local by_item
		if name:match('%.value$') then
			name = name:sub(1,-7)
			by_item = true
		end
		local item = self.by_name[name]
		if not item then
			self:err('bad_arg','unknown item '..tostring(name))
		end
		if by_item then
			item:set_value_by_item(val)
		elseif type(val) == 'string' then
			item:set_value_str(val)
		else -- other invalid types should be caught in set
			item:set_value(val)
		end
	end
end

function header_methods:set_script(s)
	self.items = {} -- array to preserve order
	self.by_name = {} -- names for easy lookup
	self.subtitle_count = 0 -- to generate unique names for "subtitle" items
	self.script = s
	self.hdr = s:sub(1,m.header_scan_len) -- CHDK code truncates header
	-- default to 1.3, as CHDK. if @chdk_version is found will override
	self.chdk_version_req = {major = 1, minor = 3, sub = 0, build = 0}
	self:parse(self.hdr)
end

--[[
hdr=m.new_header(opts)

opts:
string containing header or table
opts={
	file="filename"
	text="header content"
	cfgfile="filename" -- saved settings file to merge
	cfgtext="saved settings content" -- saved settings content as string
--	below not applicable to  hostlua
--	allow_cam_files=boolean -- load script and settings files from camera when
	                        -- path starts with 'A/', default true
--	con=connection -- connection for cam files, default global con
}

returns object representing parsed header
throws on error
]]

function m.new_header(opts)
	-- if opts is just a string, treat it as script/header content
	if type(opts) == 'string' then
		opts = {text=opts}
	end
	opts = util.extend_table({
--	not applicable to hostlua
--		allow_cam_files=true,
--		con=con,
	},opts)

	local script_content
	if opts.file then
		-- hostlua doesn't support remote
--		if opts.allow_cam_files then
--			script_content = anyfs.readfile(opts.file,{con=opts.con})
--		else
--			script_content = fsutil.readfile(opts.file)
--		end
		script_content = fsutil.readfile(opts.file)
	elseif opts.text then
		script_content = opts.text
	end
	local t = util.extend_table({},header_methods)
	t:set_script(script_content)
	local cfgtext
	if opts.cfgfile then
		-- hostlua doesn't support remote
--		if opts.allow_cam_files then
--			cfgtext = anyfs.readfile(opts.cfgfile,{con=opts.con})
--		else
--			cfgtext = fsutil.readfile(opts.cfgfile)
--		end
		cfgtext = fsutil.readfile(opts.cfgfile)
	elseif opts.cfgtext then
		cfgtext = opts.cfgtext
	end
	if cfgtext then
		t:merge_saved_cfg(cfgtext)
	end
	return t
end

--[[
script=chdkmenuscript.process_script(opts)
load a CHDK camera script, optionally using a glue template and camera settings
and optionally saving the processed script and merged settings
opts={
	-- the following are passed to chdkmenuscript.new_header
	file="filename"
	text="header content"
	cfgfile="filename" -- saved settings file to merge
	cfgtext="saved settings content" -- saved settings content as string
-- not applicable to hostlua
--	allow_cam_files=boolean -- load script and settings files from camera when
	                        -- path starts with 'A/', default true
--	con=connection -- connection for cam files, default global con
	--
	load=boolean|path -- use a loadfile line in the merged script
	tpl=string -- use template file to combine settings and script body
	tplfile=path -- use template file to combine settings and script body
	save=path -- write processed script to path
	savecfg=path -- write merged settings as CHDK script cfg to path
	menuvals=array -- array of name, value pairs setting individual script menu options
	info_fn=function -- printf-like function for status information
}
throws on error
returns merged script
]]
function m.process_script(opts)
	opts = util.extend_table({
		info_fn=util.printf,
	},opts)
	local hdr = m.new_header(util.extend_table({},opts,{keys={
		'file',
		'text',
		'cfgfile',
		'cfgtext',
		'allow_cam_files',
		'con',
	}}))
	if opts.menuvals then
		hdr:set_values(opts.menuvals)
	end
	if opts.tplfile then
		if opts.tpl then
			errlib.throw{etype='bad_arg',msg="expected only one of tpl and tplfile"}
		end
		opts.tpl = fsutil.readfile(opts.tplfile)
	end
	if opts.savecfg then
		opts.info_fn('save cfg %s\n',opts.savecfg)
		fsutil.writefile(opts.savecfg,hdr:make_saved_cfg(),{bin=true})
	end
	-- loadfile line requested, no file specified
	if opts.load == true then
		if not opts.file then
			errlib.throw{etype='bad_arg',msg="load=true without script file"}
		end
		-- hostlua doesn't support remote
		-- if file is remote, use full path
--		if anyfs.is_remote(opts.file) then
--			opts.load = opts.file
--		else -- otherwise, just filename, glue code will add script directory
--			opts.load = fsutil.basename(opts.file)
--		end
		opts.load = fsutil.basename(opts.file)
	end
	-- TODO option for rlibs
	local script = hdr:make_glue_tpl(opts.tpl,{camfile=opts.load})
	if opts.save then
		opts.info_fn('save %s\n',opts.save)
		-- script is saved in binary mode = force unix line endings for reduced size
		fsutil.writefile(opts.save,script,{bin=true})
	end
	return script
end

return m
