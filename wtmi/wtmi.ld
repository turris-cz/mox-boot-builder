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
  . = 0x20000000;
  ebg_buffer = .;
  . = 0x20001000;
  paranoid_rand_buffer = .;
  . = 0x20002000;
  paranoid_rand_dgst = .;
  . = 0x20002040;
}
