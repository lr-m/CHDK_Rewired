--[[
@title parameter test
@chdk_version 1.4
#test_number=250 "numbers" [-500 500]
#test_long=1000000 "big numbers" long
#test_value_id=0 "value id" {val_1 val_2 val_3}
#test_bool=1 "bool" bool
#test_table=1 "table" {label1 label2 label3} table
]]

set_console_layout(0,0,25,10)

print("number:", test_number)
print("big number:", test_long)
print("value id:", test_value_id)
print("bool:", test_bool)
print("table:", test_table.index, test_table.value, test_table[test_table.index])

