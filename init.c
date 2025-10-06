#include "multiboot.h"
#include "types.h"

typedef struct { //32 bit IDT entry 
	uint16_t offsetLow;
	uint16_t segSelector; //must point to valid code entry in GDT
	uint8_t Reserved;
	uint8_t flags; //p,dpl (2 bit cpu priv levels),0,gate type (we need interrupt gate)
	uint16_t offsetHigh;
} __attribute__((packed)) IDTEntryStruct;

typedef struct { 
	IDTEntryStruct idtentry [48];
} __attribute__((packed)) IDTtable;


extern IDTtable idt; 
extern void isrwrapper(void);
extern void isrwrapper2(void);

/* Hardware text mode color constants. */
enum vga_color
{
  COLOR_BLACK = 0,
  COLOR_BLUE = 1,
  COLOR_GREEN = 2,
  COLOR_CYAN = 3,
  COLOR_RED = 4,
  COLOR_MAGENTA = 5,
  COLOR_BROWN = 6,
  COLOR_LIGHT_GREY = 7,
  COLOR_DARK_GREY = 8,
  COLOR_LIGHT_BLUE = 9,
  COLOR_LIGHT_GREEN = 10,
  COLOR_LIGHT_CYAN = 11,
  COLOR_LIGHT_RED = 12,
  COLOR_LIGHT_MAGENTA = 13,
  COLOR_LIGHT_BROWN = 14,
  COLOR_WHITE = 15,
};
 
uint8_t make_color(enum vga_color fg, enum vga_color bg)
{
  return fg | bg << 4;
}
 
uint16_t make_vgaentry(char c, uint8_t color)
{
  uint16_t c16 = c;
  uint16_t color16 = color;
  return c16 | color16 << 8;
}
 
size_t strlen(const char* str)
{
  size_t ret = 0;
  while ( str[ret] != 0 )
    ret++;
  return ret;
}
 
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 24;
 
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;
 
void terminal_initialize()
{
  terminal_row = 0;
  terminal_column = 0;
  terminal_color = make_color(COLOR_LIGHT_GREY, COLOR_BLACK);
  terminal_buffer = (uint16_t*) 0xB8000;
  for ( size_t y = 0; y < VGA_HEIGHT; y++ )
    {
      for ( size_t x = 0; x < VGA_WIDTH; x++ )
	{
	  const size_t index = y * VGA_WIDTH + x;
	  terminal_buffer[index] = make_vgaentry(' ', terminal_color);
	}
    }
}
 
void terminal_setcolor(uint8_t color)
{
  terminal_color = color;
}
 
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
  const size_t index = y * VGA_WIDTH + x;
  terminal_buffer[index] = make_vgaentry(c, color);
}
 
void terminal_putchar(char c)
{
  /*    if(c == '\n'){
      terminal_row++;
        if ( ++terminal_row == VGA_HEIGHT )
    	{
    	  terminal_row = 0;
    	}
    terminal_column  = 0;
    }*/
  terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
  if ( ++terminal_column == VGA_WIDTH )
    {
      terminal_column = 0;
      if ( ++terminal_row == VGA_HEIGHT )
	{
	  terminal_row = 0;
	  terminal_initialize();
	}
    }
}
 
void terminal_writestring(const char* data)
{
  size_t datalen = strlen(data);
  for ( size_t i = 0; i < datalen; i++ )
    terminal_putchar(data[i]);
}
 

/* Convert the integer D to a string and save the string in BUF. If
   BASE is equal to 'd', interpret that D is decimal, and if BASE is
   equal to 'x', interpret that D is hexadecimal. */
void itoa (char *buf, int base, int d)
{
  char *p = buf;
  char *p1, *p2;
  unsigned long ud = d;
  int divisor = 10;
     
  /* If %d is specified and D is minus, put `-' in the head. */
  if (base == 'd' && d < 0)
    {
      *p++ = '-';
      buf++;
      ud = -d;
    }
  else if (base == 'x')
    divisor = 16;
     
  /* Divide UD by DIVISOR until UD == 0. */
  do
    {
      int remainder = ud % divisor;
     
      *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    }
  while (ud /= divisor);
     
  /* Terminate BUF. */
  *p = 0;
     
  /* Reverse BUF. */
  p1 = buf;
  p2 = p - 1;
  while (p1 < p2)
    {
      char tmp = *p1;
      *p1 = *p2;
      *p2 = tmp;
      p1++;
      p2--;
    }
}

#define MAX_THREADS 10
#define STACK_SIZE 400 //in bytes

char stacksthread[MAX_THREADS][STACK_SIZE];

enum thread_status {
  TS_EXITED,
  TS_RUNNING,
  TS_READY,
  TS_BLOCKED,
  TS_NEXT
};

struct TCB {
  uint8_t tid;
  uint32_t bp;
  uint32_t sp;
  uint32_t entry;
  uint8_t flag;
  enum thread_status TS;
  struct TCB *next;
};

static struct TCB *run_queue;
static struct TCB *last_thread;
static struct TCB threads[MAX_THREADS];
static bool avail[MAX_THREADS];
static int last_used;

int get_tcb() {
  int new_tcb = (last_used + 1) % MAX_THREADS;
  int count = 0;
  while(!avail[new_tcb] && count < MAX_THREADS) {
    new_tcb = (new_tcb + 1) % MAX_THREADS;
    count++;
  }
  if (count == MAX_THREADS) {
    return -1;
  }
  return new_tcb;
}

void runqueue_add(struct TCB * new_thread) {
  last_thread->next = new_thread;
  new_thread->next = run_queue;
  last_thread = new_thread;
}

static void yield() {
  static struct TCB *tmp;
  last_thread = run_queue;
  run_queue = run_queue->next;
  __asm__ volatile (

		    //"push $0x00101507\n\t"
		    "push %[yield_add]\n\t"
		    "pushf\n\t"
		    "pusha\n\t"
		    "pushw %%ds\n\t"
		    "pushw %%es\n\t"
		    "pushw %%fs\n\t"
		    "pushw %%gs\n\t"

		    "mov %%esp, %[old_esp]\n\t"
		    "mov %[new_esp], %%esp\n\t"

		    "popw %%gs\n\t"
		    "popw %%fs\n\t"
		    "popw %%es\n\t"
		    "popw %%ds\n\t"
		    "popa\n\t"
		    "popf\n\t"
		    "push %%eax\n\t"
		    "movb $0x20, %%al\n\t"
		    "outb %%al, $0x20\n\t"
		    "pop %%eax\n\t"
		    "ret\n\t"
		    : [old_esp] "=m" (last_thread->sp)
		    : [new_esp] "r" (run_queue->sp), [yield_add] "r" ((uint32_t)((void*) yield) + 86)
		    : "%eax", "%esp", "memory");
  
}



/* gdb to view what's happening in memory itself 
 * to qemu, -hda, can do qemu -kernel to test. has its own grub and it'll load kernel for you
 * -s -S halts execution of kernel until you send signal from gdb 
 * another window run gdb on elf binary
 */

void exit_thread() {
  __asm__ volatile ("cli\n\t");
  int tid = run_queue->tid;
  run_queue->TS = TS_EXITED;
  // FREE THE STACK
  run_queue = run_queue->next;
  // FREE THE THREAD
  last_thread->next = run_queue;
  
  avail[tid] = TRUE;

  int dummy;
  __asm__ volatile (
	"mov %[new_esp], %%esp\n\t"
	"popw %%gs\n\t"
	"popw %%fs\n\t"
	"popw %%es\n\t"
	"popw %%ds\n\t"
	"popa\n\t"
	"popf\n\t"
	"sti\n\t"
	"ret\n\t"
	: "=r" (dummy)
	: [new_esp] "r" (run_queue->sp)
		    );
}

int thread_create (void *stack, void *func(void)) {
  int new_tcb = -1;
  uint16_t ds = 0x10, es = 0x10, fs = 0x10, gs = 0x10;

  new_tcb = get_tcb();
  if (new_tcb == -1) {
    terminal_writestring("No TCB available!\n");
    return -1;
  }

  *(((uint32_t *) stack) - 0) = (uint32_t) exit_thread;
  stack = (void *) (((uint32_t *) stack) - 1);

  threads[new_tcb].tid = new_tcb;
  threads[new_tcb].bp = (uint32_t) stack;
  threads[new_tcb].entry = (uint32_t) func;
  threads[new_tcb].flag = 0;
  threads[new_tcb].TS = TS_READY;  
  threads[new_tcb].next = NULL;
  //threads[new_tcb].eip = NULL;

  threads[new_tcb].sp = (uint32_t) (((uint16_t *) stack) - 22);

  *(((uint32_t *) stack) - 0) = threads[new_tcb].entry;
  *(((uint32_t *) stack) - 1) = 0 | (1 << 9);
  *(((uint32_t *) stack) - 2) = 0;
  *(((uint32_t *) stack) - 3) = 0;
  *(((uint32_t *) stack) - 4) = 0;
  *(((uint32_t *) stack) - 5) = 0;
  *(((uint32_t *) stack) - 6) = (uint32_t) (((uint32_t *) stack) - 2);
  *(((uint32_t *) stack) - 7) = (uint32_t) (((uint32_t *) stack) - 2);
  *(((uint32_t *) stack) - 8) = 0;
  *(((uint32_t *) stack) - 9) = 0;

  *(((uint16_t *) stack) - 19) = ds;
  *(((uint16_t *) stack) - 20) = es;
  *(((uint16_t *) stack) - 21) = fs;
  *(((uint16_t *) stack) - 22) = gs;

  avail[new_tcb] = FALSE;
  runqueue_add(&threads[new_tcb]);
  return 0;
}


void foo1(void) {
  terminal_writestring("\nThread:");
  char * temp;
  itoa(temp,'d',run_queue->tid);
  terminal_writestring(temp);
  terminal_writestring(" is active\n");
  int counter = 0;
  char prcount[10];
  while(counter < 10000000) {
    counter++;
    if((counter%1000000) == 0) {
      itoa(temp,'d',run_queue->tid);
      itoa(prcount,'d',counter);
      terminal_writestring("\nThread ");
      terminal_writestring(temp);
      terminal_writestring(" , counter = ");
      terminal_writestring(prcount);
    }
  }
  itoa(temp,'d',run_queue->tid);
  terminal_writestring("\nThread ");
  terminal_writestring(temp);
  terminal_writestring(" died");
}

void foo2(void) {
  terminal_writestring("\nThread:");
  char * temp;
  itoa(temp,'d',run_queue->tid);
  terminal_writestring(temp);
  terminal_writestring(" in foo2\n");
  terminal_writestring("\nThread died");
}

void init( multiboot* pmb ) {

   memory_map_t *mmap;
   unsigned int memsz = 0;		/* Memory size in MB */
   static char memstr[10];

  for (mmap = (memory_map_t *) pmb->mmap_addr;
       (unsigned long) mmap < pmb->mmap_addr + pmb->mmap_length;
       mmap = (memory_map_t *) ((unsigned long) mmap
				+ mmap->size + 4 /*sizeof (mmap->size)*/)) {
    
    if (mmap->type == 1)	/* Available RAM -- see 'info multiboot' */
      memsz += mmap->length_low;
  }

  /* Convert memsz to MBs */
  memsz = (memsz >> 20) + 1;	/* The + 1 accounts for rounding
				   errors to the nearest MB that are
				   in the machine, because some of the
				   memory is othrwise allocated to
				   multiboot data structures, the
				   kernel image, or is reserved (e.g.,
				   for the BIOS). This guarantees we
				   see the same memory output as
				   specified to QEMU.
				    */
 
  itoa(memstr, 'd', memsz);

  terminal_initialize();
  
  
  terminal_writestring("MemOS: Welcome *** System memory is: ");
  terminal_writestring(memstr);
  terminal_writestring("MB");

  void *addr = __builtin_extract_return_addr (__builtin_return_address (0)); 

  //////////////////////////////// CURSED

  int new_tcb = 1;
  uint16_t ds = 0x10, es = 0x10, fs = 0x10, gs = 0x10;
  void* stack = stacksthread[1];
  //  *(((uint32_t *) stack) - 0) = (uint32_t) addr;
  //stack = (void *) (((uint32_t *) stack) - 1);

  threads[new_tcb].tid = new_tcb;
  threads[new_tcb].bp = (uint32_t) stack;
  threads[new_tcb].entry = (uint32_t) addr;
  threads[new_tcb].flag = 0;
  threads[new_tcb].TS = TS_READY;  
  threads[new_tcb].next = NULL;
  //threads[new_tcb].eip = NULL;

  threads[new_tcb].sp = (uint32_t) (((uint16_t *) stack) - 22);

  *(((uint32_t *) stack) - 0) = threads[new_tcb].entry;
  *(((uint32_t *) stack) - 1) = 0 | (1 << 9);
  *(((uint32_t *) stack) - 2) = 0;
  *(((uint32_t *) stack) - 3) = 0;
  *(((uint32_t *) stack) - 4) = 0;
  *(((uint32_t *) stack) - 5) = 0;
  *(((uint32_t *) stack) - 6) = (uint32_t) (((uint32_t *) stack) - 2);
  *(((uint32_t *) stack) - 7) = (uint32_t) (((uint32_t *) stack) - 2);
  *(((uint32_t *) stack) - 8) = 0;
  *(((uint32_t *) stack) - 9) = 0;

  *(((uint16_t *) stack) - 19) = ds;
  *(((uint16_t *) stack) - 20) = es;
  *(((uint16_t *) stack) - 21) = fs;
  *(((uint16_t *) stack) - 22) = gs;

  avail[new_tcb] = FALSE;


  //////////////////////////////// END
	
  run_queue = &threads[1];
  last_thread = &threads[1];
  
  for (int i = 2; i < MAX_THREADS; i++) { //TODO: why is this 1-indexed? 
    avail[i] = TRUE;
    thread_create(stacksthread[i], (void *)foo1);
  }
  last_used = 1;
  
  
  /*
  while(run_queue->next->tid) {
    yield();
    terminal_writestring("\nReturned back to main from foo1\n");
  }
  */

  /*for (int i = 1; i < MAX_THREADS; i++) { //TODO: why is this 1-indexed? 
    thread_create(stacksthread[i], (void *)foo2);
  }
  last_used = 0;

  
  terminal_writestring("\nReturned back to main from foo2\n");

    for (int i = 1; i < MAX_THREADS; i++) { //TODO: why is this 1-indexed? 
    thread_create(stacksthread[i], (void *)foo2);
  }
  last_used = 0;
  */  
 
  int dummy;
  __asm__ volatile (
	"mov %[new_esp], %%esp\n\t"
	"popw %%gs\n\t"
	"popw %%fs\n\t"
	"popw %%es\n\t"
	"popw %%ds\n\t"

	"popa\n\t"
	"popf\n\t"
	"ret\n\t"
	: "=r" (dummy)
	: [new_esp] "r" (run_queue->sp)
		    );

  terminal_writestring("\nReturned back to main for last time\n");

}

void idtpopulate() {
	// change idtentry[32] to point to isrwrapper2 to test.
	uint32_t func = (uint32_t) ((void *) isrwrapper);
	uint32_t pitfunc = (uint32_t) ((void *) isrwrapper2);
	for (int i = 0; i < 48; i++) { 
		if (i== 32) {
			idt.idtentry[i].offsetLow = pitfunc & 0xFFFF;
			idt.idtentry[i].segSelector = 0x08;
			idt.idtentry[i].Reserved = 0;
			idt.idtentry[i].flags = 0x8e;
			idt.idtentry[i].offsetHigh =  pitfunc >> 16;
		} else {
			idt.idtentry[i].offsetLow = func & 0xFFFF;
			idt.idtentry[i].segSelector = 0x08;
			idt.idtentry[i].Reserved = 0;
			idt.idtentry[i].flags = 0x8e;
			idt.idtentry[i].offsetHigh =  func >> 16;
		}
	}
}


static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %b0, %w1" : : "a"(val), "Nd"(port) : "memory");
}

void isrroutine() {
	terminal_initialize();
	terminal_writestring("\nScrew you for triggering any other interrupt! - Timo and Vi\n");
	outb(0x20, 0x20); //Indicate to PIC that you've handled interrupt, EOI
}

void isr2routine() {
  //terminal_writestring("\nPIT is triggered - \n");
  //outb(0x20, 0x20); //Indicate to PIC that you've handled interrupt, EOI
  yield();
  
}







