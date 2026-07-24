/**
 * Minimal Cortex-M4 startup for STM32WB55 (Flipper F7 class).
 * GNU assembler (arm-none-eabi-gcc -x assembler-with-cpp).
 */
  .syntax unified
  .cpu cortex-m4
  .fpu fpv4-sp-d16
  .thumb

  .global g_pfnVectors
  .global Default_Handler
  .global Reset_Handler

  .word _sidata
  .word _sdata
  .word _edata
  .word _sbss
  .word _ebss

  .section .text.Reset_Handler
  .weak Reset_Handler
  .type Reset_Handler, %function
Reset_Handler:
  ldr   r0, =_stack_end
  mov   sp, r0

  /* Copy .data */
  ldr   r0, =_sdata
  ldr   r1, =_edata
  ldr   r2, =_sidata
  movs  r3, #0
  b     1f
0:
  ldr   r4, [r2, r3]
  str   r4, [r0, r3]
  adds  r3, r3, #4
1:
  adds  r4, r0, r3
  cmp   r4, r1
  bcc   0b

  /* Zero .bss */
  ldr   r2, =_sbss
  ldr   r4, =_ebss
  movs  r3, #0
  b     3f
2:
  str   r3, [r2]
  adds  r2, r2, #4
3:
  cmp   r2, r4
  bcc   2b

  /* SystemInit weak + main */
  bl    SystemInit
  bl    main
  b     .

  .size Reset_Handler, .-Reset_Handler

  .section .text.Default_Handler,"ax",%progbits
Default_Handler:
  b     Default_Handler
  .size Default_Handler, .-Default_Handler

  /* SystemInit provided by C (bringup system_*.c or glk_usb_rcc.c) */

  .section .isr_vector,"a",%progbits
  .type g_pfnVectors, %object
g_pfnVectors:
  .word _stack_end
  .word Reset_Handler
  .word NMI_Handler
  .word HardFault_Handler
  .word MemManage_Handler
  .word BusFault_Handler
  .word UsageFault_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word SVC_Handler
  .word DebugMon_Handler
  .word 0
  .word PendSV_Handler
  .word SysTick_Handler
  /* External IRQs (STM32WB55) — first 16 enough for GDO0 EXTI1 */
  .word Default_Handler /* 0  WWDG */
  .word Default_Handler /* 1  PVD */
  .word Default_Handler /* 2  TAMP */
  .word Default_Handler /* 3  RTC_WKUP */
  .word Default_Handler /* 4  FLASH */
  .word Default_Handler /* 5  RCC */
  .word Default_Handler /* 6  EXTI0 */
  .word EXTI1_IRQHandler /* 7  EXTI1 — CC1101 GDO0 on PA1 */
  .word Default_Handler /* 8  EXTI2 */
  .word Default_Handler /* 9  EXTI3 */
  .word Default_Handler /* 10 EXTI4 */
  .word Default_Handler /* 11 DMA1_CH1 */
  .word Default_Handler /* 12 DMA1_CH2 */
  .word Default_Handler /* 13 DMA1_CH3 */
  .word Default_Handler /* 14 DMA1_CH4 */
  .word Default_Handler /* 15 DMA1_CH5 */
  .rept 48
  .word Default_Handler
  .endr
  .size g_pfnVectors, .-g_pfnVectors

  .weak NMI_Handler
  .thumb_set NMI_Handler, Default_Handler
  .weak HardFault_Handler
  .thumb_set HardFault_Handler, Default_Handler
  .weak MemManage_Handler
  .thumb_set MemManage_Handler, Default_Handler
  .weak BusFault_Handler
  .thumb_set BusFault_Handler, Default_Handler
  .weak UsageFault_Handler
  .thumb_set UsageFault_Handler, Default_Handler
  .weak SVC_Handler
  .thumb_set SVC_Handler, Default_Handler
  .weak DebugMon_Handler
  .thumb_set DebugMon_Handler, Default_Handler
  .weak PendSV_Handler
  .thumb_set PendSV_Handler, Default_Handler
  .weak SysTick_Handler
  .thumb_set SysTick_Handler, Default_Handler
  .weak EXTI1_IRQHandler
  .thumb_set EXTI1_IRQHandler, Default_Handler
