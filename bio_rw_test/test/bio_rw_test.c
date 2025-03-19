#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define DEVICE_PATH "/dev/bio_rw_char_dev"

int main() {
    int fd;
    char write_buf[512] = "Hello, BIO device! This is a test write operation.";  // 测试写入的数据
    char read_buf[512];  // 用于读取的数据缓冲区
    ssize_t bytes_written, bytes_read;

    // 打开设备文件
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd == -1) {
        perror("Failed to open the device");
        return 1;
    }

    // 写数据到设备
    bytes_written = write(fd, write_buf, strlen(write_buf) + 1);  // 写入数据到设备
    if (bytes_written == -1) {
        perror("Failed to write to the device");
        close(fd);
        return 1;
    }
    printf("Wrote %zd bytes to the device\n", bytes_written);

    // 读取数据从设备
    bytes_read = read(fd, read_buf, sizeof(read_buf));  // 从设备读取数据
    if (bytes_read == -1) {
        perror("Failed to read from the device");
        close(fd);
        return 1;
    }

    // 打印读取的数据
    printf("Read %zd bytes from the device: %s\n", bytes_read, read_buf);

    // 关闭设备文件
    close(fd);

    return 0;
}
