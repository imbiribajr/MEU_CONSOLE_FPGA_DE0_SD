MEMORY
{
  onchip (rwx) : ORIGIN = 0x0180C000, LENGTH = 0x00004000
}

ENTRY(_start)

SECTIONS
{
  .reset :
  {
    KEEP(*(.reset))
  } > onchip

  .entry :
  {
    KEEP(*(.entry))
  } > onchip

  .exceptions :
  {
    *(.exceptions*)
  } > onchip

  __flash_exceptions_start = LOADADDR(.exceptions);
  __ram_exceptions_start = ADDR(.exceptions);
  __ram_exceptions_end = .;

  .text :
  {
    *(.text*)
  } > onchip

  .rodata :
  {
    *(.rodata*)
    *(.strings*)
  } > onchip

  __flash_rodata_start = LOADADDR(.rodata);
  __ram_rodata_start = ADDR(.rodata);
  __ram_rodata_end = .;

  .got :
  {
    *(.got*)
  } > onchip

  .sdata :
  {
    *(.sdata*)
    *(.sdata2*)
  } > onchip

  .sbss :
  {
    *(.sbss*)
    *(.scommon*)
  } > onchip

  .rwdata :
  {
    *(.rwdata*)
    *(.data*)
    *(.bss*)
    *(COMMON)
  } > onchip

  __flash_rwdata_start = LOADADDR(.rwdata);
  __ram_rwdata_start = ADDR(.rwdata);
  __ram_rwdata_end = .;
  __bss_start = ADDR(.rwdata);
  __bss_end = .;

  . = ALIGN(4);
  _gp = ALIGN(16) + 0x7e00;
  PROVIDE(__global_pointer$ = _gp);
  PROVIDE(_end = .);
  PROVIDE(end = .);

  __alt_stack_pointer = ORIGIN(onchip) + LENGTH(onchip) - 4;
  _alt_stack_pointer = __alt_stack_pointer;
}
