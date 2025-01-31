/*
 * boot.S for memos-2
 *
 */
	.globl stack
	.globl idt

	.bss
	.align 0x1000
	.comm stack, 0x1000
	.comm idt, 0x800

	.data
	.align 0x4

gdt:
	.long 0
	.long 0

	.long 0x0000FFFF
	.long 0x00CF9A00

	.long 0x0000FFFF
	.long 0x00CF9200

gdt_ptr:
	.short 0x7FF
	.long gdt

idt_ptr:
	.short 0x7FF
	.long idt

	.text

	.globl _start
	
_start:
	jmp real_start

	/* Multiboot header -- Safe to place this header in 1st page for GRUB */
	.align 4
	.long 0x1BADB002 /* Multiboot magic number */
	.long 0x00000003 /* Align modules to 4KB, req. mem size */
			 /* See 'info multiboot' for further info */
	.long 0xE4524FFB /* Checksum */

real_start:

	lgdt gdt_ptr
	
	# Populate idt entries 
	# movl $stack+0x1000, %esp
	# lea %eax, isr_wrapper
	# pushl %eax
	
	ljmp $0x08, $1f
1:
	call idtpopulate
	movw $0x10, %ax
	movw %ax, %ss
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs

	movl $stack+0x1000, %esp

	pushl %ebx

	lidt idt_ptr
	sti
	# int $0x20 # to test interrupt handling
	
	# Setup PIT
	cli
	pushf
	mov $0x34, %al # Cmd reg 00 11 010 00 (binmode, mode2, access lohi, ch0)
	out %al, $0x43 # Sending to 0x43 mode/cmd register
	mov $0x89, %al # 1.19MHz PIT, 1193182 means 1 interrupt per sec 
	out %al, $0x40 # Sending (low byte) to channel0 0x40
	mov $0x40, %al 
	out %al, $0x40 # Sending (high byte) to channel0 0x40
	popf
// 	0x89, 0x40		
	# Setup PIC and remap
	# call picremap
	
	# save masks
	inb $0x21, %al
	mov %al, %cl
	inb $0xA1, %al
	mov %al, %ch
	
	# init
	mov $0x11, %al
	outb %al, $0x20
	mov $0x0, %al
	outb %al, $0x80 # iowait
	mov $0x11, %al
	outb %al, $0xA0
	mov $0x0, %al
	outb %al, $0x80 # iowait
	
	# master and slave PIC vector offset 
	mov $0x20, %al
	outb %al, $0x21
	mov $0x0, %al
	outb %al, $0x80 # iowait
	mov $0x28, %al 
	outb %al, $0xA1
	mov $0x0, %al
	outb %al, $0x80 # iowait
	
	mov $0x4, %al
	outb %al, $0x21
	mov $0x0, %al
	outb %al, $0x80 # iowait
	mov $0x2, %al 
	outb %al, $0xA1
	mov $0x0, %al
	outb %al, $0x80 # iowait
	
	# use 8086 mode
	mov $0x1, %al
	outb %al, $0x21
	mov $0x0, %al
	outb %al, $0x80 # iowait
	mov $0x1, %al
	outb %al, $0xA1
	mov $0x1, %al
	outb %al, $0x80 # iowait
	
	# restore saved masks
	mov %cl, %al
	outb %al, $0x21
	mov %ch, %al
	outb %al, $0xA1
	
	# int $0x10 # to test interrupt handling  
	call init
	sti
	# cli
	hlt
loop:
	jmp loop











