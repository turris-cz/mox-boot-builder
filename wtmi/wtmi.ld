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
    *(.data)
    *(.got*)
    *(.data.*)
    *(.bss)
    *(COMMON)
  }
  . = ALIGN(8);
  . = . + 0x2000; /* 8kB of stack memory */
  stack_top = .;
}
