# lab:copy on write

## 实验报告

### 实验目的

本次实验的目标是实现基于写时复制（Copy-On-Write, COW）的 `fork` 系统调用。COW 是一种内存管理优化技术，允许多个进程共享相同的物理内存页，直到一个进程尝试写入该页时，才将该页复制。通过实现该技术，我们可以减少内存的使用，提高系统的效率。

### 实验步骤

#### 1. 添加内存引用计数

在 `kalloc.c` 中添加了一个引用计数数组 `ref_count`，并在 `kalloc` 和 `kfree` 函数中管理内存页的引用计数。

```c
int ref_count[MAX_PHYS_PAGES];  // 引用计数数组

void kfree(void *pa) {
  struct run *r;
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  uint64 pa_index = ((uint64)pa) >> PGSHIFT;
  acquire(&kmem.lock);
  if (--ref_count[pa_index] > 0) {
    release(&kmem.lock);
    return;
  }
  release(&kmem.lock);

  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

void *kalloc(void) {
  struct run *r;
  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    uint64 pa_index = ((uint64)r) >> PGSHIFT;
    ref_count[pa_index] = 1;  // 初始化引用计数为1
  } else {
    printf("kalloc: failed to allocate page\n");
  }

  return (void*)r;
}

void kref_inc(uint64 pa) {
  uint64 pa_index = pa >> PGSHIFT;
  acquire(&kmem.lock);
  if (pa_index < MAX_PHYS_PAGES) {
    ref_count[pa_index]++;
  } else {
    panic("kref_inc: pa_index out of bounds");
  }
  release(&kmem.lock);
}
```

#### 2. 修改 `uvmcopy` 函数

在 `uvmcopy` 函数中，为每个写权限的页表项设置 `PTE_COW` 标志，并将其写权限清除。

```c
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz) {
  pte_t *pte;
  uint64 pa, i;
  uint flags;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    if (*pte & PTE_W) {
      *pte &= ~PTE_W;
      *pte |= PTE_COW;
    }
    pa = PTE2PA(*pte);
    if(pa >= PHYSTOP)
      panic("uvmcopy: pa out of bounds");

    flags = PTE_FLAGS(*pte);
    kref_inc(pa);

    if(mappages(new, i, PGSIZE, pa, flags) != 0)
      goto err;
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}
```

#### 3. 处理 COW 页错误

在 `handle_cow_fault` 函数中处理 COW 页错误，当写入一个 COW 页时，分配一个新的物理页并复制数据。

```c
int handle_cow_fault(pagetable_t pagetable, uint64 va) {
  pte_t *pte;
  uint64 pa;
  char *mem;
  uint flags;

  if ((pte = walk(pagetable, va, 0)) == 0){
    printf("handle_cow_fault: walk error\n");
    return -1;
  }
  if ((*pte & PTE_V) == 0) {
    printf("handle_cow_fault: PTE_V error\n");
    return -1;
  }

  if ((*pte & PTE_U) == 0) {
    printf("handle_cow_fault: PTE_U error\n");
    return -1;
  }
  if ((*pte & PTE_W) != 0) {
    printf("handle_cow_fault: PTE_W error\n");
    return -1;
  }
  if ((*pte & PTE_COW) == 0) {
    printf("handle_cow_fault: not a COW page\n");
    return -1;
  }

  pa = PTE2PA(*pte);
  if ((mem = kalloc()) == 0){
    printf("handle_cow_fault: kalloc error\n");
    return -1;
  }
  memmove(mem, (char*)pa, PGSIZE);
  flags = PTE_FLAGS(*pte);
  flags |= PTE_W;
  *pte = PA2PTE(mem) | flags;
  *pte &= ~PTE_COW;
  kfree((void*)pa);
  sfence_vma();

  return 0;
}
```

#### 4. 修改 `usertrap` 函数

在 `usertrap` 函数中处理页错误，检查是否是 COW 页并调用 `handle_cow_fault` 处理。

```c
void usertrap(void) {
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  w_stvec((uint64)kernelvec);
  struct proc *p = myproc();

  p->trapframe->epc = r_sepc();

  if(r_scause() == 8){
    if(p->killed)
      exit(-1);
    p->trapframe->epc += 4;
    intr_on();
    syscall();
  } else if((which_dev = devintr()) != 0){
  } else if(r_scause() == 15){
    uint64 va = r_stval();
    if (va >= p->sz)
      p->killed = 1;
    pte_t *pte = walk(p->pagetable, va, 0);
    if (pte && (*pte & PTE_COW)) {
      if(handle_cow_fault(p->pagetable, va) != 0) {
        p->killed = 1;
      }
    }
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  if(which_dev == 2)
    yield();

  usertrapret();
}
```

### 遇到的问题及解决方法

#### 问题1：内存分配失败

在 `usertests` 测试中，出现了内存分配失败的问题。主要原因可能是内存释放不正确或内存泄漏。

解决方法：检查 `kfree` 函数的实现，确保引用计数正确减少，并检查处理 COW 页错误时是否正确减少了引用计数。

#### 问题2：测试输出不一致

在 `cowtest` 测试中，输出与预期不一致。

解决方法：在 `handle_cow_fault` 函数中添加调试信息，检查每个步骤的执行情况，确保每个步骤正确完成。

### 总结与体会

通过本次实验，我深入理解了写时复制技术的实现原理及其在操作系统中的应用。通过修改 `kalloc`、`kfree`、`uvmcopy` 以及 `handle_cow_fault` 等函数，实现了对内存页的共享与复制，达到了内存优化的目的。在解决问题的过程中，我学会了如何通过调试信息逐步排查问题，并改进代码的稳定性与可靠性。

尽管在 `usertests` 测试中仍存在内存分配失败的问题，但通过本次实验，我对操作系统内存管理有了更深的理解，并为后续的优化工作奠定了基础。