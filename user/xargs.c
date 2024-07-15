#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[]) {
    // 检查参数数量，至少需要两个参数
    if (argc < 2) {
        fprintf(2, "Usage: xargs command [args...]\n");
        exit(1);
    }

    char buf[512]; // 缓冲区，用于存储从标准输入读取的数据
    char *p = buf; // 指向缓冲区的指针
    char *args[MAXARG]; // 参数数组，用于传递给 exec 的参数
    int n, i;

    // 将命令行参数复制到 args 数组中
    for (i = 0; i < argc - 1; i++) {
        args[i] = argv[i + 1];
    }

    // 读取标准输入的数据
    while ((n = read(0, p, sizeof(buf) - (p - buf))) > 0) {
        p[n] = '\0'; // 将读取的数据作为字符串处理
        char *start = p; // 用于遍历每一行的指针

        // 解析每一行数据
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

            // 创建子进程并执行命令
            if (fork() == 0) {
                exec(args[0], args); // 在子进程中执行命令
                fprintf(2, "xargs: exec %s failed\n", args[0]); // 如果 exec 失败，输出错误信息
                exit(1); // 退出子进程
            } else {
                wait(0); // 父进程等待子进程结束
            }
        }
    }

    // 检查读取是否出错
    if (n < 0) {
        fprintf(2, "xargs: read error\n");
        exit(1);
    }

    exit(0); // 退出程序
}
