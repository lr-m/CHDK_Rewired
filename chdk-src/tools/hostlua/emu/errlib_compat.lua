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
minimal errlib.throw, normally provided by chdkptp core lerrlib.c
]]
local errlib={}
errlib.err_mt={
	__tostring=function(t)
		if type(t) ~= 'table' then
			return 'expected table'
		end
		if t.msg ~= nil then
			return t.msg
		end
		if t.etype ~= nil then
			return t.etype
		end
		return 'unknown error'
	end
}
function errlib.throw(err)
	if type(err) ~= 'table' then
		err = {}
	end
	setmetatable(err,errlib.err_mt)
	error(err,2)
end
return errlib
