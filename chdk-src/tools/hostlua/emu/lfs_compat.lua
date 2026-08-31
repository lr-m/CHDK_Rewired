--[[
 Copyright (C) 2024 <reyalp (at) gmail dot com>

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
wrappers to make hostlua os functions behave more like lfs, for fsutil
]]
local lfs={}
lfs._stat2attr = {
	size='size',
	mtime='modification',
	ctime='change',
}
function lfs.attributes(path,key)
	local st, err, errno = os.stat(path)
	if not st then return
		nil, err, errno
	end
	local r
	if type(key) == 'table' then
		r = key
		key = nil
	else
		r = {}
	end
	for sk,ak in pairs(lfs._stat2attr) do
		r[ak] = st[sk]
	end
	if st.is_file then
		r.mode = 'file'
	elseif st.is_dir then
		r.mode = 'directory'
	else
		r.mode = 'other'
	end
	-- note unsupported values will be nil, lfs errors on unknown keys
	if key then
		return r[key]
	end
	return r
end
function lfs.mkdir(path)
	return os.mkdir(path)
end

return lfs
