# Lab: locks

## Memory Allocator

#### 实验目的

通过重新设计内存分配器，减少在多核系统中由于锁竞争导致的并行性能问题。目标是实现每个 CPU 一个独立的空闲列表，并在需要时从其他 CPU 的空闲列表中窃取内存，从而减少锁竞争，提高系统并行性。

### 实验步骤

1. **定义每个 CPU 独立的空闲列表和锁**：
   修改 `kmem` 结构体，添加每个 CPU 独立的空闲列表和锁。

   ```c
   struct {
     struct spinlock lock;
     struct run *freelist;
   } kmem[NCPU];
   ```
2. **初始化每个 CPU 的锁**：
   在 `kinit` 函数中，初始化每个 CPU 的锁。

   ```c
   void kinit() {
     for (int i = 0; i < NCPU; i++) {
       initlock(&kmem[i].lock, "kmem");
     }
     freerange(end, (void*)PHYSTOP);
   }
   ```
3. **实现每个 CPU 独立的内存分配和释放**：
   修改 `kalloc` 和 `kfree` 函数，使用每个 CPU 的空闲列表和锁。

   ```c
   void kfree(void *pa) {
     struct run *r;
     int cpu = cpuid();
     r = (struct run*)pa;
     acquire(&kmem[cpu].lock);
     r->next = kmem[cpu].freelist;
     kmem[cpu].freelist = r;
     release(&kmem[cpu].lock);
   }

   void* kalloc(void) {
     int cpu = cpuid();
     struct run *r;
     acquire(&kmem[cpu].lock);
     r = kmem[cpu].freelist;
     if (r)
       kmem[cpu].freelist = r->next;
     release(&kmem[cpu].lock);
     return r;
   }
   ```
4. **实现内存窃取机制**：
   在 `kalloc` 函数中，当当前 CPU 的空闲列表为空时，从其他 CPU 的空闲列表中窃取内存。

   ```c
   void* kalloc(void) {
     int cpu = cpuid();
     struct run *r;

     for (int i = 0; i < NCPU; i++) {
       acquire(&kmem[(cpu + i) % NCPU].lock);
       r = kmem[(cpu + i) % NCPU].freelist;
       if (r) {
         kmem[(cpu + i) % NCPU].freelist = r->next;
         release(&kmem[(cpu + i) % NCPU].lock);
         return r;
       }
       release(&kmem[(cpu + i) % NCPU].lock);
     }
     return 0;
   }
   ```
5. **测试并验证修改**：
   运行 `kalloctest` 测试程序，确保减少了锁竞争并且所有测试通过。

### 遇到的问题及解决方法

- **锁竞争问题**：通过实现每个 CPU 独立的空闲列表和锁，并在需要时从其他 CPU 窃取内存，成功减少了锁竞争。
- **内存窃取机制的实现**：确保内存窃取机制在多个 CPU 间平衡负载，避免一个 CPU 的空闲列表被耗尽。

### 总结与体会

通过重新设计内存分配器，显著减少了多核系统中由于锁竞争导致的性能瓶颈。实验展示了在共享资源上采用更细粒度的锁策略对性能提升的重要性。这次实验增强了对多核系统中并行编程和锁机制的理解，有助于在实际开发中设计高效的并行算法。


## Buffer Cache

### 实验目的

本实验的目的是优化 xv6 操作系统中的缓存区（Buffer Cache）机制，通过引入多桶（bucket）结构和每桶一个锁的机制，减少多核系统中访问缓存区时的锁争用，提高文件系统的并发性能。

### 实验步骤

#### 1. 数据结构定义

在 `buf.h` 文件中修改 `struct buf`，添加 `lastuse_tick` 字段，用于记录每个缓存块的上次使用时间。此外，定义一个多桶结构 `bucket` 和全局缓存区结构 `bcache`。

```c
#define NBUCKET 13

struct {
  struct spinlock lock[NBUCKET];
  struct buf buf[NBUF];
  struct spinlock bcache_lock; // bcache锁
  struct buf head[NBUCKET];    // 每个桶的链表头
} bcache;
```

#### 2. 初始化缓存区

在 `binit` 函数中，初始化每个桶的锁和链表头，并将缓存块放入第一个桶的链表中。

```c
void binit(void) {
  struct buf *b;
  initlock(&bcache.bcache_lock, "bcache_lock");
  for(int i = 0; i < NBUCKET; ++i){
    initlock(&bcache.lock[i], "bcache_bucket");
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }
}
```

#### 3. 缓存块获取和释放

在 `bget` 函数中，通过哈希计算确定块所在的桶，并在桶中查找或分配缓存块。在 `brelse` 函数中释放缓存块并记录最后使用的时间。

```c
static struct buf* bget(uint dev, uint blockno) {
  struct buf *b;
  int index = blockno % NBUCKET;
  acquire(&bcache.lock[index]);
  for(b = bcache.head[index].next; b != &bcache.head[index]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.lock[index]);

  acquire(&bcache.bcache_lock);
  acquire(&bcache.lock[index]);
  for(b = bcache.head[index].next; b != &bcache.head[index]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[index]);
      release(&bcache.bcache_lock); 
      acquiresleep(&b->lock);
      return b;
    }
  }

  struct buf *lru_block = 0;
  int min_tick = 0;
  for (b = bcache.head[index].next; b != &bcache.head[index]; b = b->next) {
    if (b->refcnt == 0 && (lru_block == 0 || b->lastuse_tick < min_tick)) {
      min_tick = b->lastuse_tick;
      lru_block = b;
    }
  }
  if(lru_block != 0){
    lru_block->dev = dev;
    lru_block->blockno = blockno;
    lru_block->refcnt++;
    lru_block->valid = 0;
    release(&bcache.lock[index]);
    release(&bcache.bcache_lock);
    acquiresleep(&lru_block->lock);
    return lru_block;
  }

  for (int other_index = (index + 1) % NBUCKET; other_index != index; other_index = (other_index + 1) % NBUCKET) {
    acquire(&bcache.lock[other_index]);
    for (b = bcache.head[other_index].next; b != &bcache.head[other_index]; b = b->next) {
      if (b->refcnt == 0 && (lru_block == 0 || b->lastuse_tick < min_tick)) {
        min_tick = b->lastuse_tick;
        lru_block = b;
      }
    }
    if(lru_block) {
      lru_block->dev = dev;
      lru_block->refcnt++;
      lru_block->valid = 0;
      lru_block->blockno = blockno;
      lru_block->next->prev = lru_block->prev;
      lru_block->prev->next = lru_block->next;
      release(&bcache.lock[other_index]);
      lru_block->next = bcache.head[index].next;
      lru_block->prev = &bcache.head[index];
      bcache.head[index].next->prev = lru_block;
      bcache.head[index].next = lru_block;
      release(&bcache.lock[index]);
      release(&bcache.bcache_lock);
      acquiresleep(&lru_block->lock);
      return lru_block;
    }
    release(&bcache.lock[other_index]); 
  }
  release(&bcache.lock[index]);
  release(&bcache.bcache_lock);
  panic("bget: no buffers");
}

void brelse(struct buf *b) {
  if(!holdingsleep(&b->lock))
    panic("brelse");
  releasesleep(&b->lock);
  acquire(&bcache.lock[b->blockno % NBUCKET]);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->lastuse_tick = ticks;
  }
  release(&bcache.lock[b->blockno % NBUCKET]);
}
```

#### 4. 缓存块的固定和解固定

通过 `bpin` 和 `bunpin` 函数增加或减少引用计数来固定或解固定缓存块。

```c
void bpin(struct buf *b) {
  acquire(&bcache.lock[b->blockno % NBUCKET]);
  b->refcnt++;
  release(&bcache.lock[b->blockno % NBUCKET]);
}

void bunpin(struct buf *b) {
  acquire(&bcache.lock[b->blockno % NBUCKET]);
  b->refcnt--;
  release(&bcache.lock[b->blockno % NBUCKET]);
}
```

### 遇到的问题及解决方法

#### 问题 1：初始化缓冲区时的分配问题

在初始代码中，所有缓冲区都被放入第一个桶中，导致其他桶没有初始缓冲区。

**解决方法：** 在 `binit` 函数中循环初始化每个桶的缓冲区。

#### 问题 2：缓存块的并发访问冲突

在并发访问缓存块时，多个进程可能会同时访问或修改同一个桶，导致冲突。

**解决方法：** 引入全局锁 `bcache_lock` 和每个桶的锁 `bcache.lock[index]`，确保在进行重要操作时进行加锁和解锁。

### 总结与体会

通过本次实验，我们优化了 xv6 操作系统的缓存区机制，引入多桶结构和每桶一个锁的机制，有效减少了多核系统中访问缓存区时的锁争用，提升了文件系统的并发性能。在实际操作过程中，加深了对操作系统文件系统和锁机制的理解，并学会了如何通过改进数据结构和锁机制来优化系统性能。这些经验对以后处理类似问题具有重要的指导意义。
