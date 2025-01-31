# operatingsystem
Made my own operating system with a FIFO scheduler. It has support for interrupts with the IDT set up, Self-Preemption with Stacks, and timed pre-emption. This was done as a part of CS552 (Introduction to operating systems) with my partner, Timothy Borunov. It also finds usable memory in the system. 
The GDT, IDT and PIT setup is done in boot.s. 
init.c has the code to find usable memory, create different threads, etc.

# HOW TO RUN

## Just run the following commands in the directory with the files:

// To build
make

// To run
qemu-system-i386 -kernel memos-2.elf  -vnc :1

// You can use -s -S to view it in debugging easier that way. Our implementation
// makes sure that the timer is not running during yield, so if you breakpoint
// on yield, and continue, you can essentially see what happens between each
// timer interrupt without disrupting the timer itself with the gdb traps.

// To view in terminal results

/path/to/vncviewer :1

/*

As for the PIT, we have it configured to be 13.5 ms.
You can reconfigure it by simply changing the byte values in boot.S where
it says we write the high and low bytes to port 0x40 which is the data port
for channel0 at port 0x43. The calculation is as follows for determining
what byte values should be: 

Example for 1ms:
Divisor = 1193182 Hz/reload_value in Hz = 1193182Hz * Value_in_ms/1000
So for 1 ms, it is 1193182 * 1/1000 = 1193 = 0x04A9
so lobyte is 0xA9, and hibyte is 0x04

*/

