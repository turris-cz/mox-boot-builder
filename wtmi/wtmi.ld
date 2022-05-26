OUTPUT_FORMAT ("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")

ENTRY(reset_handler)

SECTIONS
{
  . = 0x1FFF0000;
  .ro : {
    KEEP(*(.isr_vector));
    KEEP(*(.startup));
    KEEP(*(.main));
    *(.text*)
    *(.rodata*)
  }
  .rw : {
    KEEP(*(.from_mox_builder));
    . = ALIGN(4);
    debug_cmds_start = .;
    KEEP(*(.debug_cmds));
    debug_cmds_end = .;
    KEEP(*(.data*));
    KEEP(*(.bss*));
  }
  . = 0x1FFFF000;
  stack_top = .;
  . = 0x20000000;
  ebg_buffer = .;
  . = . + 0x1000;
  paranoid_rand_buffer = .;
  . = . + 0x1000;
  paranoid_rand_dgst = .;
  . = . + 0x40;
  paranoid_rand_tmp = .;
  . = . + 0x240;
  next_timh_image = .;
  . = . + 0x2000;
  next_timn_image = .;
  . = . + 0x2000;
  next_wtmi = .;

  /DISCARD/ : { *(.interp*) }
  /DISCARD/ : { *(.dynsym) }
  /DISCARD/ : { *(.dynstr*) }
  /DISCARD/ : { *(.dynamic*) }
  /DISCARD/ : { *(.gnu*) }
  /DISCARD/ : { *(.rel*) }
  /DISCARD/ : { *(.ARM*) }
}
