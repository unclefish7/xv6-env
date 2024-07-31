// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

#define NCPU 8

struct cpu_kmem {
  struct spinlock lock;
  struct run *freelist;
};

struct cpu_kmem kmem[NCPU];

void
kinit()
{
  // 初始化每CPU的锁
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmem[i].lock, "kmem");
  }
  freerange(end, (void*)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end) {
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
  
  // 初始化每CPU的锁
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmem[i].lock, "kmem");
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa) {
  struct run *r;
  int cpu_id;

  if (((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // 填充垃圾数据以捕捉悬空引用
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  cpu_id = cpuid();
  acquire(&kmem[cpu_id].lock);
  r->next = kmem[cpu_id].freelist;
  kmem[cpu_id].freelist = r;
  release(&kmem[cpu_id].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void) {
  struct run *r;
  int cpu_id;

  push_off();
  cpu_id = cpuid();
  acquire(&kmem[cpu_id].lock);
  r = kmem[cpu_id].freelist;
  if (r)
    kmem[cpu_id].freelist = r->next;
  release(&kmem[cpu_id].lock);

  if (!r) {
    for (int i = 0; i < NCPU; i++) {
      if (i == cpu_id)
        continue;
      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if (r) {
        kmem[i].freelist = r->next;
        release(&kmem[i].lock);
        break;
      }
      release(&kmem[i].lock);
    }
  }

  pop_off();

  if (r)
    memset((char*)r, 5, PGSIZE); // 填充垃圾数据
  return (void*)r;
}