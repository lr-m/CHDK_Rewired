--[[
@title Intervalometer
@chdk_version 1.3
@param a = interval (sec)
@default a 15
--]]
repeat
    start = get_tick_count()
	shoot()
    sleep(a*1000 - (get_tick_count() - start))
until ( false )


