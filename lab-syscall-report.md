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