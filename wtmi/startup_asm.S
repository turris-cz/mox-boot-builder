.cpu cortex-m3

.section .text
.thumb

.thumb_func
.global reset_handler
reset_handler:
	ldr	r2, =stack_top
	msr	psp, r2				/* Init Process stack pointer */
	msr	msp, r2				/* Init main stack pointer */
	ldr	r1, =main
	bx	r1

.end
