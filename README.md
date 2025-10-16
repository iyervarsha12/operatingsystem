# operatingsystem
Own operating system which can schedule threads in a FIFO order, where timed-self-preemption is done using a timer. This was done as a part of CS552 (Introduction to operating systems) with my partner, Timothy Borunov.

# FURTHER INFO
1. Populate our own Global Descriptor Table (required for telling the CPU about memory segments) and Interrupt Descriptor Table (required for telling the CPU where the interrupt service routines are located)
2. Probe the system BIOS and report the usable memory in the machine by passing it from boot.s to init.c via multiboot
3. N threads are created statically at boot time (no dynamic creation of threads supported). Each thread has a Thread Control Block that maintains its state information (thread id and other machine state registers). The thread creation function binds a thread in the pool to specific stack and function addresses.
4. We have a stack thread emulating the main thread and maintain a runqueue of threads, composed of a linked list of TCBs
5. Run a thread by saving state of current running thread, and then switching to the new thread
6. A thread pre-empts itself when it goes over time (13.5ms), which has been implemented using the Programmable Interval Timer (PIT)

# HOW TO RUN

## Just run the following commands in the directory with the files:

#### To build
make

#### To run
qemu-system-i386 -kernel memos-2.elf  -vnc :1

You can use -s -S to view it in debugging easier that way. Our implementation
makes sure that the timer is not running during yield, so if you breakpoint
on yield, and continue, you can essentially see what happens between each
timer interrupt without disrupting the timer itself with the gdb traps.

#### To view in terminal results

/path/to/vncviewer :1

#### PIT configuration 
As for the PIT, we have it configured to be 13.5 ms.
You can reconfigure it by simply changing the byte values in boot.S where
it says we write the high and low bytes to port 0x40 which is the data port
for channel0 at port 0x43. The calculation is as follows for determining
what byte values should be: 

Example for 1ms:
Divisor = 1193182 Hz/reload_value in Hz = 1193182Hz * Value_in_ms/1000
So for 1 ms, it is 1193182 * 1/1000 = 1193 = 0x04A9
so lobyte is 0xA9, and hibyte is 0x04



