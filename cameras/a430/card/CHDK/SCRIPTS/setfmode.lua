--[[
@title Set focus mode
@chdk_version 1.4.1
Set focus mode
#ui_sd_mode_t=1 "Focus mode" {Ovr MF AFL AF} table
#ui_sd=0 "Focus dist mm (0=don't)" longint

This script sets focus mode, for cameras that are unable to use CHDK SD override
in Autofocus mode. If focus mode is set to "Ovr", the script will pick
a mode that allows SD overide. Other options set manual focus, AF lock and Autofocus
In MF and AF Lock mode, focus distance can be set using the focus dist option.
]]

-- focus module
focus={
	mode_names={'AF','AFL','MF'},
	valid_modes={}, -- table of name=true
	modes={}, -- array of usable mode names
}
-- initialize valid modes for sd over
function focus:init()
	if self.inited then
		return
	end
	-- bits: 1 = AF, 2 = AFL, 3 = MF
	local modes=0
	if type(get_sd_over_modes) == 'function' then
		modes=get_sd_over_modes()
	end
	self.modes={}
	for i=1,3 do
		if bitand(1,modes) == 1 then
			table.insert(self.modes,self.mode_names[i])
		end
		modes = bitshru(modes,1)
	end
	for i,m in ipairs(self.modes) do
		self.valid_modes[m]=true
	end
	self.inited=true
end
-- get current AF/AFL/MF state
function focus:get_mode()
	if get_prop(require'propcase'.AF_LOCK) == 1 then
		return 'AFL'
	end
	if get_prop(require'propcase'.FOCUS_MODE) == 1 then
		return 'MF'
	end
	return 'AF'
end
--[[
set AF/AFL/MF state
mode is one of 'AF','MF', 'AFL'
unless force is true, does not call set functions if already in desired state
]]
function focus:set_mode(mode,force)
	local cur_mode = self:get_mode()
	if not force and cur_mode == mode then
		return
	end
	if mode == 'AF' then
		if cur_mode == 'MF' then
			set_mf(false)
		elseif cur_mode == 'AFL' then
			set_aflock(false)
		end
	elseif mode == 'AFL' then
		if cur_mode == 'MF' then
			set_mf(false)
		end
		set_aflock(true)
	elseif mode == 'MF' then
		if cur_mode == 'AFL' then
			set_aflock(false)
		end
		set_mf(true)
	end
end
--[[
set to a mode that allows override, defaulting to prefmode, or the current mode if not set
]]
function focus:enable_override(prefmode)
	self:init()
	-- override not supported or in playback
	if #self.modes == 0 or not get_mode() then
		return false
	end
	-- no pref, default to overriding in current mode if possible
	if not prefmode then
		prefmode=self:get_mode()
	end
	local usemode
	if self.valid_modes[prefmode] then
		usemode = prefmode
	else
		-- if pref is MF or AFL, prefer locked if available
		if prefmode == 'MF' and self.valid_modes['AFL'] then
			usemode = 'AFL'
		elseif prefmode == 'AFL' and self.valid_modes['MF'] then
			usemode = 'MF'
		elseif self.valid_modes[self:get_mode()] then
			usemode = self:get_mode()
		else
 			-- no pref, use first available
			usemode = self.modes[1]
		end
	end
	self:set_mode(usemode)
	return true
end
function focus:set(dist)
	set_focus(dist)
end
-- end focus module

function main()
	if not get_mode() then
		print('camera must be in rec')
		return
	end
	focus:init()
	local mode = ui_sd_mode_t.value
	if mode == 'Ovr' then
		if not focus:enable_override('MF') then
			print('no usable mode')
			return
		end
	else
		focus:set_mode(mode)
	end
	local newmode = focus:get_mode()
	print('set focus mode',newmode)
	if ui_sd ~= 0 then
		if newmode == 'AF' then
			print('not setting SD in AF')
		else
			-- set config value to update override UI
			set_config_value(require'gen/cnf_core'.subj_dist_override_value,ui_sd)
			-- set actual dist
			focus:set(ui_sd)
			print('sd',ui_sd,'set',get_focus())
		end
	end
end

main()
