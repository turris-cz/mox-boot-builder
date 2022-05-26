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
    compressed_start = .;
    KEEP(*(.compressed_data));
    compressed_end = .;
    KEEP(*(.data*));
    KEEP(*(.bss*));
  }
  . = ALIGN(8);
  . = . + 0x1000;
  stack_top = .;
  malloc_start = .;
  . = . + 0xc000;
  malloc_end = .;

  /DISCARD/ : { *(.interp*) }
  /DISCARD/ : { *(.dynsym) }
  /DISCARD/ : { *(.dynstr*) }
  /DISCARD/ : { *(.dynamic*) }
  /DISCARD/ : { *(.gnu*) }
  /DISCARD/ : { *(.rel*) }
  /DISCARD/ : { *(.ARM*) }
}
