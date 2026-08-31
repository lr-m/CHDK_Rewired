--[[
@title video test
@chdk_version 1.4
#menu_test_mode=0 "video start" {auto shoot vid_btn skip}
#mode_change_delay=250 "mode change delay, ms"
#mode_test=true "modes test"
--]]

-- log to table, because file IO will fail when recording video
log = {}

function write_log()
	local logname="A/vidtest.log"
	local logfh
	
	local logtry=0
	-- may not be able to open log right a away, retry for 2 sec
	repeat
		log_printf("log open try %d ",logtry)
		logfh=io.open(logname,"wb")
		if logfh then
			log_printf("ok\n")
			break
		end
		log_printf("fail\n")
		logtry = logtry + 1
		sleep(100)
	until logtry == 20

	if not logfh then
		error("open log failed")
	end

	for i,v in ipairs(log) do
		logfh:write(v)
	end
	logfh:close()
end


capmode=require("capmode")

function printf(...)
	print(string.format(...))
end

function log_printf(...)
	table.insert(log,string.format(...))
end

fail_count=0

function log_fail(...)
	fail_count = fail_count + 1
	log_printf(...)
end


function check_mode(id)
	if not capmode.valid(id) then
		return
	end
	local status=capmode.set(id)
	sleep(mode_change_delay)

	log_printf("set %20s: ",capmode.mode_to_name[id])
	printf("%s",capmode.mode_to_name[id])

	if status then
		if capmode.get() ~= id then
			log_fail("CHANGE FAIL req %3d got %3d\n",id,capmode.get())
			return
		end
	else
		log_fail("SET %3d FAIL\n",id)
		return
	end

	local isrec,isvid=get_mode()
	local video_rec=get_video_recording()
	if video_rec then
		log_fail("ERROR: video_recording set for mode %d\n", id)
	end
	log_printf("is_video=%s recording=%s move_status=%d\n",
				tostring(isvid),tostring(video_rec),get_movie_status())
end

function check_record()
	local test_mode = menu_test_mode
	local button

	local fail_count_save = fail_count
	printf("rec test");
	-- auto - used shoot + VIDEO_STD if valid, otherwise video button
	if test_mode == 0 then
		-- some cameras with video modes may still require video button to start recording
		if capmode.valid('VIDEO_STD') and get_video_button() == 0 then
			test_mode = 1
		else
			test_mode = 2
		end
	elseif test_mode == 3 then
		log_printf("skipping video record test\n")
	end
	log_printf("record test init: recording=%s movie_status=%d\n",tostring(get_video_recording()),get_movie_status())
	if test_mode == 1 then
		button='shoot_full'
		if not capmode.valid('VIDEO_STD') then
			log_fail("ERROR: shoot requested, VIDEO_STD not present\n")
			return
		end
		if not capmode.set('VIDEO_STD') then
			log_fail("ERROR: set VIDEO_STD failed\n")
			return
		end
		sleep(mode_change_delay)
	else
		button='video'
		-- if no video button reported, warn but continue because CAM_HAS_VIDEO_BUTTON may be wrong
		if get_video_button() == 0 then
			log_fail("WARNING: video button requested, get_video_button = 0\n")
		end
		-- set cap mode to P in case mode test left us in some weird scene mode
		if not capmode.set('P') then
			log_fail("ERROR: set P failed\n")
			return
		end
		sleep(mode_change_delay)
	end
	log_printf("using %s for video start\n",button)
	-- press button and wait 2 sec for recording to start
	press(button)
	local count=0
	while count < 20 and not get_video_recording() do
		count = count + 1
		sleep(100)
	end
	if not get_video_recording() then
		log_fail("WARNING: video record start timeout\n")
	end
	log_printf("record start: recording=%s movie_status=%d\n",tostring(get_video_recording()),get_movie_status())
	release(button)
	-- record 2 sec of video to allow movie_status to settle
	sleep(2000)
	log_printf("record: recording=%s movie_status=%d\n",tostring(get_video_recording()),get_movie_status())
	-- click button 
	click(button)
	count=0
	-- wait up to 5 sec for video to finish
	while count < 50 and get_video_recording() do
		count = count + 1
		sleep(100)
	end
	if get_video_recording() then
		log_fail("WARNING: video record stop timeout\n")
	end
	log_printf("record finish: recording=%s movie_status=%d\n",tostring(get_video_recording()),get_movie_status())
	if fail_count_save == fail_count then
		printf("rec test OK");
	else
		printf("rec test FAIL");
	end
end

function run_test()
	local bi=get_buildinfo()
	log_printf("%s %s %s %s %s %s %s %s 0x%x\n",
				bi.platform,bi.platsub,bi.version,bi.build_number,bi.build_revision,
				bi.build_date,bi.build_time,bi.os,bi.platformid)
	
	if not get_mode() then
		printf("switching to rec")
		set_record(true)
		repeat sleep(10) until get_mode()
		sleep(500)
	end
	if mode_test then
		printf("mode test");
		for id,_ in ipairs(capmode.mode_to_name) do
			check_mode(id)
		end
		if fail_count == 0 then
			printf("mode test OK");
		else
			printf("mode test FAIL");
		end
	else
		log_printf("skipping mode test\n")
	end
	check_record()
	if fail_count == 0 then
		print("PASS")
	else
		printf("%d FAILED check log",fail_count)
	end
	write_log()
end

run_test()
