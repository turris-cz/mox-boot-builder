OUTPUT_FORMAT("elf64-littleaarch64", "elf64-littleaarch64", "elf64-littleaarch64")
OUTPUT_ARCH(aarch64)

ENTRY(start)

SECTIONS
{
  . = 0x04100000;
  .ro : {
    KEEP(*(.start));
    *(.text*)
    *(.rodata*)
  }
  .rw : {
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
