OUTPUT_FORMAT ("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")

SECTIONS
{
  . = 0x1FFFFF00;
  .ro : {
    *(.text*)
    *(.rodata*)
  }
  .rw : {
    . = ALIGN(4);
    *(.data)
    *(.got*)
    *(.data.*)
    *(.bss)
    *(COMMON)
  }

  /DISCARD/ : { *(.interp*) }
  /DISCARD/ : { *(.dynsym) }
  /DISCARD/ : { *(.dynstr*) }
  /DISCARD/ : { *(.dynamic*) }
  /DISCARD/ : { *(.gnu*) }
  /DISCARD/ : { *(.rel*) }
}
