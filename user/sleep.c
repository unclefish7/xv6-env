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
