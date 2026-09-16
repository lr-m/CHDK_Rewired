--[[
@title MF Test Suite v3.2
@chdk_version 1.3

Instructions :
  1) Ensure camera is in P mode (not AUTO).
  2) Camera MF & AFL modes should not be enabled.
  3) Continuous AF and Servo AF modes should be disabled.

@param     a set_focus() test?
  @default a 1
  @range   a 0 1
@param     b set_aflock() test?
  @default b 1
  @range   b 0 1
@param     c set_mf() test?
  @default c 1
  @range   c 0 1
@param     s Shoot Full
  @default s 0
  @values  s No Yes
@param     x Bypass Interlocks?
  @default x 1
  @values  x No Yes

--]]
    props=require("propcase")
    capmode=require("capmode")
    cfg1=require("gen/cnf_osd")
    cfg2=require("gen/cnf_core")
    set_console_layout(1 ,1, 45, 14 )

F1A , F1B = 400 , 800
F2A , F2B = 600 , 1000
F3A , F3B = 800 , 1200

filename = "A/mftest.csv"

function printf(...)
    local str=string.format(...)
    print(str)
end

function new_log(cam)
    filename = "A/mf_"..cam..".csv"
    log=io.open(filename,"a")
    log:write(os.date(),",MF Test 3.2 Log\n")
    log:close()
end

function fprintf(...)
    log=io.open(filename,"a")
    local str=string.format(...)
    log:write(string.format(...),"\n")
    log:close()
end

function lprintf(...)
    printf(...)
    fprintf(...)
end

function enter_shoot_mode()
    if ( get_mode() == false ) then
        set_record(1)
        while ( get_mode() == false ) do sleep(100) end
    end
    sleep(1000)
end

function check_results(message,f1,r1)
    local f1str = ""
    local rstatus = 0
    if (f1 > 0 ) then
        if ( r1 > 0 ) then if(math.abs(f1-r1) < (f1/5)) then
             rstatus = 1
             f1str="passed"
        else f1str="failed" end
        else f1str="focus_failure" end
    end
    if (f1+r1 == 0 ) then
        fprintf("...%s, , , ,%d,%d,%d,%d",message,get_prop(props.FOCUS_STATE),get_prop(props.AF_LOCK),get_prop(props.FOCUS_MODE),get_prop(props.REAL_FOCUS_MODE))
    else
        fprintf("...%s,%d,%d,%s,%d,%d,%d,%d",message,f1,r1,f1str,get_prop(props.FOCUS_STATE),get_prop(props.AF_LOCK),get_prop(props.FOCUS_MODE),get_prop(props.REAL_FOCUS_MODE))
    end
    return(rstatus)
end

function test_shoot(foc)
    if (foc>0) then set_focus(foc) end
    sleep(1000)
    local count = 0
    local timeout = false
    press("shoot_half")
    repeat
        sleep(100)
        count = count + 1
        if (count > 20 ) then timeout = true end
    until (get_shooting() == true ) or (timeout == true)
    if ( timeout == true ) then
        release("shoot_half")
        getf1=0
    else
        sleep(1000)
        getf1 = get_focus()
        if ( s == 1 ) then
            press("shoot_full")
            sleep(200)
            release("shoot_full");
            sleep(1000)
            getf2 = get_focus()
            if ( getf1 ~= getf2 ) then
                local warn = "warning : possible focus error"
                printf(warn)
                fprintf("%s %d %d",warn,getf1, getf2)
            end
        else
            release("shoot_half")
        end
    end
    return getf1
end

function focus_test(f1, f2, msg)
    if ( msg ~= nil ) then fprintf(msg) end
    sleep(1000)
    check_results("start",0,0)
    local r1 = test_shoot(f1)
    local fstatus=check_results("shot1",f1,r1)
    r2 = test_shoot(f2)
    fstatus = fstatus+check_results("shot2",f2,r2)
    if fstatus == 2 then return(true) end
    return(false)
end

function restore()
    if ((type(set_focus_interlock_bypass) == "function") and (x == 1)) then set_focus_interlock_bypass(0) end
    set_config_value(cfg1.override_disable, cfg_override)
    set_config_value(cfg2.subj_dist_override_koef, cfg_sd)
end

 -- main program
    bi=get_buildinfo()
    version= tonumber(string.sub(bi.build_number,1,1))*100 + tonumber(string.sub(bi.build_number,3,3))*10 + tonumber(string.sub(bi.build_number,5,5))
    if ( tonumber(bi.build_revision) > 0 ) then
        build = tonumber(bi.build_revision)
    elseif tonumber(bi.build_revision) == 0 then -- builds outside of svn will have build rev = 0 by default
        build = 9999
    else
        build = tonumber(string.match(bi.build_number,'-(%d+)$'))
    end
    if ((version<130) or (build<3303)) then
        printf("CHDK 1.3.0 build 3303 or higher required")
        return
    end

    cfg_override = get_config_value(cfg1.override_disable)
    cfg_sd = get_config_value(cfg2.subj_dist_override_koef)
    set_config_value(cfg1.override_disable, 1)  -- ensure SD overrides from menu are off
    set_config_value(cfg2.subj_dist_override_koef, 0)


    new_log(string.format('%x',bi.platformid))
    enter_shoot_mode()
    print_screen(0)
    printf("Set Focus Tester v3.2 %s",string.upper(bi.platform))

    if( props.CONTINUOUS_AF == nil ) then caf=-999 else caf = get_prop(props.CONTINUOUS_AF) end
    if( props.SERVO_AF == nil ) then saf=-999 else saf = get_prop(props.SERVO_AF) end
    cmode = capmode.get_name()
    fprintf("%s,%s,%s,%s-%s", bi.platform, bi.platsub, bi.version, bi.build_number, bi.build_revision)
    fprintf("Mode:%s,Continuous_AF:%d,Servo_AF:%d", cmode,caf,saf)
    fprintf("tests : set_focus:%d, set_aflock:%d, set_mf:%d, bypass:%d",a,b,c,x)
    fprintf("test,set,actual,result,prop.FOCUS_STATE,prop.AF_LOCK,prop.FOCUS_MODE,prop.REAL_FOCUS_MODE")

 -- check initial focus conditions
    lprintf("1) Testing initial conditions")
    if ( cmode ~= "P" ) then printf("   ..warning : not in P mode") end
    if ( props.CONTINUOUS_AF and caf ~= 0 ) then printf("   ..warning : Continuous AF mode enabled") end
    if ( props.SERVO_AF and saf ~= 0 ) then printf("   ..warning : Servo AF enabled") end
    if ( cmode == "P" and caf == 0 and saf == 0 ) then printf("   ..passed") end
    if type(set_focus_interlock_bypass) == "function" then
        fprintf("...interlock bypass function available")
        if ( x == 1 ) then set_focus_interlock_bypass(1) end
    else
        printf("   ..warning : interlock bypass n/a")
        fprintf("...interlock bypass function not available")
    end
    if type(set_mf) == "function" then
        fprintf("...set_mf function available")
    else
        printf("   ..warning : set_mf n/a")
        fprintf("...set_mf function not available")
    end

    check_results("start",0,0)
    R0A = get_focus()
    R0B = test_shoot(0)
    check_results("shot1",R0A,R0B)
    sleep(1000)

 -- test using just set_focus()
    lprintf("2) Testing set_focus() only")
    if ( a == 1 ) then
        if focus_test(F1A, F1B) then printf("   ..passed") else printf("   ..failed") end
    else lprintf("   ..disabled") end
    sleep(1000)

  -- test using set_aflock()
    lprintf("3) Testing set_aflock()")
    if ( b == 1 ) then
        check_results("start",0,0)
        set_aflock(1)
        check_results("set_aflock(1)",0,0)
        set_prop(props.AF_LOCK,1)
        if focus_test(F2A, F2B, "set_prop.AF_LOCK(1)") then printf("   ..passed") else printf("   ..failed") end
        set_prop(props.AF_LOCK,0)
        check_results("set_prop.AF_LOCK(0)",0,0)
        set_aflock(0)
        check_results("set_aflock(0)",0,0)
    else lprintf("   ..disabled") end
    sleep(1000)

 -- test using set_mf()
    lprintf("4) Testing set_mf()")
    if ( c == 1 ) then
        check_results("start",0,0)
        set_mf(1)
        if focus_test(F3A, F3B, "set_mf") then printf("   ..passed") else printf("   ..failed") end
        set_mf(0)
        check_results("set_mf",0,0)
    else lprintf("   ..disabled") end
    sleep(1000)

    lprintf("5) Done")

    restore()

 -- done  --
