OUTPUT_FORMAT("elf64-littleaarch64", "elf64-littleaarch64", "elf64-littleaarch64")
OUTPUT_ARCH(aarch64)

ENTRY(start)

SECTIONS
{
  . = 0x10000000;
  .ro : {
    KEEP(*(.start));
    *(.text*)
    *(.rodata*)
  }
  .rw : {
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

  /DISCARD/ : { *(.dynsym) }
  /DISCARD/ : { *(.dynstr*) }
  /DISCARD/ : { *(.dynamic*) }
  /DISCARD/ : { *(.plt*) }
  /DISCARD/ : { *(.interp*) }
  /DISCARD/ : { *(.gnu*) }
  /DISCARD/ : { *(.eh_frame) }
  /DISCARD/ : { *(.comment) }
}
