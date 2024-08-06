# Lab: system calls

## System Call Tracing

### 实验目的

通过实现一个 `trace` 系统调用，允许用户跟踪特定进程的系统调用，输出系统调用名称、进程ID和返回值。这将有助于调试和理解操作系统的内部工作。

### 实验步骤

1. **添加 `trace_mask` 字段**：
   在 `proc.h` 文件的 `proc` 结构体中添加一个 `trace_mask` 字段，用于存储每个进程的系统调用跟踪掩码。

   ```c
   struct proc {
       ...
       int trace_mask; // 系统调用跟踪掩码
       ...
   };
   ```

2. **实现 `sys_trace` 系统调用**：
   在 `sysproc.c` 文件中实现 `sys_trace` 函数，接受一个掩码参数，并将其存储在当前进程的 `trace_mask` 字段中。

   ```c
   uint64 sys_trace(void) {
       int mask;
       if (argint(0, &mask) < 0)
           return -1;
       myproc()->trace_mask = mask;
       return 0;
   }
   ```

3. **定义 `SYS_trace` 编号**：
   在 `syscall.h` 文件中定义 `SYS_trace` 的系统调用编号。

   ```c
   #define SYS_trace 22
   ```

4. **添加系统调用包装**：
   在 `usys.pl` 文件中添加 `entry("trace");`，生成用户级的 `trace` 函数包装。

   ```perl
   entry("trace");
   ```

5. **继承 `trace_mask`**：
   在 `proc.c` 文件的 `fork` 函数中，将父进程的 `trace_mask` 复制到子进程中。

   ```c
   int fork(void) {
       ...
       np->trace_mask = p->trace_mask;
       ...
   }
   ```

6. **打印系统调用信息**：
   在 `syscall.c` 文件中添加一个数组存储系统调用的名称，并在 `syscall` 函数中检查 `trace_mask`，如果需要跟踪，打印系统调用信息。

   ```c
   char *syscallnames[] = {
       [SYS_fork]    "fork",
       [SYS_exit]    "exit",
       ...
       [SYS_trace]   "trace",
   };
   
   void syscall(void) {
       ...
       if (p->trace_mask & (1 << num)) {
           printf("%d: syscall %s -> %d\n", p->pid, syscallnames[num], p->trapframe->a0);
       }
       ...
   }
   ```

7. **编写用户程序**：
   编写 `trace.c` 用户程序，通过 `trace` 系统调用设置跟踪掩码，并执行指定命令。

   ```c
   #include "kernel/param.h"
   #include "kernel/types.h"
   #include "kernel/stat.h"
   #include "user/user.h"
   
   int
   main(int argc, char *argv[])
   {
     int i;
     char *nargv[MAXARG];
   
     if(argc < 3 || (argv[1][0] < '0' || argv[1][0] > '9')){
       fprintf(2, "Usage: %s mask command\n", argv[0]);
       exit(1);
     }
   
     if (trace(atoi(argv[1])) < 0) {
       fprintf(2, "%s: trace failed\n", argv[0]);
       exit(1);
     }
     
     for(i = 2; i < argc && i < MAXARG; i++){
       nargv[i-2] = argv[i];
     }
     exec(nargv[0], nargv);
     exit(0);
   }
   ```
   
8. **修改 `Makefile`**：
   在 `Makefile` 中添加 `trace` 程序，以确保在编译时生成可执行文件。

   ```makefile
   UPROGS=\
       _cat\
       _echo\
       _forktest\
       ...
       _trace\
       ...
   ```

### 遇到的问遇到的问题以及解决方法

1. **无法编译 `trace.c`**：
   - **问题**：缺少用户态系统调用存根。
   - **解决方法**：在 `user/user.h` 中添加 `trace` 的原型，并在 `user/usys.pl` 中添加相应的条目，生成系统调用存根。

1. **系统调用未实现**：
   - **问题**：调用 `trace` 时程序崩溃。
   - **解决方法**：在 `kernel/sysproc.c` 中实现 `sys_trace` 函数，并在 `kernel/syscall.c` 中注册。

1. **系统调用返回值不正确**：
   - **问题**：跟踪输出显示系统调用返回值不正确。
   - **解决方法**：确保在 `syscall` 函数中正确获取和打印返回值。

1. **进程间跟踪信息共享**：
   - **问题**：子进程未继承父进程的 `tracemask`。
   - **解决方法**：在 `fork` 函数中复制父进程的 `tracemask` 到子进程。

### 总结与体会

通过实现 `trace` 系统调用，深入理解了操作系统中系统调用的处理机制。这个实验展示了如何通过内核函数处理用户请求，并实现了跟踪和调试系统调用的功能。进一步掌握了进程管理、系统调用处理以及调试技巧。这些知识对理解操作系统的工作原理和提高调试能力非常有帮助。

## sysinfo

### 实验目的

通过实现一个 `sysinfo` 系统调用，收集系统运行信息，包括空闲内存的字节数和当前活动的进程数。这将有助于理解操作系统的资源管理和进程调度机制。

### 实验步骤

1. **定义 `sysinfo` 结构体**：
   在 `kernel/sysinfo.h` 文件中定义 `sysinfo` 结构体，用于存储系统信息。

   ```c
   struct sysinfo {
       uint64 freemem; // 空闲内存字节数
       uint64 nproc;   // 活动进程数
   };
   ```

2. **声明 `sysinfo` 系统调用**：
   在 `user/user.h` 文件中声明 `sysinfo` 系统调用的原型。

   ```c
   struct sysinfo;
   int sysinfo(struct sysinfo *);
   ```

3. **添加系统调用包装**：
   在 `usys.pl` 文件中添加 `entry("sysinfo");`，生成用户级的 `sysinfo` 函数包装。

   ```perl
   entry("sysinfo");
   ```

4. **定义系统调用号**：
   在 `syscall.h` 文件中定义 `SYS_sysinfo` 的系统调用编号。

   ```c
   #define SYS_sysinfo 22
   ```

5. **实现 `sys_sysinfo` 函数**：
   在 `sysproc.c` 文件中实现 `sys_sysinfo` 函数，收集系统信息并将其复制到用户空间。

   ```c
   #include "types.h"
   #include "riscv.h"
   #include "defs.h"
   #include "param.h"
   #include "memlayout.h"
   #include "proc.h"
   #include "sysinfo.h"
   #include "spinlock.h"
   
   extern uint64 kfreemem(void); // 获取空闲内存函数声明
   extern int getnproc(void);    // 获取活动进程数函数声明
   
   // sysinfo 系统调用的实现
   int sys_sysinfo(void) {
       struct sysinfo info;
       uint64 addr;
       struct proc *p = myproc();
   
       // 获取系统调用的第一个参数，即用户空间的地址
       if (argaddr(0, &addr) < 0) {
           return -1;
       }
   
       // 获取系统信息
       info.freemem = kfreemem();
       info.nproc = getnproc();
   
       // 将 sysinfo 结构体复制到用户空间
       if (copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0) {
           return -1;
       }
   
       return 0;
   }
   ```

6. **计算空闲内存**：
   在 `kalloc.c` 文件中实现 `kfreemem` 函数，用于计算系统中的空闲内存。

   ```c
   uint64 kfreemem(void) {
       struct run *r;
       uint64 free_mem = 0;
       acquire(&kmem.lock); // 获取锁
       for(r = kmem.freelist; r; r = r->next) // 遍历空闲内存链表
           free_mem += PGSIZE; // 累加每个空闲内存块的大小
       release(&kmem.lock); // 释放锁
       return free_mem; // 返回空闲内存总字节数
   }
   ```

7. **计算活动进程数**：
   在 `proc.c` 文件中实现 `getnproc` 函数，用于计算当前活动的进程数。

   ```c
   int getnproc(void) {
       struct proc *p;
       int count = 0;
       for(p = proc; p < &proc[NPROC]; p++) { // 遍历进程表
           if(p->state != UNUSED) // 如果进程不是 UNUSED 状态
               count++; // 计数加1
       }
       return count; // 返回活动进程数
   }
   ```

8. **在 `syscall.c` 中注册系统调用**：
   在 `syscall.c` 文件中添加对 `sysinfo` 系统调用的处理。

   ```c
   extern int sys_sysinfo(void); // 声明 sys_sysinfo 函数
   
   static int (*syscalls[])(void) = {
       [SYS_fork]    sys_fork,
       [SYS_exit]    sys_exit,
       ...
       [SYS_sysinfo] sys_sysinfo, // 注册 sysinfo 系统调用
   };
   ```

### 遇到的问题及解决方法

1. **编译错误：缺少系统调用定义**：
   - **问题**：编译过程中出现错误，提示找不到 `sysinfo` 的定义。
   - **解决方法**：检查 `user/user.h` 和 `kernel/syscall.h` 中是否正确添加了 `sysinfo` 的声明和系统调用编号，确保在 `usys.pl` 中添加了相应的条目以生成系统调用存根。
2. **数据复制失败：用户空间地址无效**：
   - **问题**：`sysinfo` 系统调用无法将数据复制到用户空间，导致 `copyout` 函数返回错误。
   - **解决方法**：确认在 `sys_sysinfo` 函数中正确获取了用户空间的地址，并检查 `copyout` 函数的参数是否正确。需要确保传递的地址在用户空间有效且可写。

### 总结与体会

通过实现 `sysinfo` 系统调用，深入理解了操作系统中系统资源管理和进程调度的机制。这个实验展示了如何通过内核函数处理用户请求，并实现了系统信息的收集功能。进一步掌握了进程管理、内存管理以及系统调用处理等知识。
