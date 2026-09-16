--[[
This module loads a table which maps camera property case names to 
their respective numbers. It also takes care to load the correct property 
case set for the camera.

Usage: 
  proptable=require "propcase"
  Tv=get_prop(proptable.TV)
--]]

p = get_propset()
if (p >= 1) and (p <= 13) then
  return require("gen/propset"..p)
else
  error('Unsupported propset ' .. p, 1)
end
