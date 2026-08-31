--[[
@title Shot Histo Test
@chdk_version 1.3

@param a time delay (sec)
@default a 2
--]]

shot_histo_enable(1)

sleep(a*1000)

press("shoot_half")
repeat sleep(50) until get_shooting() == true

press("shoot_full")
sleep(500)
release("shoot_full")

repeat sleep(50) until get_shooting() == false	
release("shoot_half")

shot_histo_write_to_file()

p = get_histo_range(0,255)
print(p)
p = get_histo_range(256,511)
print(p)
p = get_histo_range(512,767)
print(p)
p = get_histo_range(768,1023)
print(p)

shot_histo_enable(0)
