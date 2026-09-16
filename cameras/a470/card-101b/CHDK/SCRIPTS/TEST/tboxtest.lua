--[[
@title textbox test
@chdk_version 1.4
#len=20 "Max length" [1 100]
]]

-- Test user input using text box
-- parameters are:
--      text box title
--      prompt
--      initial value of string (user can edit this)
--      maximum length allowed for input
f = textbox("Text Box Title", "Enter some text", "", len)

print(f)
