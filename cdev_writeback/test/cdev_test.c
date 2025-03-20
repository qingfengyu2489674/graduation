#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>

// 定义设备路径
#define DEVICE_PATH "/dev/nvme_read_write"

// IOCTL 命令
#define IOCTL_WRITEBACK    _IOW('r', 2, char *)

// 用户态测试函数
int main() {
    int fd;
    char write_buf[] = "Hello from user space!";
    char read_buf[1024];
    off_t offset = 0;  // 偏移量
    ssize_t bytes_written, bytes_read;

    // 打开设备
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device");
        return 1;
    }

    // 使用 pwrite 写入数据到设备，指定偏移量
    bytes_written = pwrite(fd, write_buf, strlen(write_buf), offset);  // 使用 pwrite 指定偏移量
    if (bytes_written < 0) {
        perror("Failed to write to the device");
        close(fd);
        return 1;
    }
    printf("Written %zd bytes to the device at offset %lld: %s\n", bytes_written, offset, write_buf);

    // 使用 pread 读取数据从设备，直接指定偏移量
    bytes_read = pread(fd, read_buf, sizeof(read_buf) - 1, offset);  // 使用 pread 读取并指定偏移量
    if (bytes_read < 0) {
        perror("Failed to read from the device");
        close(fd);
        return 1;
    }

    // Null terminate the buffer and print the data
    read_buf[bytes_read] = '\0';
    printf("Read %zd bytes from the device at offset %lld: %s\n", bytes_read, offset, read_buf);

    // 使用 ioctl 进行写回操作

    printf("Issuing ioctl with cmd: %d\n", IOCTL_WRITEBACK);

    if (ioctl(fd, IOCTL_WRITEBACK, write_buf) < 0) {
        perror("Failed to perform writeback operation");
        close(fd);
        return 1;
    }
    printf("Writeback operation completed successfully\n");

    // 关闭设备
    close(fd);
    return 0;
}
