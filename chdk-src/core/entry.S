    .section .entry
#ifndef THUMB_FW
    MOV     R0, #2
    TEQ     R0, #2
    LDR     SP, =MEMBASEADDR
    MOV     R11, #0
#else
    .code 16
    .syntax unified
#endif
    B       startup
