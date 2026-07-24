/* Placeholder linker script — replace with board-accurate memory map. */
MEMORY
{
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 512K
  RAM (rwx)  : ORIGIN = 0x20000008, LENGTH = 192K
}
/* Sections omitted — wire during silicon bring-up with CMSIS startup. */
