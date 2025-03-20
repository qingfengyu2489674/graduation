#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#define CHAR_DEV "/dev/bio_rw_char_dev"  // 我们在驱动中创建的字符设备
#define SECTOR_SIZE 512                   // 每个扇区大小为 512 字节

// 与驱动里定义一致的 ioctl 命令
#define IOCTL_READ_SECOND  _IOR('b', 2, char*)  // 从第二个设备读
#define IOCTL_WRITE_SECOND _IOW('b', 3, char*)  // 写数据到第二个设备

// 每个KV存储条目占用一个扇区（512字节）
#define KV_SIZE 512
// 进行KV测试的次数
#define NUM_OPERATIONS 1000

static void print_hex(const char *prefix, const unsigned char *buf, size_t len) {
    printf("%s (len=%zu):\n", prefix, len);
    for (size_t i = 0; i < len; i++) {
        if (i % 16 == 0) printf("%04zu : ", i);
        printf("%02X ", buf[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (len % 16 != 0) printf("\n");
}

int main() {
    int fd;
    ssize_t ret;
    unsigned char buffer[SECTOR_SIZE];
    unsigned char read_buffer[SECTOR_SIZE];

    // 打开字符设备
    fd = open(CHAR_DEV, O_RDWR);
    if (fd < 0) {
        perror("open char dev");
        return 1;
    }
    printf("Opened char device: %s\n", CHAR_DEV);

    // 测试 KV 存储：写入 NUM_OPERATIONS 次数据到默认设备，并验证
    printf("Starting KV Write/Read Test...\n");
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        // 模拟一个 KV 存储条目：写入 0x55 填充的缓冲区
        memset(buffer, 0x55, SECTOR_SIZE);  // 用 0x55 填充缓冲区
        buffer[0] = i & 0xFF;  // 用 i 的低位填充模拟键

        // 写数据到默认设备
        ret = write(fd, buffer, SECTOR_SIZE);
        if (ret < 0) {
            perror("write default device");
            close(fd);
            return 1;
        }

        // 读取数据并验证
        memset(read_buffer, 0, SECTOR_SIZE);
        ret = read(fd, read_buffer, SECTOR_SIZE);
        if (ret < 0) {
            perror("read default device");
            close(fd);
            return 1;
        }

        // 输出测试进度
        if (i % 100 == 0) {
            printf("KV Operation %d: Write/Read for device %s succeeded.\n", i, CHAR_DEV);
        }

        // 验证数据是否正确
        if (memcmp(buffer, read_buffer, SECTOR_SIZE) != 0) {
            printf("Test FAILED at KV Operation %d\n", i);
            print_hex("Written data", buffer, SECTOR_SIZE);
            print_hex("Read data", read_buffer, SECTOR_SIZE);
            close(fd);
            return 1;
        }
    }

    printf("KV Write/Read Test for default device completed successfully.\n");

    //--------------------------------------------------------------
    // 对第二个设备的 KV 写入和读取
    printf("Starting KV Write/Read Test for second device (%s)...\n", "/dev/nvme0n1p2");

    for (int i = 0; i < NUM_OPERATIONS; i++) {
        // 模拟一个 KV 存储条目：写入 0x77 填充的缓冲区
        memset(buffer, 0x77, SECTOR_SIZE);  // 用 0x77 填充缓冲区
        buffer[0] = i & 0xFF;  // 用 i 的低位填充模拟键

        // 写数据到第二个设备
        ret = ioctl(fd, IOCTL_WRITE_SECOND, buffer);
        if (ret < 0) {
            perror("ioctl write second device");
            close(fd);
            return 1;
        }

        // 读取数据并验证
        memset(read_buffer, 0, SECTOR_SIZE);
        ret = ioctl(fd, IOCTL_READ_SECOND, read_buffer);
        if (ret < 0) {
            perror("ioctl read second device");
            close(fd);
            return 1;
        }

        // 输出测试进度
        if (i % 100 == 0) {
            printf("KV Operation %d: Write/Read for second device (%s) succeeded.\n", i, "/dev/nvme0n1p2");
        }

        // 验证数据是否正确
        if (memcmp(buffer, read_buffer, SECTOR_SIZE) != 0) {
            printf("Test FAILED at KV Operation %d for second device\n", i);
            print_hex("Written data", buffer, SECTOR_SIZE);
            print_hex("Read data", read_buffer, SECTOR_SIZE);
            close(fd);
            return 1;
        }
    }

    printf("KV Write/Read Test for second device completed successfully.\n");

    // 关闭字符设备
    close(fd);
    return 0;
}
