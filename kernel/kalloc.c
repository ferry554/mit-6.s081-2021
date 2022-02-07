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

struct kmem{
  struct spinlock lock;
  struct run *freelist;
};

struct kmem kmems[NCPU];

void
kinit()
{
  for(int i=0;i<NCPU;i++)
  {
    initlock(&(kmems[i].lock), "kmem");  
  }
  
  freerange(end, (void*)PHYSTOP);
}



void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)

//释放内存，在当前cpu的freelist表头插入一个节点。
void
kfree(void *pa)
{
  struct run *r;

  

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  int cpu_id;
  push_off();
  cpu_id=cpuid();
  
  acquire(&(kmems[cpu_id].lock));
  r->next = kmems[cpu_id].freelist;
  kmems[cpu_id].freelist = r;
  release(&(kmems[cpu_id].lock));
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.

//分配内存，首先从当前cpu的freelist表头删除一个节点作为新分配的内存
//如果当前cpu的freelist已经为空了，就去其他cpu的freelist中窃取内存。
//如果所有cpu的freelist都已经为空了，就返回0.
void *
kalloc(void)
{
  struct run *r;

  int cpu_id;
  push_off();
  cpu_id=cpuid();
  
  
  acquire(&(kmems[cpu_id].lock));
  r = kmems[cpu_id].freelist;
  if(r)
  {
    //当前cpu的freelist不为空
    kmems[cpu_id].freelist = r->next;
  }
  else
  {
    //当前cpu的freelist为空
    //遍历其他所有cpu的freelist
    //int steal_num=32;       
    for(int steal_id=0;steal_id<NCPU;steal_id++)
    {
      if(steal_id==cpu_id)
      {
        continue;
      }     
      acquire(&(kmems[steal_id].lock));
      r=kmems[steal_id].freelist;                 
      if(r)
      {
        //找到一个其他cpu的freelist不为空，从其表头拿1个节点    
        kmems[steal_id].freelist = r->next; 
        release(&(kmems[steal_id].lock));
        break;
      }
      release(&(kmems[steal_id].lock));
    }         
  }
  release(&(kmems[cpu_id].lock));
  pop_off(); 

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}