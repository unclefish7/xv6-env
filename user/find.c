#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

char* fmtname(char *path) {
    static char buf[DIRSIZ+1];
    char *p;

    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    if (strlen(p) >= DIRSIZ)
        return p;
    memmove(buf, p, strlen(p));
    buf[strlen(p)] = '\0'; // Ensure null-terminated string
    return buf;
}

// 定义查找函数
void find(char *path, char *filename) {
    char buf[512], *p; // 缓冲区和指针
    int fd; // 文件描述符
    struct dirent de; // 目录项
    struct stat st; // 文件状态

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

    //printf("find: checking %s\n", path);

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
}

int main(int argc, char *argv[]) {
    if (argc != 3) { // 检查参数数量
        printf("Usage: find <path> <filename>\n"); // 参数不足则输出用法
        exit(1);
    }
    find(argv[1], argv[2]); // 调用查找函数
    exit(0);
}
