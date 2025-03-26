#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cacheIOHandler.h"
#include "hashTable.h"

#define CACHE_TYPE_MASK ((off_t)1 << (sizeof(off_t)*8 - 1))
#define SET_CACHE_TYPE(off, type) \
    ((off) = ((off) & ~CACHE_TYPE_MASK) | ((type) ? CACHE_TYPE_MASK : 0))

#define DEVICE_FILE "/dev/bio_rw_char_dev"
#define DATA_SIZE 512

int main(void) {
    // 使用 openWithCache 接口打开设备文件

    int fd = openWithCache(DEVICE_FILE, O_RDWR, 0, CACHE_TYPE_DEVICE);
    if (fd < 0) {
        perror("openWithCache error");
        return EXIT_FAILURE;
    }
    
    // 准备写入数据
    char writeBuf[DATA_SIZE];
    memset(writeBuf, 'B', sizeof(writeBuf));
    
    // 设置写入的 offset，并使用 SET_CACHE_TYPE 将缓存类型设置为 1（device）
    off_t writeOffset = 0;
    printf("writeOffset after SET_CACHE_TYPE: 0x%lx\n", (unsigned long)writeOffset);

    
    // 写入数据
    ssize_t bytes_written = writeWithCache(fd, writeBuf, sizeof(writeBuf), writeOffset);
    if (bytes_written < 0) {
        perror("writeWithCache error");
        closeWithCache(fd);
        return EXIT_FAILURE;
    }
    printf("Wrote %ld bytes to device\n", bytes_written);
    
    // 准备读取数据的缓冲区
    char readBuf[DATA_SIZE];
    memset(readBuf, 0, sizeof(readBuf));
    
    // 设置读取的 offset，并使用 SET_CACHE_TYPE 将缓存类型设置为 1（device）
    off_t readOffset = 0;
    printf("readOffset after SET_CACHE_TYPE: 0x%lx\n", (unsigned long)readOffset);

    
    // 读取数据
    ssize_t bytes_read = readWithCache(fd, readBuf, sizeof(readBuf), readOffset);
    if (bytes_read < 0) {
        perror("readWithCache error");
        closeWithCache(fd);
        return EXIT_FAILURE;
    }
    printf("Read %ld bytes: %.*s\n", bytes_read, (int)bytes_read, readBuf);
    
    // 关闭设备文件并清理缓存
    if (closeWithCache(fd) < 0) {
        perror("closeWithCache error");
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
