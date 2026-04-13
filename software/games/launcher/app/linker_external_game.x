/*
 * Linker script for games loaded dynamically by the launcher.
 *
 * This image is copied directly by the launcher to SDRAM. The BSP runtime
 * still expects the usual HAL symbols, so we keep a compatible layout while
 * forcing all loadable content into SDRAM.
 */

MEMORY
{
    new_sdram_controller_0 : ORIGIN = 0x00800000, LENGTH = 0x00800000
    onchip_memory2_0       : ORIGIN = 0x0180C000, LENGTH = 0x00004000
}

__alt_mem_new_sdram_controller_0 = 0x00800000;
__alt_mem_onchip_memory2_0 = 0x0180C000;

OUTPUT_FORMAT( "elf32-littlenios2",
               "elf32-littlenios2",
               "elf32-littlenios2" )
OUTPUT_ARCH( nios2 )
ENTRY( _start )

SECTIONS
{
    .entry :
    {
        KEEP(*(.entry))
    } > onchip_memory2_0

    .exceptions :
    {
        PROVIDE(__ram_exceptions_start = ABSOLUTE(.));
        KEEP(*(.irq))
        KEEP(*(.exceptions*))
        PROVIDE(__ram_exceptions_end = ABSOLUTE(.));
    } > new_sdram_controller_0

    PROVIDE(__flash_exceptions_start = LOADADDR(.exceptions));

    .text :
    {
        PROVIDE(stext = ABSOLUTE(.));
        KEEP(*(.init))
        *(.text .text.* .gnu.linkonce.t.*)
        *(.rodata .rodata.* .gnu.linkonce.r.*)
        *(.strings*)
        KEEP(*(.fini))

        . = ALIGN(4);
        PROVIDE(__etext = ABSOLUTE(.));
        PROVIDE(_etext = ABSOLUTE(.));
        PROVIDE(etext = ABSOLUTE(.));
    } > new_sdram_controller_0

    .rwdata :
    {
        . = ALIGN(4);
        PROVIDE(__ram_rwdata_start = ABSOLUTE(.));
        *(.got.plt) *(.got)
        *(.data .data.* .gnu.linkonce.d.*)
        *(.rwdata .rwdata.*)
        *(.sdata .sdata.* .gnu.linkonce.s.*)
        *(.sdata2 .sdata2.* .gnu.linkonce.s2.*)

        . = ALIGN(4);
        _gp = ABSOLUTE(. + 0x8000);
        PROVIDE(gp = _gp);
        PROVIDE(__global_pointer$ = _gp);

        . = ALIGN(4);
        _edata = ABSOLUTE(.);
        PROVIDE(edata = ABSOLUTE(.));
        PROVIDE(__ram_rwdata_end = ABSOLUTE(.));
    } > new_sdram_controller_0

    PROVIDE(__flash_rwdata_start = LOADADDR(.rwdata));

    .bss :
    {
        . = ALIGN(4);
        __bss_start = ABSOLUTE(.);
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
        *(.scommon)
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = ABSOLUTE(.);
    } > new_sdram_controller_0

    .rodata_dummy :
    {
        PROVIDE(__ram_rodata_start = ABSOLUTE(.));
        PROVIDE(__ram_rodata_end = ABSOLUTE(.));
    } > new_sdram_controller_0

    PROVIDE(__flash_rodata_start = __ram_rodata_start);

    . = ALIGN(4);
    PROVIDE(_end = .);
    PROVIDE(end = .);

    __alt_stack_pointer = ORIGIN(new_sdram_controller_0) + LENGTH(new_sdram_controller_0) - 4;
    _alt_stack_pointer = __alt_stack_pointer;
}
