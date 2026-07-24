/* GrokLink OS bring-up linker script — STM32WB55 / Flipper F7 class */
ENTRY(Reset_Handler)

_stack_end = 0x20030000;
_stack_size = 0x1000; /* 4 KiB — GUI + RPC need headroom */

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
  RAM1  (xrw) : ORIGIN = 0x20000008, LENGTH = 0x2FFF8
}

SECTIONS
{
  .isr_vector : {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .glk_meta : {
    . = ALIGN(4);
    KEEP(*(.glk_meta))
    . = ALIGN(4);
  } >FLASH

  .text : {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata : {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    KEEP(*(.rodata.glk))
    . = ALIGN(4);
  } >FLASH

  _sidata = LOADADDR(.data);

  .data : {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM1 AT> FLASH

  .bss : {
    . = ALIGN(4);
    _sbss = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    end = .; /* newlib _sbrk */
    _end = .;
  } >RAM1

  /DISCARD/ : {
    *(.comment)
    *(.note*)
  }
}
