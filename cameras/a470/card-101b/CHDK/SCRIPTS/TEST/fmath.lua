--[[
********************************************
Licence: GPL
(C)2021 philmoz

fmath library test
    double precision floating point library test
********************************************
@title fmath functions V0.1
@chdk_version 1.6
]]

function str1E3(val)
    return val:tostr(3);
end

function printf(...)
    print(string.format(...))
end

-- constants
x = fmath.new(4)                                    -- 4.000
y = fmath.new(3)                                    -- 3.000
z = fmath.new(2)                                    -- 2.000
hyp = fmath.sqrt(x * x + y * y)                     -- 5.000

phi_deg = fmath.new(30)                             -- 30 deg
phi_rad = fmath.pi_2 / y                            -- (PI/2)/3 equal 30 deg

-----MAIN-----
set_console_layout(0,0,40,12)

print("constants:")
printf("2*PI = %s", str1E3(fmath.pi2))
printf("PI = %s", str1E3(fmath.pi))
printf("PI/2 = %s", str1E3(fmath.pi_2))
print()
print("press key for next page")
wait_click(0)
cls()
print("Arithmetic:")
printf("%s * %s / %s = %s", str1E3(hyp), str1E3(x), str1E3(y), str1E3(hyp * x / y))
printf("%s * %s = %s", str1E3(x), str1E3(y), str1E3(x * y))
printf("%s / %s = %s", str1E3(hyp), str1E3(x), str1E3(hyp / x))
printf("%s + %d = %s", str1E3(hyp), 2, str1E3(hyp + 2))
printf("%s - %d = %s", str1E3(hyp), 7, str1E3(hyp - 7))
printf("negate %s = %s", str1E3(hyp), str1E3(-hyp))
print()
print("press key for next page")
wait_click(0)
cls()
print("Comparison:")
printf("%s == %s - %s", str1E3(hyp), str1E3(x), tostring(hyp == x))
printf("%s ~= %s - %s", str1E3(hyp), str1E3(x), tostring(hyp ~= x))
printf("%s <= %s - %s", str1E3(hyp), str1E3(x), tostring(hyp <= x))
printf("%s < %s - %s", str1E3(hyp), str1E3(x), tostring(hyp < x))
printf("%s == %s - %s", str1E3(y), str1E3(y), tostring(y == y))
printf("%s <= %s - %s", str1E3(y), str1E3(x), tostring(y <= x))
printf("%s < %s - %s", str1E3(y), str1E3(x), tostring(y < x))
printf("%s >= %s - %s", str1E3(y), str1E3(x), tostring(y >= x))
printf("%s > %s - %s", str1E3(y), str1E3(x), tostring(y > x))
print()
print("press key for next page")
wait_click(0)
cls()
print("Trigonometry:")
-- sin, cos, tan
sin_rad = phi_rad:sin()
cos_rad = phi_rad:cos()
tan_rad = phi_rad:tan()
printf("sin(%s) = %s", str1E3(phi_rad), str1E3(sin_rad))
printf("cos(%s) = %s", str1E3(phi_rad), str1E3(cos_rad))
printf("tan(%s) = %s", str1E3(phi_rad), str1E3(tan_rad))
-- asinr, acosr, atanr
asin_rad = sin_rad:asin()
acos_rad = cos_rad:acos()
atan_rad = tan_rad:atan()
printf("asinr(%s) = %s", str1E3(sin_rad), str1E3(asin_rad))
printf("acosr(%s) = %s", str1E3(cos_rad), str1E3(acos_rad))
printf("atanr(%s) = %s", str1E3(tan_rad), str1E3(atan_rad))
--recr to polr
v, a = fmath.pol(x, y)
printf("recr(%s, %s) = polr(%s, %s)", str1E3(x), str1E3(y), str1E3(v), str1E3(a))
--polr to recr
_x, _y = fmath.rec(v, a)
printf("polr(%s, %s) = recr(%s, %s)", str1E3(v), str1E3(a), str1E3(_x), str1E3(_y))
print()
print("press key for next page")
wait_click(0)
cls()
print("Trigonometry 2:")
-- sin, cos, tan
phi_rad = -phi_rad
sin_rad = phi_rad:sin()
cos_rad = phi_rad:cos()
tan_rad = phi_rad:tan()
printf("sin(%s) = %s", str1E3(phi_rad), str1E3(sin_rad))
printf("cos(%s) = %s", str1E3(phi_rad), str1E3(cos_rad))
printf("tan(%s) = %s", str1E3(phi_rad), str1E3(tan_rad))
-- asinr, acosr, atanr
asin_rad = sin_rad:asin()
acos_rad = cos_rad:acos()
atan_rad = tan_rad:atan()
printf("asinr(%s) = %s", str1E3(sin_rad), str1E3(asin_rad))
printf("acosr(%s) = %s", str1E3(cos_rad), str1E3(acos_rad))
printf("atanr(%s) = %s", str1E3(tan_rad), str1E3(atan_rad))
print()
print("press key for next page")
wait_click(0)
cls()
print("Conversion:")
val = fmath.new(45,10)   -- 4.500
printf("int(%s) = %d", str1E3(val), val:int())
printf("frac(%s) = %s", str1E3(val), str1E3(val - val:int()))
printf("ceil(%s) = %d", str1E3(val), val:ceil())
printf("floor(%s) = %d", str1E3(val), val:floor())
printf("round(%s) = %d", str1E3(val), val:round())
print()
print("press key for next page")
wait_click(0)
cls()
print("Conversion (negative):")
val = fmath.new(-46,10)   -- -4.600
printf("int(%s) = %d", str1E3(val), val:int())
printf("frac(%s) = %s", str1E3(val), str1E3(val - val:int()))
printf("ceil(%s) = %d", str1E3(val), val:ceil())
printf("floor(%s) = %d", str1E3(val), val:floor())
printf("round(%s) = %d", str1E3(val), val:round())
print()
print("press key for next page")
wait_click(0)
cls()
print("Functions:")
pow = z ^ x
const_1E3 = fmath.new(1000,1)
printf("log(%s) = %s", str1E3(pow), str1E3(pow:log()))
printf("log2(%s) = %s", str1E3(pow), str1E3(pow:log2()))
printf("log10(%s) = %s", str1E3(const_1E3), str1E3(const_1E3:log10()))
printf("pow(%s, %s) = %s", str1E3(z), str1E3(x), str1E3(pow))
printf("pow(%s, 1/%s) = %s", str1E3(pow), str1E3(x), str1E3(pow ^ (1/x)))
printf("sqrt(%s) = %s", str1E3(pow), str1E3(pow:sqrt()))
printf("rad(%s) = %s", str1E3(phi_deg), str1E3(phi_deg:rad()))
printf("deg(%s) = %s", str1E3(phi_rad), str1E3(phi_rad:deg()))
print()
print("press key for end")
wait_click(0)
cls()
