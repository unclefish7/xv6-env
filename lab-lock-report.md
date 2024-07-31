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

