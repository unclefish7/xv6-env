#include "kernel/types.h"
#include "user/user.h"

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

int main() {
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
    exit(0);
}
