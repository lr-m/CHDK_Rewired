--[[
@title key click test
@chdk_version 1.7.0
#passthrough=false "Passthrough"
-- TODO g5x has both, but CHDK only exposes one
#secdials=1 "Secondary dial" {None Ring Front} table
]]
--[[
test wait_click / is_key with known defined keys
if passthrough is set, the script attempts to send the corresponding
action to the Canon firmware, using click for keys, wheel_* for jogdial
and levents for secondary dials.
]]
keys={
"up",
"down",
"left",
"right",
"set",
"shoot_half",
"shoot_full",
"shoot_full_only",
"zoom_in",
"zoom_out",
"menu",
"display",
"print",
"erase",
"iso",
"flash",
"mf",
"macro",
"video",
"timer",
"expo_corr",
"fe",
"face",
"zoom_assist",
"ae_lock",
"metering_mode",
"playback",
"help",
"mode",
"wifi",
"framing_assist",
"auto",
"jogdial_left",
"jogdial_right",
"frontdial_left",
"frontdial_right",
"remote",
"no_key",
}

bad_keys={}
-- check is_key, without throwing an error if the key name is unknown
function is_key_safe(k)
	local status, r = pcall(is_key,k)
	if not status then
		if not bad_keys[k] then
			print('bad key',k,r)
			bad_keys[k] = true
		end
		return false
	end
	return r
end

-- change exit key to allow ending script without pressing half_shoot
-- which can switch to rec in passthrough mode
set_exit_key'set'

while true do
	wait_click(1000)
	local found
	for i,k in ipairs(keys) do
		if is_key_safe(k) then
			if k ~= 'no_key' then
				if passthrough then
					if k == 'jogdial_left' then
						wheel_left()
					elseif k == 'jogdial_right' then
						wheel_right()
-- currently don't have function for front dial
-- can use levents, but differ between cams because CHDK frontdial
-- is "Control Ring" on some and "Front Dial" on others
-- unclear if EDial is correct for front
					elseif k == 'frontdial_left' then
						if secdials.value == 'Ring' then
							post_levent_to_ui('RotateRingFuncLeft',2)
						elseif secdials.value == 'Front' then
							post_levent_to_ui('RotateEDialLeft',2)
						else
							print('ignored frontdial_left')
						end
					elseif k == 'frontdial_right' then
						if secdials.value == 'Ring' then
							post_levent_to_ui('RotateRingFuncRight',2)
						elseif secdials.value == 'Front' then
							post_levent_to_ui('RotateEDialRight',2)
						else
							print('ignored frontdial_right')
						end
					else
						click(k)
					end
				end
				print('key',k)
			elseif not nk then
				print('no_key')
				nk=true
			end
			found=true
			break
		end
	end
	if not found then
		print('unk key')
	end
end
