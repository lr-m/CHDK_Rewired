--[[
@title restore test
@chdk_version 1.5.1
#test_mode=1 "Mode" {loop wait yield error} table
#loop_count=1000 "Loop iterations" long
#wait_time=1000 "Wait time (ms)"
--]]
--[[
This script tests the "restore" function, triggered when a script
is interrupted with the shoot button
loop: Loops for the specified number of iterations
wait: Loops for the specified number of milliseconds, without yielding
yield: Calls sleep, which is not allowed in restore
error: Calls error('TEST') in the restore function

The script waits for a key. To test restore functionality, full press the shutter
Pressing set triggers a runtime error in normal code. Restore should not be executed
Pressing menu exits the script normally. Restore should not be executed
--]]

tests={
	loop=function()
		print('loop',loop_count)
		local j
		for i=1,loop_count do
			j=i
		end
		print('loop done',j)
	end,
	wait=function()
		print('wait',wait_time,'ms')
		local i = 0
		local t0=get_tick_count()
		while get_tick_count() - t0 < wait_time do
			i = i + 1
		end
		print('wait done',get_tick_count()-t0,i)
	end,
	yield=function()
		print('yielding')
		local t0=get_tick_count()
		sleep(100)
		print('yield done',get_tick_count()-t0)
	end,
	error=function()
		print('error TEST')
		error('TEST')
		print('failed!')
	end,
}
restore = tests[test_mode.value]
print('shoot=test restore',test_mode.value)
print('set=error, menu=end')
while true do
	wait_click()
	if is_key 'set' then
		error 'runtime error'
	elseif is_key('menu') then
		print 'normal exit'
		break
	end
end
