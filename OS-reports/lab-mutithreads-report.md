# Lab: Multithreading

## Uthread: switching between threads

### 实验目的

本次实验的目的是实现一个用户级线程切换机制。通过设计和实现线程创建、线程调度以及线程切换的功能，理解用户级线程的基本工作原理。

### 实验步骤

1. **初始化线程系统**：
    - 初始化主线程，并设置其状态为 `RUNNING`。

    ```c
    void thread_init(void) {
        current_thread = &all_thread[0];
        current_thread->state = RUNNING;
    }
    ```

2. **实现线程调度**：
    - 查找下一个可运行的线程，并调用 `thread_switch` 进行线程切换。

    ```c
    void thread_schedule(void) {
        struct thread *t, *next_thread = 0;
        for (int i = 0; i < MAX_THREAD; i++) {
            t = &all_thread[i];
            if (t->state == RUNNABLE) {
                next_thread = t;
                break;
            }
        }
        if (next_thread) {
            next_thread->state = RUNNING;
            t = current_thread;
            current_thread = next_thread;
            thread_switch((uint64)&t->context, (uint64)&next_thread->context);
        }
    }
    ```

3. **实现线程切换**：
    - 保存当前线程的上下文并恢复下一个线程的上下文。

    ```assembly
    .globl thread_switch
    thread_switch:
        sd ra, 0(a0)
        sd sp, 8(a0)
        sd s0, 16(a0)
        sd s1, 24(a0)
        sd s2, 32(a0)
        sd s3, 40(a0)
        sd s4, 48(a0)
        sd s5, 56(a0)
        sd s6, 64(a0)
        sd s7, 72(a0)
        sd s8, 80(a0)
        sd s9, 88(a0)
        sd s10, 96(a0)
        sd s11, 104(a0)
    
        ld ra, 0(a1)
        ld sp, 8(a1)
        ld s0, 16(a1)
        ld s1, 24(a1)
        ld s2, 32(a1)
        ld s3, 40(a1)
        ld s4, 48(a1)
        ld s5, 56(a1)
        ld s6, 64(a1)
        ld s7, 72(a1)
        ld s8, 80(a1)
        ld s9, 88(a1)
        ld s10, 96(a1)
        ld s11, 104(a1)
    
        ret
    ```

4. **实现线程创建**：
    - 创建一个新线程，初始化线程函数和堆栈指针，并设置返回地址为 `thread_exit`。

    ```c
    void thread_create(void (*func)()) {
        struct thread *t;
        for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
            if (t->state == FREE) break;
        }
        t->context.sp = (uint64)(t->stack + STACK_SIZE);
        t->context.ra = (uint64)thread_exit;
        *(uint64 *)(t->context.sp - 8) = (uint64)func; // 设置线程函数
        t->context.sp -= 8;
        t->state = RUNNABLE;
    }
    ```

5. **实现线程退出**：
    - 将当前线程状态设置为 `FREE` 并调用 `thread_schedule` 切换到下一个线程。

    ```c
    void thread_exit() {
        current_thread->state = FREE;
        thread_schedule();
    }
    ```

### 遇到的问题及解决方法

1. **线程上下文保存和恢复**：
    - 问题：在实现 `thread_switch` 时，不确定需要保存和恢复哪些寄存器。
    - 解决方法：根据 RISC-V 的调用约定，只需要保存和恢复被调用者保存的寄存器（`ra`, `sp`, `s0-s11`）。

2. **线程创建时的堆栈初始化**：
    - 问题：如何初始化新线程的堆栈，使其在第一次运行时能够正确执行线程函数。
    - 解决方法：将堆栈指针初始化为堆栈顶部，并将返回地址设置为 `thread_exit`，然后将线程函数指针压入堆栈。

3. **线程退出后资源释放**：
    - 问题：线程函数执行完毕后如何正确退出并释放资源。
    - 解决方法：在 `thread_exit` 函数中将当前线程状态设置为 `FREE`，并调用 `thread_schedule` 切换到下一个线程。

### 总结与体会

通过本次实验，我深入理解了用户级线程的工作原理。通过实现线程的创建、调度和切换，掌握了线程上下文保存和恢复的方法，并通过 RISC-V 汇编实现了线程切换功能。实验过程中遇到的一些问题和解决方法使我更加理解了线程管理的细节和复杂性。这次实验不仅提高了我对操作系统线程机制的理解，也增强了我的编程和调试能力。

## Using threads

### 实验目的

本实验旨在探索如何使用多线程和锁来实现一个线程安全的哈希表。通过这个实验，我们将了解多线程编程中的数据一致性问题，以及如何使用互斥锁来确保线程安全。

### 实验步骤

#### 1. 下载并编译代码

首先，获取并编译提供的代码：

```sh
$ make ph
```

#### 2. 理解并运行初始代码

运行单线程版本的程序，以了解基础性能和功能：

```sh
$ ./ph 1
```

观察输出结果，确保程序能够正确执行。

#### 3. 多线程问题分析

运行多线程版本的程序，以观察在多线程环境下可能出现的问题：

```sh
$ ./ph 2
```

观察输出结果，特别是缺失的键数量，了解在多线程环境下数据不一致的问题。

#### 4. 修改代码，添加互斥锁

为每个哈希桶添加一个互斥锁，以确保对每个桶的操作是线程安全的。

1. **定义锁**:
   在 `ph.c` 文件中添加锁的声明：

   ```c
   pthread_mutex_t bucket_locks[NBUCKET];
   ```

2. **初始化锁**:
   在 `main` 函数中初始化锁：

   ```c
   for (int i = 0; i < NBUCKET; i++) {
     pthread_mutex_init(&bucket_locks[i], NULL);
   }
   ```

3. **修改 `put` 函数**:
   在插入操作前后加锁和解锁：

   ```c
   void put(int key, int value) {
     int i = key % NBUCKET;
     pthread_mutex_lock(&bucket_locks[i]);
     struct entry *e = 0;
     for (e = table[i]; e != 0; e = e->next) {
       if (e->key == key)
         break;
     }
     if(e){
       e->value = value;
     } else {
       insert(key, value, &table[i], table[i]);
     }
     pthread_mutex_unlock(&bucket_locks[i]);
   }
   ```

4. **修改 `get` 函数**:
   在读取操作前后加锁和解锁：

   ```c
   struct entry* get(int key) {
     int i = key % NBUCKET;
     pthread_mutex_lock(&bucket_locks[i]);
     struct entry *e = 0;
     for (e = table[i]; e != 0; e = e->next) {
       if (e->key == key) break;
     }
     pthread_mutex_unlock(&bucket_locks[i]);
     return e;
   }
   ```

#### 5. 测试修改后的代码

1. **使用单线程测试**:
   确保单线程环境下程序仍然正确：

   ```sh
   $ ./ph 1
   ```

2. **使用多线程测试**:
   确保多线程环境下没有缺失的键，并且性能有所提高：

   ```sh
   $ ./ph 2
   ```

### 遇到的问题及解决方法

1. **数据不一致问题**:
   在多线程环境下，未加锁的哈希表操作会导致数据不一致。通过为每个哈希桶添加互斥锁，解决了并发插入和读取导致的数据不一致问题。

2. **性能问题**:
   初步加锁实现可能会影响性能。通过精细化锁的粒度（每个哈希桶一个锁），在保证线程安全的同时最大限度地提高了并发性能。

### 总结与体会

通过本次实验，我们深入理解了多线程编程中的数据一致性问题和互斥锁的使用。多线程编程不仅要求我们考虑如何正确地实现并发操作，还需要权衡性能和安全性。通过合理使用锁，我们可以在保证数据一致性的同时，尽量减少性能损失。这些技能对于开发高效、安全的多线程应用程序至关重要。

## Barrier

### 实验目的

本实验旨在实现一个屏障机制，使得所有参与的线程在屏障点之前阻塞，直到所有线程都到达屏障点后再继续执行。我们将使用POSIX线程库中的条件变量和互斥锁来实现这一功能。

### 实验步骤

1. **初始化屏障结构**：
   我们定义了一个包含互斥锁、条件变量和计数器的结构体`barrier`，并在`barrier_init`函数中初始化这些变量。

   ```c
   struct barrier {
     pthread_mutex_t barrier_mutex;
     pthread_cond_t barrier_cond;
     int count;        // Number of threads that have reached this round of the barrier
     int round;        // Barrier round
   } bstate;
   
   static void
   barrier_init(void)
   {
     assert(pthread_mutex_init(&bstate.barrier_mutex, NULL) == 0);
     assert(pthread_cond_init(&bstate.barrier_cond, NULL) == 0);
     bstate.count = 0;
     bstate.round = 0;
   }
   ```

2. **实现屏障函数**：
   在`barrier`函数中，我们使用互斥锁保护共享变量。每个线程到达屏障点时增加计数器`count`，如果所有线程都到达了屏障点，则增加轮次`round`，重置计数器`count`，并唤醒所有等待的线程。否则，线程等待，直到所有线程都到达屏障点。

   ```c
   static void 
   barrier()
   {
     pthread_mutex_lock(&bstate.barrier_mutex);
   
     int round = bstate.round;
   
     bstate.count++;
     if (bstate.count == nthread) {
       // All threads have reached the barrier
       bstate.round++;
       bstate.count = 0;
       pthread_cond_broadcast(&bstate.barrier_cond); // Wake up all threads
     } else {
       // Wait until all threads reach the barrier
       while (round == bstate.round) {
         pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
       }
     }
   
     pthread_mutex_unlock(&bstate.barrier_mutex);
   }
   ```

3. **创建线程并使用屏障**：
   在主函数中，我们创建多个线程，每个线程执行一定次数的循环，每次循环中调用`barrier`函数以同步所有线程。线程在每次循环后随机睡眠一段时间以模拟不同步的执行。

   ```c
   static void *
   thread(void *xa)
   {
     long n = (long) xa;
     int i;
   
     for (i = 0; i < 20000; i++) {
       int t = bstate.round;
       assert (i == t);
       barrier();
       usleep(random() % 100);
     }
   
     return 0;
   }
   
   int
   main(int argc, char *argv[])
   {
     pthread_t *tha;
     void *value;
     long i;
   
     if (argc < 2) {
       fprintf(stderr, "%s: %s nthread\n", argv[0], argv[0]);
       exit(-1);
     }
     nthread = atoi(argv[1]);
     tha = malloc(sizeof(pthread_t) * nthread);
     srandom(0);
   
     barrier_init();
   
     for(i = 0; i < nthread; i++) {
       assert(pthread_create(&tha[i], NULL, thread, (void *) i) == 0);
     }
     for(i = 0; i < nthread; i++) {
       assert(pthread_join(tha[i], &value) == 0);
     }
     printf("OK; passed\n");
   }
   ```

### 遇到的问题及解决方法

1. **线程同步问题**：
   初始实现的屏障函数没有正确处理线程同步问题，导致有些线程在所有线程到达屏障点之前就继续执行。通过使用条件变量和互斥锁，确保所有线程在屏障点等待，直到所有线程都到达屏障点。

2. **条件变量的使用**：
   理解条件变量的使用细节，包括`pthread_cond_wait`释放互斥锁并在被唤醒时重新获取互斥锁，这是确保线程安全的关键。

### 总结与体会

通过本实验，我们深入理解了多线程编程中的同步问题，学会了使用POSIX线程库中的互斥锁和条件变量来实现复杂的线程同步机制。条件变量的正确使用对于实现类似屏障这样的同步机制至关重要。本实验不仅加强了对线程同步的理解，还提升了编写线程安全代码的能力。