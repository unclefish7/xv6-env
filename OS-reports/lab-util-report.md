# Lab: Xv6 and Unix utilities相关实验报告

## 1. Boot xv6

### 实验目的

在Ubuntu环境下运行xv6。并掌握xv6的基础使用方法

### 实验步骤

1. 在克隆下来的xv6-labs-2021仓库中，执行：

   ```cmd
   $ git checkout util
   ```

   这样就切换到了专门用于该实验的环境。

1. 尝试构建并运行xv6：

   ```cmd
   $ make qemu
   riscv64-unknown-elf-gcc    -c -o kernel/entry.o kernel/entry.S
   ...
   
   xv6 kernel is booting
   
   hart 2 starting
   hart 1 starting
   init: starting sh
   $ 
   ```

1. 尝试使用`ls`命令：

1. 使用Ctrl-a 以退出qemu

### 遇到的问题以及解决方法

- `make qemu`命令必要在`/xv6-labs-2021`这个根目录下运行，否则会报错。

### 总结与体会

该实验主要目的除了熟悉xv6的基本操作以外，也是为了测试xv6环境搭建是否正确。并且这也是第一次在linux环境下进行项目的搭建，linux相关的知识也是学习了不少（比如：linux目录结构，用户创建与切换，使用命令行进行git clone等操作）

## 2. sleep

### 实验目的

在xv6下构建一个sleep程序，使得整个系统能够停止特定的一段时间（tick数由用户指定）。

### 实验步骤

1. 源代码放在`user/sleep.c`下。

1. 编写源代码：

   ```c
   #include "kernel/types.h"
   #include "user/user.h"
   
   int main(int argc, char *argv[]) {
       if (argc != 2) { //传入的参数个数必须是两个
           printf("Usage: sleep <ticks>\n");
           exit(1);
       }
       int ticks = atoi(argv[1]); //把字符串转化为整数
       sleep(ticks);//让系统陷入睡眠
       exit(0);
   }
   
   ```

   > main函数中的argc代表传入参数的个数，*argv[]内部是每个被传入的参数

1. 在`Makefile`文件中添加sleep函数

1. 在xv6环境中运行sleep，查看是否符合预期。

1. 运行测试程序`$ make GRADEFLAGS=sleep grade`，查看运行结果是否正确

### 遇到的问题以及解决方法

- 第一次尝试编写xv6程序，不知道如何接收命令行的参数：知道argc，argv的含义之后就知道怎么编写程序了。

### 总结与体会

该实验有助于我理解xv6环境下程序是如何编写，编译，运行的。同时加深我对C语言一些特性的认知。

## 3. pingpong

### 实验目的

实现两个进程通过一对管道传输一个字节的数据：父进程传输一字节给子进程，子进程回复："<pid>: received ping"，再传输一字节给父进程，父进程回复："<pid>: received pong"。

### 实验步骤

1. 创建两个管道与字节缓冲区

   ```c
   int p1[2], p2[2];
       char buf[1];
       
       // 创建两个管道
       pipe(p1);
       pipe(p2);
   ```

1. 创建一个进程，并根据进程号区分父子进程，以进行不同的读写操作

   ```C
   int pid = fork();
       
       if (pid == 0) { // 子进程
           close(p1[1]); // 关闭不需要的写端
           close(p2[0]); // 关闭不需要的读端
           read(p1[0], buf, 1); // 从管道 p1 读取数据
           printf("%d: received ping\n", getpid());
           write(p2[1], " ", 1); // 写数据到管道 p2
           close(p1[0]);
           close(p2[1]);
           exit(0);
       } else { // 父进程
           close(p1[0]); // 关闭不需要的读端
           close(p2[1]); // 关闭不需要的写端
           write(p1[1], " ", 1); // 写数据到管道 p1
           read(p2[0], buf, 1); // 从管道 p2 读取数据
           printf("%d: received pong\n", getpid());
           close(p1[1]);
           close(p2[0]);
           wait(0); // 等待子进程结束
       }
   ```

### 遇到的问题以及解决方法

1. 不知道进程的机制

   > 使用`fork()`函数以创建一个子进程（相当于父进程的副本），并且返回一个pid，如果进程号为0则代表这是刚刚被创建的子进程，如果进程号不为0则代表这是父进程。

1. 不知道如何进行管道通信

   > 使用`pipe()`函数创建一个管道，并且返回一个文件描述符`p[2]`，其中，`p[0]`是读端，`p[1]`是写端。

### 总结与体会

该实验主要学习了xv6中进程协作与管道通信的机制。了解管道通信的文件描述符，进程号在协作时发挥的作用等。

## 4. primes

### 实验目的

实现一个程序，通过创建一系列进程和管道，生成从 2 到 35 的素数。父进程生成数字并通过管道传递给子进程，子进程过滤掉非素数并将剩余数字传递给下一个子进程。

### 实验步骤

1. 创建初始管道并生成数字

   ```c
   int p[2];
   pipe(p);
   if (fork() == 0) {
       filter(p);
   } else {
       close(p[0]); // 关闭读端
       for (int i = 2; i <= 35; i++) {
           write(p[1], &i, sizeof(i));
       }
       close(p[1]); // 关闭写端
       wait(0); // 等待子进程结束
   }
   ```

2. 在子进程中创建过滤器

   ```c
   void filter(int p[2]) {
       int prime;
       int num;
       close(p[1]); // 关闭写端
       if (read(p[0], &prime, sizeof(prime)) != sizeof(prime))
           exit(0);
       printf("prime %d\n", prime);
   
       int next_p[2];
       pipe(next_p);
       if (fork() == 0) {
           close(p[0]);
           filter(next_p);
       } else {
           close(next_p[0]); // 关闭新管道的读端
           while (read(p[0], &num, sizeof(num)) == sizeof(num)) {
               if (num % prime != 0) {
                   write(next_p[1], &num, sizeof(num));
               }
           }
           close(p[0]);
           close(next_p[1]); // 关闭新管道的写端
           wait(0); // 等待子进程结束
       }
       exit(0);
   }
   ```

### 遇到的问题以及解决方法

1. **管道关闭问题**：

   - **问题**：在进程结束前没有正确关闭管道的读写端，会导致资源泄漏和数据不一致问题。
   - **解决方法**：在每个进程完成读写操作后，明确关闭不再使用的管道端口。例如，在父进程中生成数字后关闭写端，在子进程中读取和处理数据后关闭相应端口。

2. **读写同步问题**：

   - **问题**：如果父进程在子进程准备好读取数据之前关闭了写端，子进程可能会读取到部分数据或出现读取错误。
   - **解决方法**：确保父进程在写入所有数据后才关闭写端，同时确保子进程在关闭读端之前完成所有读取操作。使用 `wait(0)` 等待子进程结束，确保数据处理的同步。

3. **递归深度控制问题**：

   - **问题**：使用递归调用 `filter` 函数会导致递归深度过大，可能会引起栈溢出。
   - **解决方法**：优化递归调用的条件，确保每次递归调用都能有效减少数据量，并且在每个递归层次都明确释放不需要的资源（如管道端口），避免栈溢出。

4. **资源管理问题**：

   - **问题**：在进程间传递管道时，如果不正确管理资源，可能会导致文件描述符泄漏。
   - **解决方法**：在进程开始和结束时，明确关闭不需要的文件描述符，避免资源泄漏。在管道创建和使用的每个步骤都要小心处理文件描述符的打开和关闭。

### 总结与体会

该实验进一步加深了对操作系统进程协作和 IPC 机制的理解，通过编写 `primes` 程序，深入理解了 xv6 中的进程管理和管道通信机制。掌握了如何创建和使用管道在进程间传递数据，并在实践中解决了管道关闭、读写同步、递归深度控制等问题。

## 5. find

### 实验目的

实现一个 `find` 程序，在 xv6 文件系统中搜索并列出指定目录及其子目录中匹配给定名称的文件。

### 实验步骤

1. 创建 `find` 函数，根据当前路径以及文件名递归遍历目录：

   ```c
   void find(char *path, char *filename);
   ```

1. 根据路径打开目录或者文件，检查其是否存在损坏等错误：

   ```c
   // 打开目录或文件
       if ((fd = open(path, 0)) < 0) {
           printf("find: cannot open %s\n", path); // 打开失败则输出错误
           return;
       }
   
       // 获取文件状态
       if (fstat(fd, &st) < 0) {
           printf("find: cannot stat %s\n", path); // 获取文件状态失败则输出错误
           close(fd);
           return;
       }
   ```

1. 根据当前的路径是目录还是文件，进行字符串比对或者递归查找的操作：

   ```c
   switch (st.type) {
       case T_FILE: // 如果是文件类型
           //printf("this is a file: %s\n", fmtname(path));
           if (!strcmp(fmtname(path), filename)) { // 比较文件名
               printf("%s\n", path); // 如果匹配则打印路径
           }
           break;
   
       case T_DIR: // 如果是目录类型
           if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
               printf("find: path too long\n"); // 路径太长则输出错误
               break;
           }
           strcpy(buf, path); // 将路径复制到缓冲区
           p = buf + strlen(buf);
           *p++ = '/'; // 添加路径分隔符
           // 读取目录项
           while (read(fd, &de, sizeof(de)) == sizeof(de)) {
               if (de.inum == 0)
                   continue; // 跳过空目录项
               memmove(p, de.name, DIRSIZ); // 复制目录项名称
               p[DIRSIZ] = 0;
               if (stat(buf, &st) < 0) {
                   printf("find: cannot stat %s\n", buf); // 获取目录项状态失败则输出错误
                   continue;
               }
               if (strcmp(de.name, ".") != 0 && strcmp(de.name, "..") != 0) {
                   find(buf, filename); // 递归查找子目录
               }
           }
           break;
       }
       close(fd); // 关闭文件描述符
   ```

   

### 遇到的问题以及解决方法

1. **文件打开失败问题**：

   - **问题**：在递归遍历目录时，可能会遇到无法打开文件或目录的情况。
   - **解决方法**：在每次尝试打开文件或目录前，添加错误检查，并在失败时输出相应的错误信息，同时确保关闭已打开的文件描述符，避免资源泄漏。

2. **路径长度限制问题**：

   - **问题**：在递归遍历目录时，如果路径过长，会导致缓冲区溢出。
   - **解决方法**：在每次构建新路径时，检查路径长度，确保不会超过缓冲区的限制。如果路径过长，则输出错误信息并跳过该路径。

3. **处理特殊目录问题**：

   - **问题**：在遍历目录时，需要跳过 `.` 和 `..` 目录，否则会导致无限递归。
   - **解决方法**：在处理目录内容时，检查并跳过 `.` 和 `..` 目录。

4. **文件类型判断问题**：

   - **问题**：需要正确判断文件类型，以便对文件和目录分别处理。
   - **解决方法**：使用 `fstat` 函数获取文件状态，并根据文件类型执行相应操作。

### 总结与体会

该实验进一步加深了对操作系统文件管理机制的理解，通过编写 `find` 程序，深入理解了 xv6 文件系统的结构和操作。掌握了如何递归遍历目录，并在实践中解决了文件打开失败、路径长度限制、处理特殊目录等问题。

## 6. xargs

### 实验目的

实现一个 `xargs` 程序，从标准输入读取行，并为每行运行一次给定命令，将行内容作为命令的参数。

### 实验步骤

1. **检查参数数量**：
   确保传递给程序的参数数量至少有两个（程序名和命令名）。
   ```c
   if (argc < 2) {
       fprintf(2, "Usage: xargs command [args...]\n");
       exit(1);
   }
   ```

2. **初始化缓冲区和参数数组**：
   设置用于存储从标准输入读取数据的缓冲区和参数数组。
   ```c
   char buf[512]; // 缓冲区，用于存储从标准输入读取的数据
   char *p = buf; // 指向缓冲区的指针
   char *args[MAXARG]; // 参数数组，用于传递给 exec 的参数
   int n, i;
   
   // 将命令行参数复制到 args 数组中
   for (i = 0; i < argc - 1; i++) {
       args[i] = argv[i + 1];
   }
   ```

3. **读取标准输入数据**：
   使用 `read` 函数从标准输入读取数据，并将其存储在缓冲区中。
   ```c
   while ((n = read(0, p, sizeof(buf) - (p - buf))) > 0) {
       p[n] = '\0'; // 将读取的数据作为字符串处理
       char *start = p; // 用于遍历每一行的指针
   ```

4. **解析每一行数据**：
   遍历缓冲区数据，逐行解析，将每行作为参数传递给命令。
   ```c
       while (*start != '\0') {
           // 跳过行首的换行符
           while (*start != '\0' && *start == '\n') {
               start++;
           }
           if (*start == '\0') break;
   
           args[argc - 1] = start; // 设置当前行的起始位置
   
           // 找到行尾的换行符
           while (*start != '\0' && *start != '\n') {
               start++;
           }
           if (*start != '\0') {
               *start = '\0'; // 将换行符替换为字符串结束符
               start++;
           }
   
           args[argc] = 0; // 参数数组的最后一个元素设为 NULL
   ```

5. **创建子进程并执行命令**：
   使用 `fork` 创建子进程，并在子进程中使用 `exec` 执行命令，将解析的行作为参数传递。
   ```c
           if (fork() == 0) {
               exec(args[0], args); // 在子进程中执行命令
               fprintf(2, "xargs: exec %s failed\n", args[0]); // 如果 exec 失败，输出错误信息
               exit(1); // 退出子进程
           } else {
               wait(0); // 父进程等待子进程结束
           }
       }
   }
   ```

6. **检查读取是否出错**：
   检查 `read` 函数的返回值，如果出错则输出错误信息并退出程序。
   ```c
   if (n < 0) {
       fprintf(2, "xargs: read error\n");
       exit(1);
   }
   ```

7. **退出程序**：
   正常退出程序。
   ```c
   exit(0); // 退出程序
   ```

### 遇到的问题以及解决方法

1. **参数数量不足**：
   - **问题**：如果传递给程序的参数数量不足，程序无法正常运行。
   - **解决方法**：在程序开始时检查参数数量，并在不足时输出用法信息并退出。

2. **读取标准输入失败**：
   - **问题**：在从标准输入读取数据时可能会出错，导致程序无法继续。
   - **解决方法**：在每次读取数据后检查返回值，并在出错时输出错误信息并退出。

3. **子进程执行命令失败**：
   - **问题**：在子进程中执行命令时可能会失败，导致程序无法正常工作。
   - **解决方法**：在子进程中使用 `exec` 执行命令后，检查返回值，并在出错时输出错误信息并退出子进程。

4. **参数解析不正确**：
   - **问题**：从标准输入读取的数据需要正确解析为命令参数，否则命令无法正确执行。
   - **解决方法**：在解析每一行数据时，处理换行符和字符串结束符，确保参数数组的最后一个元素为 `NULL`。

### 总结与体会

该实验进一步加深了对操作系统进程间通信和数据处理机制的理解，通过编写 `xargs` 程序，深入理解了 xv6 中进程管理和标准输入输出处理机制。掌握了如何从标准输入读取数据并将其解析为命令参数，在实践中解决了参数检查、标准输入读取、命令执行失败等问题。
