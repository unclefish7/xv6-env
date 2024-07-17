# Lab: page tables

## Speed up system calls 

### 实验目的

通过在用户空间和内核之间共享数据，优化 `getpid()` 系统调用的性能，从而减少系统调用的开销。

### 实验步骤

1. **定义数据结构**：
   在 `memlayout.h` 中定义 `usyscall` 结构和 `USYSCALL` 地址。

   ```c
   struct usyscall {
     int pid;  // Process ID
   };
   
   #define USYSCALL (TRAMPOLINE - 2 * PGSIZE)
   ```

2. **修改 `proc.h` 文件**：
   在 `proc` 结构体中添加 `usyscall` 指针，用于存储进程的 `USYSCALL` 页。

   ```c
   struct proc {
     ...
     struct usyscall *usyscall;  // 用于存储进程的USYSCALL页
     ...
   };
   ```

3. **分配和初始化 `usyscall` 页**：
   在 `allocproc()` 函数中分配 `usyscall` 页并初始化其内容。

   ```c
   // Allocate a page for the USYSCALL structure.
     p->usyscall = kalloc();
     if(p->usyscall == 0){
       freeproc(p);
       release(&p->lock);
       return 0;
     }
     memset(p->usyscall, 0, PGSIZE);  // 确保页面初始化为0
     p->usyscall->pid = p->pid;
   ```
   
4. **映射 `usyscall` 页**：
   在 `proc_pagetable()` 函数中将 `usyscall` 页映射到用户空间。

   ```c
   
   // Map the USYSCALL page to the user space.
   if(mappages(pagetable, USYSCALL, PGSIZE, (uint64)p->usyscall, PTE_U | PTE_R) < 0){
       uvmunmap(pagetable, TRAMPOLINE, 1, 0);
       uvmunmap(pagetable, TRAPFRAME, 1, 0);
       uvmfree(pagetable, 0);
       return 0;
   }
   ```

5. **释放 `usyscall` 页**：
   在 `freeproc()` 函数中释放 `usyscall` 页。

   ```c
   if(p->usyscall)
       kfree(p->usyscall);
   p->usyscall = 0;
   ```
   
6. **释放页表**：
   在 `proc_freepagetable()` 函数中添加解除映射 `usyscall` 页的逻辑。

   ```c
   void
   proc_freepagetable(pagetable_t pagetable, uint64 sz)
   {
     uvmunmap(pagetable, USYSCALL, 1, 0);
     uvmunmap(pagetable, TRAMPOLINE, 1, 0);
     uvmunmap(pagetable, TRAPFRAME, 1, 0);
     uvmfree(pagetable, sz);
   }
   ```

### 遇到的问题以及解决方法

1. **内核空间和用户空间的地址映射**：
   - **问题**：确保内核分配的 `usyscall` 页正确映射到用户空间。
   - **解决方法**：使用 `mappages` 函数在 `proc_pagetable()` 中进行映射，并设置适当的权限位 `PTE_U | PTE_R`。

2. **进程创建和 `fork` 处理**：
   - **问题**：子进程的usyscall和父进程之间似乎没有正确继承
   - **解决方法**：在 `fork` 中不应该继承usyscall相关信息。

3. **调试信息的添加和验证**：
   - **问题**：ugetpid和真正pid不同
   - **解决方法**：在关键步骤添加调试信息，如 `allocproc` 和 `proc_pagetable` 中，打印 `pid` 以验证映射和初始化是否成功。

### 总结与体会

通过本实验，掌握了如何在 xv6 中优化系统调用的性能，深入理解了用户空间和内核空间的数据共享机制。通过实现 `usyscall` 页的映射和初始化，成功减少了 `getpid()` 系统调用的开销。实验过程中，通过调试信息的添加和验证，解决了地址映射和进程管理中的问题，增强了对 xv6 内核的理解和操作系统开发的技能。

## Print a Page Table

### 实验目的

实现一个函数 `vmprint`，用于打印 RISC-V 的页表结构，帮助可视化和调试页表内容。

### 实验步骤

1. **定义 `vmprint` 和 `vmprint_inner` 函数**：
   在 `kernel/vm.c` 中定义 `vmprint` 和 `vmprint_inner` 函数。

   ```c
   void vmprint(pagetable_t pagetable) {
       printf("page table %p\n", pagetable);
       vmprint_inner(pagetable, 0, 0);
   }
   
   void vmprint_inner(pagetable_t pagetable, int level, int index) {
       for (int i = 0; i < 512; i++) {
           pte_t pte = pagetable[i];
           if (pte & PTE_V) {
               uint64 child = PTE2PA(pte);
               for (int j = 0; j < level; j++)
                   printf(" ..");
               printf("%d: pte %p pa %p\n", i, pte, child);
               if ((pte & (PTE_R | PTE_W | PTE_X)) == 0) {
                   pagetable_t next_level = (pagetable_t)child;
                   vmprint_inner(next_level, level + 1, i);
               }
           }
       }
   }
   ```

2. **在 `defs.h` 中添加函数声明**：
   确保其他文件可以调用 `vmprint` 和 `vmprint_inner` 函数。

   ```c
   void vmprint(pagetable_t pagetable);
   void vmprint_inner(pagetable_t pagetable, int level, int index);
   ```

3. **在 `exec.c` 中调用 `vmprint` 函数**：
   在执行 `exec` 时打印第一个进程的页表。

   ```c
   if (p->pid == 1) {
       vmprint(p->pagetable);
   }
   ```

### 遇到的问题及解决方法

1. **隐式声明错误**：
   - **问题**：在调用 `vmprint_inner` 时出现隐式声明错误。
   - **解决方法**：在 `kernel/vm.c` 中定义 `vmprint_inner` 函数之前，添加函数声明。

     ```c
     void vmprint_inner(pagetable_t pagetable, int level, int index);
     ```

2. **pte_t 和 child 的理解**：
   - **问题**：理解 `pte_t` 和 `child` 在页表打印中的作用。
   - **解决方法**：`pte_t` 是页表项，包含页的物理地址和权限信息。`child` 是从 `pte` 中提取的物理地址，用于递归处理下一级页表或物理页。

3. **递归打印页表**：
   - **问题**：如何递归地打印多级页表结构。
   - **解决方法**：在 `vmprint_inner` 中，通过检查 PTE 的权限位，判断是否需要递归处理下一级页表。

### 总结与体会

通过实现 `vmprint` 函数，增强了对 RISC-V 页表结构的理解。页表是一种多级结构，每一级都有 512 个条目，每个条目可以指向下一级页表或实际物理页。通过递归遍历和打印页表，我们可以清晰地看到虚拟地址到物理地址的映射关系，这对于调试和分析内存管理问题非常有帮助。

在实现过程中，遇到了一些技术细节问题，如隐式声明错误、页表项解析等。这些问题的解决过程，不仅加深了对 xv6 操作系统内部机制的理解，也提高了问题分析和解决能力。

总体来说，这个实验帮助我们掌握了如何通过编程手段查看和调试操作系统中的复杂数据结构，进一步提升了对操作系统原理和实现的认识。

## Detecting which pages have been accessed

### 实验目的

实现一个新的 `pgaccess` 系统调用，用于检测哪些页面已经被访问（读或写）。这一功能可以帮助垃圾收集器等需要了解页面访问情况的自动内存管理机制。

### 实验步骤

1. 定义 PTE_A 位

在 `kernel/riscv.h` 文件中定义 PTE_A 访问位：
```c
#define PTE_A (1L << 6)
```

2. 实现 `sys_pgaccess` 函数

在 `kernel/sysproc.c` 文件中实现 `sys_pgaccess` 函数：
```c
int sys_pgaccess(void) {
    uint64 start_va;
    int num_pages;
    uint64 user_mask_addr;

    // 解析系统调用参数
    if (argaddr(0, &start_va) < 0 || argint(1, &num_pages) < 0 || argaddr(2, &user_mask_addr) < 0)
        return -1;

    // 设定一个扫描页数的上限
    if (num_pages > 64)
        return -1;

    struct proc *p = myproc();
    uint64 mask = 0;

    for (int i = 0; i < num_pages; i++) {
        pte_t *pte = walk(p->pagetable, start_va + i * PGSIZE, 0);
        if (pte && (*pte & PTE_V) && (*pte & PTE_A)) {
            mask |= (1UL << i);
            *pte &= ~PTE_A; // 清除访问位
        }
    }

    // 将结果复制到用户空间
    if (copyout(p->pagetable, user_mask_addr, (char *)&mask, sizeof(mask)) < 0)
        return -1;

    return 0;
}
```

### 遇到的问题与解决方法

1. **walk 函数未找到**：
   - **问题**：在实现 `sys_pgaccess` 时，编译器报告 `walk` 函数未定义。
   - **解决方法**：显式地声明 `walk` 函数。

2. **参数解析错误**：
   - **问题**：在解析系统调用参数时，可能会因为参数类型不匹配导致解析失败。
   - **解决方法**：使用正确的解析函数（如 `argaddr` 和 `argint`）并检查返回值。

3. **复制结果失败**：
   - **问题**：将结果复制到用户空间时可能会失败。
   - **解决方法**：检查 `copyout` 函数的返回值，确保复制操作成功。

### 总结与体会

通过实现 `pgaccess` 系统调用，我深入理解了操作系统如何管理页面访问权限以及如何通过页表来跟踪页面的访问情况。

1. **页表管理**：理解了页表的结构和作用，学习了如何通过页表项来判断页面的访问情况。
2. **系统调用**：掌握了如何实现新的系统调用，包括参数解析、内核空间与用户空间的数据交互等。
3. **调试技巧**：学会了通过打印调试信息来排查问题，并借助 xv6 提供的辅助函数（如 `vmprint`）进行调试。
