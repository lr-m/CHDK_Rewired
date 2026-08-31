--[[
@title Default Script
@chdk_version 1.3
when run, attempts to reset the CHDK language/codepage to match the camera selection, or build default
]]

osdcfg=require("gen/cnf_osd") 

lang_names={
	'ENGLISH',      -- 1
	'GERMAN',       -- 2
	'FRENCH',       -- 3
	'DUTCH',        -- 4
	'ITALIAN',      -- 5
	'UKRAINIAN',    -- 6
	'INDONESIAN',   -- 7
	'FINNISH',      -- 8
	'SPANISH',      -- 9
	'RUSSIAN',      -- 10
	'PORTUGUESE',   -- 11
	'GREEK',        -- 12
	'POLISH',       -- 13
	'CZECH',        -- 14
	'HUNGARIAN',    -- 15
	'TURKISH',      -- 16
	'ROMANIAN',     -- 17
	'CROATIAN',     -- 18
	'ESTONIAN',     -- 19
	'SLOVAK',       -- 20
}

bi=get_buildinfo()
if bi.version=="CHDK" then
	chdk_def_lang='ENGLISH'
else
	chdk_def_lang='GERMAN'
end

-- supported languages, name must match CHDK language file name
langs={
	ENGLISH		={font_cp=2,hint="CHDK language changed to english"},
	GERMAN		={font_cp=2,hint="CHDK-Sprache auf deutsch geдndert"},
	FRENCH		={font_cp=2,hint="CHDK language changed to french"},
	DUTCH		={font_cp=2,hint="CHDK language changed to dutch"},
	ITALIAN		={font_cp=2,hint="CHDK language changed to italian"},
	UKRAINIAN	={font_cp=1,hint="Мову CHDK змінено на україньску"},
	INDONESIAN	={font_cp=2,hint="CHDK language changed to indonesian"},
	FINNISH		={font_cp=2,hint="CHDK language changed to finnish"},
	SPANISH		={font_cp=2,hint="CHDK language changed to spanish"},
	RUSSIAN		={font_cp=1,hint="Язык CHDK изменен на русский"},
	PORTUGUESE	={font_cp=2,hint="CHDK language changed to portuguese"},
	GREEK		={font_cp=3,hint="CHDK language changed to greek"},
	POLISH		={font_cp=0,hint="CHDK language changed to polish"},
	CZECH		={font_cp=0,hint="CHDK language changed to czech"},
	HUNGARIAN	={font_cp=0,hint="CHDK language changed to hungarian"},
	TURKISH		={font_cp=4,hint="CHDK language changed to turkish"},
	ROMANIAN	={font_cp=0,hint="CHDK language changed to romanian"},
	-- following 3 not seen in Canon firmware
	CROATIAN	={font_cp=0,hint="CHDK language changed to croatian"},
	-- cp 5 = win1257 unlike others doesn't match list digit of windows CP
	ESTONIAN	={font_cp=5,hint="CHDK language changed to estonian"},
	SLOVAK		={font_cp=0,hint="CHDK language changed to slovak"},
}

-- mapping from LANGUAGE property value to language name index
-- many propsets share the same mapping
-- in each mapping :- array index = LANGUAGE property value, array value = index of language name in lang_names array
prop_maps={
	 [1]={[0]=1,[1]=2, [2]=3,[3]=4, [5]=8,[6]=5,[9]=9,[11]=10,[12]=11,[13]=12,[14]=13,[15]=14,[16]=15,[17]=16,},                        -- propset 1
	 [2]={[0]=1,[1]=2, [2]=3,[3]=4, [5]=8,[6]=5,[8]=6,[10]=9, [12]=10,[13]=11,[14]=12,[15]=13,[16]=14,[17]=15,[18]=16,[23]=17},         -- propset 2, 3 & 4
	 [5]={[0]=1,[1]=2, [2]=3,[3]=4, [5]=5,[6]=6,[8]=7,[10]=8, [13]=9, [15]=10,[16]=11,[17]=12,[18]=13,[19]=14,[20]=15,[21]=16,[26]=17}, -- propset 5 & 6
	 [7]={[0]=1,[2]=17,[3]=2,[5]=16,[6]=3,[7]=9,[9]=4,[10]=12,[13]=10,[15]=11,[16]=13,[18]=8, [19]=14,[21]=5, [22]=15,[24]=6, [26]=7},  -- propset 7 - 13
}

-- propcase id and prop_map index for each propset
prop_langs={
	 [1]={prop=196,map=1},
	 [2]={prop=61, map=2},
	-- 3-4 assumed to be the same as 2, IDs appear the same based on surrounding, values not tested
	 [3]={prop=61, map=2},
	 [4]={prop=61, map=2},
	-- 5, 6 identical, values changed from 2 (verified sx160, elph130)
	 [5]={prop=61, map=5},
	 [6]={prop=61, map=5},
	-- 7 id changed, values go by row rather than col (verified g7x)
	 [7]={prop=66, map=7},
	-- 8 is chronologically between 6 and 7, same order as 7
	 [8]={prop=63, map=7},
	 [9]={prop=66, map=7},
	-- 10 id changed, values not changed from 7
	[10]={prop=68, map=7},
	-- 11 based on ixus190 report (added later than 12)
	[11]={prop=69, map=7},
	-- 12 id changed from 10, values not changed
	[12]={prop=69, map=7},
	-- 13 same as 7
	[13]={prop=68, map=7},
}

-- return camera language name, or nil
function get_cam_lang()
	local ps = get_propset()
	local p=prop_langs[ps]
	if not p then
		print('unsupported propset '..ps)
		return
	end
	-- one propset11 cam appeared to set upper bit of language byte
	-- https://chdk.setepontos.com/index.php?topic=13146.msg146335#msg146335
	local pv=bitand(get_prop(p.prop)/256,0x7f)
	local lang=lang_names[prop_maps[p.map][pv]]
	if not lang then
		print('unknown language '..ps .. ':'..pv)
	end
	return lang
end

-- return chdk language name, or nil
function get_chdk_lang()
	local lf=get_config_value(osdcfg.lang_file)
	if lf=="" then
		return chdk_def_lang
	else
		lf=string.upper(lf)
		for k,v in pairs(langs) do
			if string.find(lf, k..".LNG")~=nil then
				return k
			end
			if string.len(k) > 8 then
				-- check for most likely 8.3 name
				local k2=string.sub(k,1,6)..'~1.LNG'
				if string.find(lf, k2)~=nil then
					return k
				end
			end
		end
	end
end

function set_chdk_lang_default()
	set_config_value(osdcfg.lang_file,"")
	set_config_value(osdcfg.font_cp,langs[chdk_def_lang].font_cp)
	print(langs[chdk_def_lang].hint)
end
chdk_lang=get_chdk_lang()
cam_lang=get_cam_lang()

-- if CHDK language and camera language both unknown, reset CHDK to default
-- matches behavior of original script, unclear if desirable
if not cam_lang and not chdk_lang then
	set_chdk_lang_default()
elseif cam_lang~=chdk_lang then
	if not chdk_lang or cam_lang==chdk_def_lang then
		set_chdk_lang_default()
	elseif langs[cam_lang]~=nil then
		-- note assumes cam can handle long name (generally true)
		if os.stat("A/CHDK/LANG/"..cam_lang..".LNG") then
			set_config_value(osdcfg.lang_file,"A/CHDK/LANG/"..cam_lang..".LNG")
			set_config_value(osdcfg.font_cp,langs[cam_lang].font_cp)
			print(langs[cam_lang].hint)
		else
			print(cam_lang..".LNG is missing")
		end
	end
elseif cam_lang then
	print(";)")
end
