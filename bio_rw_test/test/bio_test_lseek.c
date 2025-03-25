#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

/* 与内核中的定义保持一致 */
#define CHAR_DEV "/dev/bio_rw_char_dev"   // 我们在驱动中创建的字符设备
#define SECTOR_SIZE 512

/* 与驱动里定义一致的 ioctl 命令 */
#define IOCTL_READ_SECOND  _IOR('b', 2, char*)
#define IOCTL_WRITE_SECOND _IOW('b', 3, char*)

static void print_hex(const char *prefix, const unsigned char *buf, size_t len) {
    printf("%s (len=%zu):\n", prefix, len);
    for (size_t i = 0; i < len; i++) {
        if (i % 16 == 0) printf("%04zu : ", i);
        printf("%02X ", buf[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (len % 16 != 0) printf("\n");
}

int main()
{
    int fd;
    ssize_t ret;
    unsigned char buffer[SECTOR_SIZE];

    /* 打开我们字符设备驱动 */
    fd = open(CHAR_DEV, O_RDWR);
    if (fd < 0) {
        perror("open char dev");
        return 1;
    }
    printf("Opened char device: %s\n", CHAR_DEV);

    /*--------------------------------------------------------------
     *  1) 测试对默认块设备 (/dev/nvme0n1p1) 的写操作
     *--------------------------------------------------------------*/
    memset(buffer, 0x66, SECTOR_SIZE);  // 用 0x66 填充缓冲区
    print_hex("Write default device buffer", buffer, 32);

    ret = write(fd, buffer, SECTOR_SIZE);  // 写入默认设备
    if (ret < 0) {
        perror("write default device");
    } else {
        printf("Wrote %zd bytes to the default device.\n", ret);
    }

    /*--------------------------------------------------------------
     *  2) 测试对默认块设备 (/dev/nvme0n1p1) 的读操作
     *--------------------------------------------------------------*/
    memset(buffer, 0, SECTOR_SIZE);
    ret = read(fd, buffer, SECTOR_SIZE);   // 从默认设备读取
    if (ret < 0) {
        perror("read default device");
    } else {
        printf("Read %zd bytes from the default device.\n", ret);
        print_hex("Read default device buffer", buffer, 32);
    }

    /*--------------------------------------------------------------
     *  3) 测试对第二个块设备 (/dev/nvme0n1p2) 的写操作，通过 ioctl
     *--------------------------------------------------------------*/
    memset(buffer, 0x77, SECTOR_SIZE);  // 用 0x77 填充缓冲区
    print_hex("Write second device buffer", buffer, 32);

    ret = ioctl(fd, IOCTL_WRITE_SECOND, buffer);
    if (ret < 0) {
        perror("ioctl write second device");
    } else {
        printf("IOCTL write second device success.\n");
    }

    /*--------------------------------------------------------------
     *  4) 测试对第二个块设备 (/dev/nvme0n1p2) 的读操作，通过 ioctl
     *--------------------------------------------------------------*/
    memset(buffer, 0, SECTOR_SIZE);
    ret = ioctl(fd, IOCTL_READ_SECOND, buffer);
    if (ret < 0) {
        perror("ioctl read second device");
    } else {
        printf("IOCTL read second device success.\n");
        print_hex("Read second device buffer", buffer, 32);
    }

    /*--------------------------------------------------------------
     *  5) 使用 lseek（模拟 iseek）来调整文件偏移
     *--------------------------------------------------------------*/
    off_t offset = 1024;  // 设置一个新的偏移量
    ret = lseek(fd, offset, SEEK_SET);  // 设置文件偏移量
    if (ret < 0) {
        perror("lseek");
    } else {
        printf("File offset set to %ld\n", (long)offset);
    }

    /*--------------------------------------------------------------
     *  6) 使用 pread 和 pwrite 进行读取和写入
     *--------------------------------------------------------------*/
    memset(buffer, 0x99, SECTOR_SIZE);  // 用 0x99 填充缓冲区
    print_hex("Write to file using pwrite", buffer, 32);
    
    ret = pwrite(fd, buffer, SECTOR_SIZE, offset);  // 使用偏移量进行写操作
    if (ret < 0) {
        perror("pwrite");
    } else {
        printf("Pwrite wrote %zd bytes to file at offset %ld.\n", ret, (long)offset);
    }

    memset(buffer, 0, SECTOR_SIZE);  // 清空缓冲区
    ret = pread(fd, buffer, SECTOR_SIZE, offset);  // 使用偏移量进行读操作
    if (ret < 0) {
        perror("pread");
    } else {
        printf("Pread read %zd bytes from file at offset %ld.\n", ret, (long)offset);
        print_hex("Read buffer from file using pread", buffer, 32);
    }

    /* 关闭字符设备 */
    close(fd);
    return 0;
}
