#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define DEVICE_PATH "/dev/nvme_read_write"

int main() {
    int fd;
    char write_buf[] = "Hello from user space!";
    char read_buf[1024];
    off_t offset = 0;  // 偏移量

    // 打开设备
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device");
        return 1;
    }

    // 使用 pwrite 写入数据到设备，可以直接指定偏移量
    ssize_t bytes_written = pwrite(fd, write_buf, strlen(write_buf), offset);
    if (bytes_written < 0) {
        perror("Failed to write to the device");
        close(fd);
        return 1;
    }
    printf("Written %zd bytes to the device\n", bytes_written);

    // 使用 pread 读取数据从设备，直接指定偏移量
    ssize_t bytes_read = pread(fd, read_buf, sizeof(read_buf) - 1, 0);  // 偏移量为 0
    if (bytes_read < 0) {
        perror("Failed to read from the device");
        close(fd);
        return 1;
    }

    // Null terminate the buffer and print the data
    read_buf[bytes_read] = '\0';
    printf("Read %zd bytes from the device: %s\n", bytes_read, read_buf);

    // 关闭设备
    close(fd);
    return 0;
}
