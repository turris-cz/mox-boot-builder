OUTPUT_FORMAT ("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")

GROUP(libgcc.a)

ENTRY(reset_handler)

SECTIONS
{
  . = 0x1FFF0000;
  .ro : {
    KEEP(*(.isr_vector));
    *(.text*)
    *(.rodata*)
  }
  .rw : {
    KEEP(*(.from_mox_builder));
    . = ALIGN(4);
    debug_cmds_start = .;
    KEEP(*(.debug_cmds));
    debug_cmds_end = .;
    *(.data)
    *(.got*)
    *(.data.*)
    *(.bss)
    *(COMMON)
  }
  . = 0x1FFFF000;
  stack_top = .;
}
