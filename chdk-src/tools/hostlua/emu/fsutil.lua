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
--[[
path and filesystem related utilities
funtions ending in _cam deal with camera side paths
depends on util, sys.ostype, errlib and lfs
]]
local lfs=require'lfs_compat'
local errlib=require'errlib_compat'
local util=require'util'
local fsutil={}
--[[
select functions from chdkptp fsutil.lua
]]
local fsutil={}
-- normally chdkptp core provides sys.ostype
-- guess at startup based on package.path (could also use os.getenv)
-- the default lua path includes '!' on windows but not on *nix
if package.path:find('!') or package.path:find('\\') then
	fsutil._ostype = 'Windows'
else
	-- chdkptp distinguishes between Linux and Mac but not possible (and not important) here
	fsutil._ostype = 'Linux'
end

function fsutil.ostype()
	return fsutil._ostype
end

function fsutil.dir_sep_chars()
	if fsutil.ostype() == 'Windows' then
		return '\\/'
	end
	return '/'
end

function fsutil.is_dir_sep(c)
	if c == '/' then
		return true
	end
	if fsutil.ostype() == 'Windows' then
		return c == '\\'
	end
	return false
end

-- switch all slashes to / on windows
function fsutil.normalize_dir_sep(path)
	if fsutil.ostype() == 'Windows' then
		path=string.gsub(path, "\\", "/")
	end
	return path
end

-- is path a bare name without drive / directory parts?
function fsutil.is_basename(path)
	return (fsutil.basename(path) == path)
end

--[[
remove suffix from a path if found
opts {
	ignorecase=bool -- is suffix case sensitive? default true
}
]]
function fsutil.remove_sfx(path,sfx,opts)
	opts = util.extend_table({ignorecase=true},opts)
	if string.len(sfx) <= string.len(path) then
		if (opts.ignorecase and string.lower(string.sub(path,-string.len(sfx))) == string.lower(sfx))
				or string.sub(path,-string.len(sfx)) == sfx then
			return string.sub(path,1,string.len(path) - string.len(sfx))
		end
	end
	return path
end
--[[
similar to unix basename
]]
function fsutil.basename(path,sfx,opts)
	if not path then
		return nil
	end
	local drive
	-- drive is discarded, like leading /
	drive,path = fsutil.splitdrive(path)
	local s,e,bn=string.find(path,'([^'..fsutil.dir_sep_chars()..']+)['..fsutil.dir_sep_chars()..']?$')
	if not s then
		return nil
	end
	-- unix basename returns full name if suffix is equal to original
	if sfx and string.len(sfx) < string.len(bn) then
		bn = fsutil.remove_sfx(bn,sfx,opts)
	end
	return bn
end

function fsutil.splitdrive(path)
	if fsutil.ostype() ~= 'Windows' then
		return '',path
	end
	local s,e,drive,rest=string.find(path,'^(%a:)(.*)')
	if not drive then
		drive = ''
		rest = path
	end
	if not rest then
		rest = ''
	end
	return drive,rest
end
--[[
note A/=>nil
]]
function fsutil.basename_cam(path,sfx,opts)
	if not path then
		return nil
	end
	if path == 'A/' then
		return nil
	end
	local s,e,bn=string.find(path,'([^/]+)/?$')
	if not s then
		return nil
	end
	-- unix basename returns full name if suffix is equal to original
	if sfx and string.len(sfx) < string.len(bn) then
		bn = fsutil.remove_sfx(bn,sfx,opts)
	end
	return bn
end

--[[
similar to unix dirname, with some workarounds to make it more useful on windows
UNC paths are not supported
]]
function fsutil.dirname(path)
	if not path then
		return nil
	end
	local drive=''
	-- windows - save the drive, if present, and perform dirname on the rest of the path
	drive,path=fsutil.splitdrive(path)
	-- remove trailing blah/?
	local dn=string.gsub(path,'[^'..fsutil.dir_sep_chars()..']+['..fsutil.dir_sep_chars()..']*$','')
	if dn == '' then
		if drive == '' then
			return '.'
		else
			return drive
		end
	end
	-- remove any trailing /
	dn = string.gsub(dn,'['..fsutil.dir_sep_chars()..']*$','')
	-- all /
	if dn == '' then
		return drive..'/'
	end
	return drive..dn
end

--[[
dirname variant for camera paths
note, A/ is ambiguous if used on relative paths, treated specially
has trailing directory removed, except for A/ (camera functions require trailing / on A/ and reject on subdirs)
A/ must be uppercase (as required by dryos)
]]
function fsutil.dirname_cam(path)
	if not path then
		return nil
	end
	if path == 'A/' then
		return path
	end
	-- remove trailing blah/?
	local dn=string.gsub(path,'[^/]+/*$','')
	-- invalid,
	if dn == '' then
		return nil
	end
	-- remove any trailing /
	dn = string.gsub(dn,'/*$','')
	if dn == 'A' then
		return 'A/'
	end
	-- all /, invalid
	if dn == '' then
		return nil
	end
	return dn
end

--[[
add / between components, only if needed.
accepts / or \ as a separator on windows
TODO joinpath('c:','foo') becomes c:/foo
]]
function fsutil.joinpath(...)
	local parts={...}
	-- TODO might be more useful to handle empty/missing parts
	if #parts < 2 then
		error('joinpath requires at least 2 parts',2)
	end
	local r=parts[1]
	for i = 2, #parts do
		local v = string.gsub(parts[i],'^['..fsutil.dir_sep_chars()..']','')
		if not string.match(r,'['..fsutil.dir_sep_chars()..']$') then
			r=r..'/'
		end
		r=r..v
	end
	return r
end

function fsutil.joinpath_cam(...)
	local parts={...}
	-- TODO might be more useful to handle empty/missing parts
	if #parts < 2 then
		error('joinpath requires at least 2 parts',2)
	end
	local r=parts[1]
	for i = 2, #parts do
		local v = string.gsub(parts[i],'^/','')
		if not string.match(r,'/$') then
			r=r..'/'
		end
		r=r..v
	end
	return r
end

--[[
ensure path starts with A/, replace \ with /
]]
function fsutil.make_camera_path(path)
	if not path then
		return 'A/'
	end
	-- fix slashes
	path = string.gsub(path,'\\','/')
	local pfx = string.sub(path,1,2)
	if pfx == 'A/' then
		return path
	end
	if pfx == 'a/' then
		return 'A' .. string.sub(path,2,-1)
	end
	return 'A/' .. path
end

--[[
make multiple subdirectories, if they don't exist
throws on error
]]
function fsutil.mkdir_m(path)
	local mode, err, errno = lfs.attributes(path,'mode')
	if mode == 'directory' then
		return
	elseif mode then
		errlib.throw{etype='exists', msg='path exists, not directory'}
-- hostlua doesn't know errno values, assume any error is not found, mkdir should error if not
--	elseif errno ~= errno_vals.ENOENT then
--		errlib.throw{etype='lfs', errno=errno, msg=tostring(err)}
	end
	local parts = fsutil.splitpath(path)
	-- never try to create the initial . or /
	local p=parts[1]
	for i=2, #parts do
		p = fsutil.joinpath(p,parts[i])
		local mode, merr, merrno = lfs.attributes(p,'mode')
		if not mode then
-- hostlua doesn't know errno values, assume any error is not found, mkdir should error if not
--			if errno ~= errno_vals.ENOENT then
--				errlib.throw{etype='lfs', errno=merrno, msg=tostring(merr)}
--			end
			local status,err, errno = lfs.mkdir(p)
			if not status then
				errlib.throw{etype='lfs', errno=errno, msg=tostring(err)}
			end
		elseif mode ~= 'directory' then
			errlib.throw{etype='exists', msg='path exists, not directory'}
		end
	end
end

--[[
make a directory containing the target path
]]
function fsutil.mkdir_parent(path)
	fsutil.mkdir_m(fsutil.dirname(path))
end

--[[
wrapper for io.open that throws on error
]]
function fsutil.open_e(path,mode)
	local fh, err, errno=io.open(path,mode)
	if not fh then
		errlib.throw{etype='io', errno=errno, msg=tostring(err)}
	end
	return fh
end

--[[
read the entire contents of file, throw on error
opts: {
	bin=boolean - use binary mode where applicable
--	not supported in hostlua
--	missing_ok=boolean - return nil if file doesn't exist
	                     note other error conditions still throw
}
]]
function fsutil.readfile(path,opts)
	local mode
	opts = opts or {}
	if opts.bin then
		mode = 'rb'
	else
		mode = 'r'
	end
	local fh,err,errno=io.open(path,mode)
	if not fh then
-- hostlua doesn't know errno values
--		if opts.missing_ok and errno == errno_vals.ENOENT then
--			return nil
--		end
		errlib.throw{etype='io',errno=errno, msg=tostring(err)}
	end
	local r,err,errno=fh:read('*a')
	fh:close()
	if not r then
		errlib.throw{etype='io',errno=errno, msg=tostring(err)}
	end
	return r
end

--[[
write string or number to file, throw on error
numbers are converted to text with tostring()
opts: {
	bin=boolean - write file in binary mode. Default false (text mode)
	append=boolean - open file for append. Default false
	mkdir=boolean - create parent directories as needed, default true
}
]]
function fsutil.writefile(path,val,opts)
	opts = util.extend_table({
		mkdir = true,
	},opts)
	local mode
	if opts.append then
		mode = 'a'
	else
		mode = 'w'
	end
	if opts.bin then
		mode = mode .. 'b'
	end
	if opts.mkdir then
		fsutil.mkdir_parent(path)
	end
	if type(val) == 'number' then
		val = tostring(val)
	elseif type(val) ~= 'string' then
		errlib.throw{etype='bad_arg',msg='expected string not '..type(val)}
	end

	local fh=fsutil.open_e(path,mode)
	local status = true
	local err, errno
	if val:len() > 0 then
		status,err,errno=fh:write(val)
	end
	fh:close()
	if not status then
		errlib.throw{etype='io',errno=errno, msg=tostring(err)}
	end
end

return fsutil
