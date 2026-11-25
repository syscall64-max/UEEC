.syntax unified
.thumb
.section .isr_vector,"a",%progbits
.word _stack_top
.word Reset_Handler
.rept 13
.word Default_Handler
.endr
.word SysTick_Handler

.section .text.Reset_Handler,"ax",%progbits
.thumb_func
.global Reset_Handler
Reset_Handler:
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
1:  cmp r1, r2
    bcs 2f
    ldr r3, [r0], #4
    str r3, [r1], #4
    b 1b
2:  ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
3:  cmp r1, r2
    bcs 4f
    str r3, [r1], #4
    b 3b
4:  bl main
5:  b 5b

.section .text.Default_Handler,"ax",%progbits
.thumb_func
.global Default_Handler
Default_Handler:
    b .
