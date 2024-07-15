#include "kernel/types.h"
#include "user/user.h"

int main() {
    int p1[2], p2[2];
    char buf[1];
    
    // 创建两个管道
    pipe(p1);
    pipe(p2);

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
    
    exit(0);
}
