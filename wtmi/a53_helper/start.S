.arch armv8-a+crc

.section .start
.global start
start:
	ldr x2, stack_core0
	mov sp, x2
	b main
.L:
	b .L
