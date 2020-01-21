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
    . = ALIGN(0x800);
    vbar_start = .;
    *(.vbar0)
    . = ALIGN(0x80);
    *(.vbar1)
    . = ALIGN(0x80);
    *(.vbar2)
    . = ALIGN(0x80);
    *(.vbar3)
    . = ALIGN(0x80);
    *(.vbar4)
    . = ALIGN(0x80);
    *(.vbar5)
    . = ALIGN(0x80);
    *(.vbar6)
    . = ALIGN(0x80);
    *(.vbar7)
    . = ALIGN(0x80);
    *(.vbar8)
    . = ALIGN(0x80);
    *(.vbar9)
    . = ALIGN(0x80);
    *(.vbarA)
    . = ALIGN(0x80);
    *(.vbarB)
    . = ALIGN(0x80);
    *(.vbarC)
    . = ALIGN(0x80);
    *(.vbarD)
    . = ALIGN(0x80);
    *(.vbarE)
    . = ALIGN(0x80);
    *(.vbarF)
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
