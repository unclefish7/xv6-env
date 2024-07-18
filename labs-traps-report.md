# labs:traps

## RISC-V Assembly

### 实验目的

本实验旨在帮助理解RISC-V汇编语言，通过分析汇编代码了解函数调用、参数传递及返回值处理的机制。同时，通过几个具体问题加深对RISC-V指令和内存表示的理解。

### 实验步骤

1. **生成汇编代码**
   在 `user` 目录下有一个 `call.c` 文件，运行 `make fs.img` 编译该文件并生成对应的汇编代码 `call.asm`。该文件包含三个函数 `g`、`f` 和 `main`。

2. **查看并分析汇编代码**
   打开 `call.asm`，查找并分析函数 `g`、`f` 和 `main` 的汇编代码，回答以下问题：
   
   1. **函数参数保存在哪些寄存器中？例如，main 调用 printf 时，13 保存在哪个寄存器中？**
      
      在 RISC-V 汇编中，寄存器 `a0` 到 `a7` 用于保存函数的参数。在 `main` 中调用 `printf` 时，值 `13` 保存在寄存器 `a2` 中。

      ```asm
      24: 4635 li a2,13  // 将 13 加载到 a2 中
      ```

   2. **在 main 的汇编代码中，调用函数 f 的代码在哪里？调用 g 的代码在哪里？（提示：编译器可能会内联函数。）**
      
      在汇编代码中，`main` 调用 `f` 和 `g` 的代码被内联了。也就是说，编译器将这两个函数的代码直接插入到 `main` 函数中，而不是进行函数调用。`f` 和 `g` 函数的汇编代码如下：

      ```asm
      0e: 1141 addi sp,sp,-16
      10: e422 sd s0,8(sp)
      12: 0800 addi s0,sp,16
      14: 250d addiw a0,a0,3   // f 和 g 函数都被内联为这一条指令
      16: 6422 ld s0,8(sp)
      18: 0141 addi sp,sp,16
      1a: 8082 ret
      ```

   3. **printf 函数位于什么地址？**
      
      `printf` 函数的地址在运行时确定。以下指令表明 `printf` 位于地址 `0x628`。

      ```asm
      34: 5f8080e7 jalr 1528(ra) # 628 <printf>  // 调用 printf，目标地址是 0x628
      ```

   4. **在 main 中的 jalr 到 printf 之后，ra 寄存器中的值是多少？**
      
      在 `jalr` 到 `printf` 之后，`ra`（返回地址）寄存器中的值将是 `jalr` 指令之后的指令地址，即 `0x38`（`li a0, 0` 的地址）。

      ```asm
      34: 5f8080e7 jalr 1528(ra) # 628 <printf>  // ra 寄存器保存了下一条指令的地址 0x38
      ```

   5. **运行以下代码，输出是什么？**
      
      ```c
      unsigned int i = 0x00646c72;
      printf("H%x Wo%s", 57616, &i);
      ```

      **输出：** `He110 WoWorld`
      
      **解释：**
      
      - `0x00646c72` 是字符串 `rld\0` 的小端表示。ASCII 值为：`r` (0x72), `l` (0x6c), `d` (0x64), `\0` (0x00)。
      - `57616` 的十六进制表示为 `e110`。
      
   6. **如果 RISC-V 是大端序，为了产生相同的输出，你需要将 i 设置为什么值？是否需要将 57616 改为其他值？**
      
      如果 RISC-V 是大端序，你需要将 `i` 设置为 `0x726c6400` 才能产生相同的输出。而 `57616`（`e110` 的十六进制）不需要改变。
   
   7. **在以下代码中，'y=' 后面将打印什么？为什么会发生这种情况？**
   
      ```c
      printf("x=%d y=%d", 3);
      ```
   
      **输出：** `x=3 y=` 后面跟随一个未定义的值。
   
      **解释：** 这是因为格式字符串期望有两个整数参数，但只提供了一个（`3`）。格式字符串中的第二个 `%d` 使 `printf` 从堆栈中读取下一个值，但这个值未被指定，因此导致未定义行为。

### 总结与体会

通过本实验，我深入理解了 RISC-V 汇编语言的函数调用、参数传递和返回值处理机制。理解了编译器如何内联函数以优化性能。还学习了大端和小端表示对数据存储和处理的影响。在实际操作中，通过观察汇编代码和寄存器值，加深了对底层计算机体系结构的理解，为后续的系统编程和调试打下了坚实的基础。

## Backtrace

### 实验目的

本实验旨在实现一个 `backtrace()` 函数，用于在 xv6 操作系统内核中打印函数调用栈。这在调试时非常有用，可以帮助我们理解程序的执行路径，并定位错误发生的位置。通过实现和测试 `backtrace()` 函数，我们将深入理解 RISC-V 汇编、栈帧结构和函数调用的机制。

### 实验步骤

#### 添加函数原型

在 `kernel/defs.h` 中添加 `backtrace()` 函数的原型声明，以便在其他文件中调用。

```c
void backtrace(void);
```

#### 实现 `backtrace()` 函数

在 `kernel/printf.c` 中实现 `backtrace()` 函数，使用帧指针遍历栈帧并打印每个栈帧的返回地址。

```c
#include "riscv.h"
#include "defs.h"

static inline uint64 r_fp() {
  uint64 x;
  asm volatile("mv %0, s0" : "=r" (x));
  return x;
}

void backtrace(void) {
  uint64 fp = r_fp();
  printf("backtrace:\n");
  while (fp != 0) {
    uint64 ra = *(uint64 *)(fp - 8);
    printf(" %p\n", ra);
    fp = *(uint64 *)(fp - 16);
  }
}
```

#### 调用 `backtrace()` 函数

在 `sys_sleep()` 函数中调用 `backtrace()`，确保在执行 `bttest` 时打印调用栈。

```c
#include "defs.h"

uint64 sys_sleep(void) {
  backtrace(); // 添加调用
  ...
}
```

#### 编译并运行测试

编译 xv6 并运行 `bttest`，验证 `backtrace()` 的输出。

```sh
$ make clean && make qemu
$ bttest
```

### 遇到的问题及解决方法

#### 错误输出和中断

- **问题**：在运行 `bttest` 时，出现意外的错误输出和中断。
- **解决方法**：确保在 `backtrace()` 函数中正确处理栈帧指针，并确保指针在有效范围内。使用内核宏 `PGROUNDDOWN(fp)` 和 `PGROUNDUP(fp)` 确保帧指针在合法的栈帧范围内。

  ```c
  void backtrace(void) {
    uint64 fp = r_fp();
    printf("backtrace:\n");
    while (fp != 0 && fp >= PGROUNDDOWN(fp) && fp < PGROUNDUP(fp)) {
      uint64 ra = *(uint64 *)(fp - 8);
      printf(" %p\n", ra);
      fp = *(uint64 *)(fp - 16);
    }
  }
  ```

#### 理解栈帧结构

- **问题**：对栈帧结构和帧指针的理解不够深入，导致实现 `backtrace()` 时出现问题。
- **解决方法**：阅读并理解 RISC-V 的栈帧结构，确保正确访问和遍历栈帧。

#### 调试地址转换

- **问题**：在调试过程中，难以将打印的地址与源代码对应起来。
- **解决方法**：使用 `addr2line` 工具将打印的地址转换为源代码中的文件名和行号。

  ```sh
  addr2line -e kernel/kernel 0x0000000080002de2
  ```

### 总结与体会

通过本次实验，我们深入理解了 RISC-V 汇编、函数调用和栈帧结构。实现 `backtrace()` 函数的过程不仅加强了我们对内核栈管理的理解，还提高了我们在调试和定位错误方面的能力。实验中遇到的问题和解决方法，使我们更加熟悉了内核开发和调试工具的使用，为后续的内核开发奠定了坚实的基础。

## Alarm

### 实验目的

本实验的目的是在 xv6 操作系统中添加一个新功能，使其能够定期提醒正在运行的进程。这对于那些计算密集型进程非常有用，它可以限制进程占用的 CPU 时间，或者允许进程在计算的同时定期执行某些操作。更广泛地说，我们将实现一种用户级别的中断/故障处理机制。

### 实验步骤

#### 1. 添加 `sigalarm` 和 `sigreturn` 系统调用

在 `proc.h` 中为进程结构添加必要的字段：

```c
struct proc {
  ...
  int alarmticks;                 // 间隔时间（以滴答计数）
  int alarmelapsed;               // 已经过的时间滴答数
  void (*alarmhandler)();         // 用户定义的警报处理程序
  int alarmactive;                // 是否正在处理警报
  struct trapframe alarmtrapframe; // 保存被打断时的寄存器状态
  ...
};
```

在 `sysproc.c` 中实现 `sys_sigalarm` 和 `sys_sigreturn` 系统调用：

```c
uint64 sys_sigalarm(void)
{
  int ticks;
  uint64 handler;

  if(argint(0, &ticks) < 0 || argaddr(1, &handler) < 0)
    return -1;

  struct proc *p = myproc();
  p->alarmticks = ticks;
  p->alarmhandler = (void (*)())handler;
  p->alarmelapsed = 0;

  return 0;
}

uint64 sys_sigreturn(void)
{
  struct proc *p = myproc();
  memmove(p->trapframe, &p->alarmtrapframe, sizeof(struct trapframe));
  p->alarmactive = 0;
  return 0;
}
```

#### 2. 修改 `usertrap` 函数

在 `trap.c` 中修改 `usertrap` 函数，以便在时钟中断时检查并调用警报处理程序：

```c
void usertrap(void)
{
  int which_dev = 0;

  struct proc *p = myproc();
  
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call
    ...
  } else if((which_dev = devintr()) != 0){
    if (which_dev == 2) {
      p->alarmelapsed++;
      if (p->alarmticks > 0 && p->alarmelapsed >= p->alarmticks && !p->alarmactive) {
        memmove(&p->alarmtrapframe, p->trapframe, sizeof(struct trapframe));
        p->trapframe->epc = (uint64)p->alarmhandler;
        p->alarmactive = 1;
        p->alarmelapsed = 0;
      }
    }
  } else {
    ...
  }

  if(p->killed)
    exit(-1);

  if(which_dev == 2)
    yield();

  usertrapret();
}
```

#### 3. 初始化进程的警报字段

在 `proc.c` 中的 `allocproc` 函数中初始化这些字段：

```c
static struct proc* allocproc(void)
{
  struct proc *p;
  ...
  p->alarmticks = 0;
  p->alarmhandler = 0;
  p->alarmelapsed = 0;
  p->alarmactive = 0;
  ...
  return p;
}
```

### 遇到的问题及解决方法

1. **警报处理程序没有被调用**：
   - 解决方法：确保 `usertrap` 函数中正确检测到时钟中断，并正确更新 `alarmelapsed` 和检查条件。

2. **处理程序执行后无法恢复**：
   - 解决方法：在 `sys_sigreturn` 中正确恢复被打断的 `trapframe`，并将 `alarmactive` 标志置为 0，确保可以再次触发警报。

3. **多次触发警报处理程序**：
   - 解决方法：添加 `alarmactive` 标志，防止在处理警报时再次进入警报处理程序。

### 总结与体会

通过本实验，深入理解了 xv6 中的中断和陷阱机制，学会了如何利用这些机制实现用户级别的警报处理。该实验不仅增强了对操作系统内部工作原理的理解，还提供了实现用户态中断处理的实际经验。这种机制在处理计算密集型任务和需要定期执行任务的场景中非常有用。通过解决实验过程中遇到的问题，进一步提高了调试和问题解决的能力。