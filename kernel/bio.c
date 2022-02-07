// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"



#define NBUCKETS 13//把一个大链表拆成13个小链表


struct {
  struct spinlock lock[NBUCKETS];//每个小链表需要一个锁保证一致性
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  //struct buf head;
  struct buf head[NBUCKETS]; //每个小链表的表头
} bcache;

void
binit(void)
{
  struct buf *b;

  for(int i=0;i<NBUCKETS;i++)
  {
    //初始化所有锁
    initlock(&bcache.lock[i], "bcache"); 
    //初始化所有链表表头
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }

  //把所有块插到0号链表里
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){   
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.

//按设备号和块号查找块
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int id=blockno%NBUCKETS;

  acquire(&bcache.lock[id]);

  // Is the block already cached?
  //已经缓存
  for(b = bcache.head[id].next; b != &bcache.head[id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  //未缓存
  //在当前链表里找到一个未使用块，缓存在这里
  for(b = bcache.head[id].prev; b != &bcache.head[id]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  //在当前链表里未找到未使用块
  //去其他链表里偷
  for(uint i=1;i<NBUCKETS;i++)
  {
    uint steal_id=(id+i)%NBUCKETS;
    acquire(&bcache.lock[steal_id]);
    for(b = bcache.head[steal_id].prev; b != &bcache.head[steal_id]; b = b->prev)
    { 
      if(b->refcnt == 0) 
      {
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1; 

          //从原来链表剥离，查找自己链表的head.next处
          b->next->prev = b->prev;
          b->prev->next = b->next;              
          b->next = bcache.head[id].next;
          b->prev = &bcache.head[id];
          bcache.head[id].next->prev = b;
          bcache.head[id].next = b;  

          release(&bcache.lock[id]);        
          release(&bcache.lock[steal_id]);
          acquiresleep(&b->lock);
          return b;
      } 
    }
    release(&bcache.lock[steal_id]);
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  uint id=(b->blockno)%NBUCKETS;
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock[id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head[id].next;
    b->prev = &bcache.head[id];
    bcache.head[id].next->prev = b;
    bcache.head[id].next = b;
  } 
  release(&bcache.lock[id]);
}

void
bpin(struct buf *b) {
  uint id=(b->blockno)%NBUCKETS;
  acquire(&bcache.lock[id]);
  b->refcnt++;
  release(&bcache.lock[id]);
}

void
bunpin(struct buf *b) {
  uint id=(b->blockno)%NBUCKETS;
  acquire(&bcache.lock[id]);
  b->refcnt--;
  release(&bcache.lock[id]);
}