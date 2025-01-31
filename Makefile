CC = gcc
CFLAGS = -m32 -fno-builtin -fno-stack-protector -fno-strict-aliasing -fno-delete-null-pointer-checks -nostdinc -I. -g -Wall -std=c99
CPPFLAGS = -Wa,--32 -MMD
OBJS = boot.o init.o interrupt_handler.o
PROGS = memos-2.elf
MNT_POINT=/mnt/C

all: $(PROGS)

memos-2.elf: $(OBJS)
	$(LD) -m elf_i386 -T memos.ld -o $@ $^

%: %.c %.S

install: $(PROGS)
	cp $(PROGS) $(MNT_POINT)/boot
	sync

clean:
	-rm *.o *.d $(PROGS) *~

-include *.d
