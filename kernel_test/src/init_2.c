// hello.c
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

void create_device_nodes() {
    // 创建 /dev/console
    if (mknod("/dev/console", S_IFCHR | 0600, makedev(5, 1)) && errno != EEXIST) {
        perror("mknod /dev/console");
        exit(1);
    }

    // 创建 /dev/null
    if (mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3)) && errno != EEXIST) {
        perror("mknod /dev/null");
        exit(1);
    }

    // 创建 /dev/tty
    if (mknod("/dev/tty", S_IFCHR | 0666, makedev(5, 0)) && errno != EEXIST) {
        perror("mknod /dev/tty");
        exit(1);
    }
}

int main(void) {
    // 挂载 proc 文件系统
    if (mount("proc", "/proc", "proc", 0, NULL) < 0) {
        perror("mount /proc");
        exit(1);
    }
    printf("Hello, World!\n");
    while (1) {
        pause();
    }

    return 0;
}
