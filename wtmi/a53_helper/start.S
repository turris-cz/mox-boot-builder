.arch armv8-a+crc

.section .start
.global start
start:
	ldr x2, stack_core0
	mov sp, x2
	b main
.L:
	b .L

.section .text
.global run_on_core1_helper_asm
run_on_core1_helper_asm:
	dsb sy
	ldr x2, stack_core1
	mov sp, x2
	b run_on_core1_helper
