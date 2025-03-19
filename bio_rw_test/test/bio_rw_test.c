#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define DEVICE_PATH "/dev/bio_rw_char_dev"  // 替换为实际的块设备路径
#define SECTOR_SIZE 512                      // 每个扇区的大小
#define SECTOR_COUNT 1                       // 读取/写入的扇区数

// 数据填充和验证函数
void fill_buffer_with_data(char *buffer) {
    // 填充数据为 0x55，例如每个字节都设置为 0x55
    memset(buffer, 0x45, SECTOR_SIZE);
}

int main() {
    int fd;
    char write_buf[SECTOR_SIZE];  // 写入设备的数据缓冲区
    char read_buf[SECTOR_SIZE];   // 从设备读取的数据缓冲区
    ssize_t bytes_written, bytes_read;

    // 填充写入的数据
    fill_buffer_with_data(write_buf);

    // 打开设备文件
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd == -1) {
        perror("Failed to open the device");
        return 1;
    }

    // 写入数据到设备（按扇区为单位）
    bytes_written = write(fd, write_buf, SECTOR_SIZE);
    if (bytes_written == -1) {
        perror("Failed to write to the device");
        close(fd);
        return 1;
    }
    printf("Wrote %zd bytes to the device\n", bytes_written);

    // 读取数据从设备（按扇区为单位）
    bytes_read = read(fd, read_buf, SECTOR_SIZE);
    if (bytes_read == -1) {
        perror("Failed to read from the device");
        close(fd);
        return 1;
    }
    printf("Read %zd bytes from the device\n", bytes_read);

    // 打印写入的数据
    printf("Write buffer (hex): ");
    for (int i = 0; i < SECTOR_SIZE; i++) {
        printf("%02x ", (unsigned char)write_buf[i]);
    }
    printf("\n");

    // 打印读取的数据
    printf("Read buffer (hex): ");
    for (int i = 0; i < SECTOR_SIZE; i++) {
        printf("%02x ", (unsigned char)read_buf[i]);
    }
    printf("\n");

    // 验证读写结果是否正确
    if (memcmp(write_buf, read_buf, SECTOR_SIZE) == 0) {
        printf("Read data matches written data: Test PASSED\n");
    } else {
        printf("Read data does not match written data: Test FAILED\n");
    }

    // 关闭设备文件
    close(fd);

    return 0;
}
