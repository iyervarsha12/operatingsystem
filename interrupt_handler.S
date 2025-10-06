
# .global int_table
.globl isrwrapper
.globl isrwrapper2

.section .data
.align 0x4

isrwrapper: 
	# pusha
	# cld
	call isrroutine
	# popa
	iret

isrwrapper2:
	# pusha
	# cld
	call isr2routine
	# popa
	iret
