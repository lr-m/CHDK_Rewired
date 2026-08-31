@title ubasic abort test
@chdk_version 1.5.1

print "set=error, menu=end"
do
	wait_click
    if is_key "set" then this_is_an_error
until is_key "menu"
goto "done"

:restore
print "restore start"
i = 0
while i < 100
	sleep 10
	i = i + 1
wend
print "restore done"

:done
end
