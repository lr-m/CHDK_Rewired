--[[
@title Dump levent list to A/LEVENT.CSV
#extra_test=true "Consistency check"
@chdk_version 1.3
--]]

csv=io.open("A/levent.csv","wb")
csv:write("index, id hex, id dec, name, param, warn\n")
i = 0
done = false
warncount = 0
while not done do
	name,id,param=get_levent_def_by_index(i)
	if name then
		if name == "" then
			name = "(none)"
		end
		warnings=""
		if extra_test then
			if get_levent_index(id) ~= i then
				warnings=warnings.."id mismatch "..tostring(get_levent_index(id)).." "
				warncount=warncount+1
			end
			if name ~= "(none)" and get_levent_index(name) ~= i then
				warnings=warnings.."name mismatch "..tostring(find_levent_by_name(name))
				warncount=warncount+1
			end
		end
		csv:write(string.format("%5d, 0x%04x, %5d, %s, %d, %s\n",i,id,id,name,param,warnings))
		i = i + 1
	else
		done = true
	end
end
csv:close()
print(string.format("A/LEVENT.CSV %d events",i))
if warncount > 0 then
	print(string.format("%d warnings",warncount))
end
