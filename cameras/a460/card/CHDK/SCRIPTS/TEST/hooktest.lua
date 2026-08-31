--[[
@title shoot hook+filecounter test
@chdk_version 1.4.0
#ui_single=true "Test single"
#ui_fast=true "Test half+click full"
#ui_cont=true "Test cont mode"
#ui_shots=5 "burst shots"
#ui_append=false "append log"
#ui_raw=false "CHDK raw"
]]
--[[
this script tests the operation and placement of the remote hook (wait_until_remote_button_is_released)
raw hook (capt_seq_hook_raw_here), and override code (shooting_expo_param_override).

It also checks if the file counter (get_exp_count etc) increments by the time the raw hook is reached.

If one of the hooks isn't reached, the hook may need to be moved or additional branches may need to be hooked.

If the file counter increments late, PAUSE_FOR_FILE_COUNTER may need to be set or increased in capt_seq.c

If a hook isn't reached, the single shot test will generate errors like:
0020660: FAIL: hook_raw count 0 expect 1
The fast and cont tests will generate errors like:
0037700: FAIL: hook_raw wait timeout

NOTES:
* Shot review should be disabled in the Canon menu.
* Cameras that require a short PAUSE_FOR_FILE_COUNTER may pass the file counter tests.
* PAUSE_FOR_FILE_COUNTER is only used if CHDK raw or remote shoot with raw is enabled.
  If raw is not enabled, the script will not treat a late exposure counter increment as a failure, 
  but will generate the message "PAUSE_FOR_FILE_COUNTER required?"
* Older cameras (where get_file_next_counter is file counter +1) will fail the file counter checks in the
  fast and continuous tests
]]

require'hookutil'

props=require'propcase'
capmode=require'capmode'

raw_enable_save=get_raw()
set_raw(ui_raw)
function restore()
	set_raw(raw_enable_save)
end

local hooktest={
	-- time to wait for any hook to become ready
	hook_wait_timeout=10000,
	-- time for script to block shoot hook
	hook_shoot_timeout=5000,
	-- time for script to block raw hook
	hook_raw_timeout=5000,
	logname='A/hooktest.log',
	hook_shoot_timeout_count=0,
	hook_raw_timeout_count=0,
	failcount=0,
	failtotal=0,
	hook_shoot_check_count=0,
	hook_raw_check_count=0,
	raw_exp_count_timeout_count=0,
	raw_exp_count_max_wait=0,
	raw_exp_count_min_wait=10000,
}

function hooktest:init(opts)
	for k,v in pairs(opts) do
		self[k] = opts[k]
	end
	if not self.log_append and os.stat(self.logname) then
		os.remove(self.logname)
	end
	self:log("hooktest start: %s",os.date())
	local bi=get_buildinfo()
	self:log("buildinfo: %s %s %s %s %s %s %s %s 0x%x",
						bi.platform,bi.platsub,bi.version,bi.build_number,bi.build_revision,
						bi.build_date,bi.build_time,bi.os,bi.platformid)
	self:log("options: single=%s fast=%s cont=%s shots=%d append=%s raw=%s",
				tostring(ui_single),tostring(ui_fast),tostring(ui_cont),
				ui_shots,tostring(ui_append),tostring(ui_raw))
	if not get_mode() then
		self:logcon("switching to rec")
		set_record(true)
		repeat sleep(10) until get_mode()
		sleep(500)
	end

	self:log("capture mode: %s (%d)",capmode.get_name(),capmode.get_canon())

	if self.do_cont and get_prop(props.DRIVE_MODE) ~= 1 then
		self:logcon("Canon cont mode not set, disabling cont test")
		self.do_cont=false
	end
end

function hooktest:logline(s)
	s=string.format('%07d: %s\n',get_tick_count(),s)
	local fh,err=io.open(self.logname,'ab')
	if not fh then
		error(tostring(err))
	end
	fh:write(s)
	fh:close()
end
function hooktest:log(fmt,...)
	self:logline(string.format(fmt,...))
end

function hooktest:logcon(fmt,...)
	local s=string.format(fmt,...)
	print(s)
	self:logline(s)
end

function hooktest:log_fail(fmt,...)
	self.failcount = self.failcount+1
	self:logcon('FAIL: '..tostring(fmt),...)
end

function hooktest:run_test(name)
	self.failcount = 0
	if not self['do_'..name] then
		self:log('%s: skipped',name)
		return
	end
	self:log('%s: start',name)

	self['test_'..name](self)

	self:log('%s: end',name)

	-- give any test shots some time to finish
	repeat sleep(10) until not get_shooting()
	sleep(500)

	self.failtotal = self.failtotal + self.failcount
	if self.failcount > 0 then
		self:logcon('%s: FAIL %d',name,self.failcount)
	else
		self:logcon('%s: PASSED',name)
	end
end

function hooktest:update_exp_count()
	self.exp_count = get_exp_count()
-- expect wrap on next shot
-- TODO reset may occur in other cases
	if self.exp_count == 9999 then
		self:log('exp_count wrap')
		self.exp_count = 0
	end
end

--[[
wait for shoot hook, log if it times out, continue
]]
function hooktest:check_hook_shoot()
	self:log('exp=%04d dir=%s hook_shoot wait',get_exp_count(),get_image_dir())
	self.hook_shoot_check_count = self.hook_shoot_check_count + 1
	if not hook_shoot.wait_ready{timeout=self.hook_wait_timeout,timeout_error=false} then
		self:log_fail('hook_shoot wait timeout')
		-- hook is cleared on timeout, reset
		hook_shoot.set(self.hook_shoot_timeout)
		self.hook_shoot_timeout_count = self.hook_shoot_timeout_count + 1
		return
	end
	self:log('exp=%04d dir=%s hook_shoot ready',get_exp_count(),get_image_dir())
	hook_shoot.continue()
end

--[[
wait for raw hook, log if it times out, check file counter, if file counter not incremented, 
report and wait
]]
function hooktest:check_hook_raw()
	self:log('exp=%04d dir=%s hook_raw wait',get_exp_count(),get_image_dir())
	self.hook_raw_check_count = self.hook_raw_check_count + 1
	if not hook_raw.wait_ready{timeout=self.hook_wait_timeout,timeout_error=false} then
		self:log_fail('hook_raw wait timeout')
		-- hook is cleared on timeout, reset
		hook_raw.set(self.hook_raw_timeout)
		self.hook_raw_timeout_count = self.hook_raw_timeout_count + 1
		return
	end
	local c = get_exp_count()
	local t0 = get_tick_count()
	self:log('exp=%04d dir=%s hook_raw ready',c,get_image_dir())

	local next_ec = self.exp_count + 1 
	if c ~= next_ec then
		-- if raw is enabled, PAUSE_FOR_FILE_COUNTER should be in effect, so this is certain fail
		if get_raw() then
			self:log_fail('exp count %d expect %d',c,next_ec)
		else
			-- if not, will warn at the end
			self:log('exp count %d expect %d',c,next_ec)
		end
		-- wait up to 1 sec to see if counter increments late
		local ec_wait=0
		local ec_timeout=true
		while ec_wait < 1000 do
			sleep(10)
			ec_wait = get_tick_count() - t0
			if get_exp_count() == next_ec then
				self:log('exp count increment late: %d',ec_wait)
				ec_timeout = false
				break
			end
		end
		if ec_wait > self.raw_exp_count_max_wait then
			self.raw_exp_count_max_wait = ec_wait
		end
		if ec_wait < self.raw_exp_count_min_wait then
			self.raw_exp_count_min_wait = ec_wait
		end
		if ec_timeout then
			self.raw_exp_count_timeout_count = self.raw_exp_count_timeout_count + 1
			self:log_fail('exp count wait timeout')
		end
	else 
		self.raw_exp_count_min_wait = 0
	end
	self:update_exp_count()
	hook_raw.continue()
end

--[[
press shoot half and wait for get_shooting
fail if get_shooting never went true
check that preshoot hook was reached
]]
function hooktest:preshoot()
	local c_expect = hook_preshoot.count()+1
	self:log('exp=%04d dir=%s preshoot',get_exp_count(),get_image_dir())
	press'shoot_half'
	local timeout = get_tick_count() + 3000
	repeat
		sleep(10)
		if get_tick_count() > timeout then
			self:log_fail('preshoot get_shooting timeout')
			release'shoot_half'
			return false
		end
	until get_shooting()
	self:log('exp=%04d dir=%s preshoot ready tv=%d av=%d sv=%d',get_exp_count(),get_image_dir(),get_tv96(),get_av96(),get_sv96())

	local c = hook_preshoot.count()
	if c ~= c_expect then
		self:log_fail('hook_preshoot %d expect %d',c,c_expect)
	end
	return true
end

--[[
test a single shot with "shoot", verify each hook counter and the file counter increment
]]
function hooktest:test_single()
	local hook_names = {'hook_preshoot','hook_shoot','hook_raw'}
	local counts={}
	-- exp count can reset on first shot after boot depending on folder / file number settings
	-- do a single warmup shot without checking counter
	self:log('exp=%04d dir=%s warmup shoot start',get_exp_count(),get_image_dir())
	local r=shoot()
	if r ~= 0 then
		self:log_fail('warmup shoot failed %d',r)
	end
	self:log('exp=%04d dir=%s warmup shoot done',get_exp_count(),get_image_dir())
	sleep(500)

	for _,name in ipairs(hook_names) do
		counts[name] = _G[name].count()
	end
	self:update_exp_count()
	self:log('exp=%04d dir=%s shoot start',get_exp_count(),get_image_dir())
	local r=shoot()
	if r ~= 0 then
		self:log_fail('shoot failed %d',r)
	end
	self:log('exp=%04d dir=%s shoot done',get_exp_count(),get_image_dir())
	for _,name in ipairs(hook_names) do
		local c=_G[name].count()
		if counts[name]+1 ~= c then
			self:log_fail('%s count %d expect %d',name,c,counts[name]+1)
		end
	end

	local c = get_exp_count()
	if c ~= self.exp_count + 1 then
		self:log_fail('exp count %d expect %d',c,self.exp_count + 1)
	end
end

--[[
test a burst of shots holding half press and clicking full press
verify each hook is reached, verify file counter has incremented when raw hook is reached
]]
function hooktest:test_fast()
	self:update_exp_count()

	-- if preshoot failed completely, skip shooting test
	if not self:preshoot() then
		return
	end

	hook_shoot.set(self.hook_shoot_timeout)
	hook_raw.set(self.hook_raw_timeout)
	for i=1,self.burst_shots do
		press'shoot_full_only'
		self:check_hook_shoot()
		release'shoot_full_only'
		self:check_hook_raw()
		-- allow some time before pressing shoot_full again
		sleep(100)
	end
	release'shoot_half'
	hook_shoot.set(0)
	hook_raw.set(0)
end

--[[
test a burst of shots in continuous mode
verify each hook is reached, verify file counter has incremented when raw hook is reached
]]
function hooktest:test_cont()
	self:update_exp_count()

	-- if preshoot failed completely, skip shooting test
	if not self:preshoot() then
		return
	end
	hook_shoot.set(self.hook_shoot_timeout)
	hook_raw.set(self.hook_raw_timeout)
	press'shoot_full_only'
	for i=1,self.burst_shots do
		self:check_hook_shoot()
		-- release shoot_full and hook immediately on final shot
		-- to avoid spurious extra shots
		if i == self.burst_shots then
			hook_shoot.set(0)
			release'shoot_full'
		end
		self:check_hook_raw()
	end
	hook_raw.set(0)
end

function hooktest:results()
	if self.hook_shoot_timeout_count > 0 then
		self:log('wait_until_remote_button_is_released bad/missing %d/%d',
				self.hook_shoot_timeout_count,
				self.hook_shoot_check_count)
	end
	if self.hook_raw_timeout_count > 0 then
		self:log('capt_seq_hook_raw_here bad/missing %d/%d',
				self.hook_raw_timeout_count,
				self.hook_raw_check_count)
	end
	-- if every exp count wait failed, don't bother with pause messages
	if self.raw_exp_count_max_wait > 0 and self.raw_exp_count_timeout_count ~= self.hook_raw_check_count then
		if get_raw() then
			self:logcon('PAUSE_FOR_FILE_COUNTER short/missing?')
		else
			self:logcon('PAUSE_FOR_FILE_COUNTER required?')
		end
		self:logcon('pause min=%d max=%d',self.raw_exp_count_min_wait,self.raw_exp_count_max_wait)
	end
	if self.failtotal > 0 then
		self:logcon("FAILED %d",self.failtotal)
	else
		self:logcon("ALL PASS")
	end
end

hooktest:init{
	do_single=ui_single,
	do_fast=ui_fast,
	do_cont=ui_cont,
	burst_shots=ui_shots,
	log_append=ui_append,
}
hooktest:run_test('single')
hooktest:run_test('fast')
hooktest:run_test('cont')
hooktest:results()
restore()
