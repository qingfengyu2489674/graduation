#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>

#include "cacheIOHandler.h"

#define CACHE_TYPE_HOST 0
#define CACHE_TYPE_DEVICE 1

#define IOCTL_READ_BLOCK  _IOR('b', 2, char*)
#define IOCTL_WRITE_BLOCK _IOW('b', 3, char*)

#define CACHE_TYPE_MASK ((off_t)1 << (sizeof(off_t)*8 - 1))
#define GET_CACHE_TYPE(off) (((off) & CACHE_TYPE_MASK) ? 1 : 0)
#define GET_REAL_OFFSET(off) ((off) & ~CACHE_TYPE_MASK)
#define SET_CACHE_TYPE(off, type) \
    ((off) = ((off) & ~CACHE_TYPE_MASK) | ((type) ? CACHE_TYPE_MASK : 0))

#define CACHE_SIZE 512

int openWithCache(const char *pathname, int flags, mode_t mode);
int closeWithCache(int fd);
ssize_t readWithCache(int fd, void *buf, size_t count, off_t offset);
ssize_t writeWithCache(int fd, const void *buf, size_t count, off_t offset);

void* thread_func(void* arg);


int main() 
{
    pthread_t thread1, thread2;

    pthread_create(&thread1, NULL, thread_func, NULL);
    pthread_create(&thread2, NULL, thread_func, NULL);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    unlink("testfile.tmp");

    return 0;
}


void* thread_func(void* arg) 
{
    int fd = openWithCache("/dev/bio_rw_char_dev", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) 
    {
        perror("Failed to open test file in thread");
        return NULL;
    }

    printf("===== Thread %ld: Test 1: Large data write to cache =====\n", pthread_self());
    char largeWriteData[5 * CACHE_SIZE];
    for (int i = 0; i < sizeof(largeWriteData); i++) 
    {
        largeWriteData[i] = 'A' + (i % 26);
    }

    ssize_t bytes_written_large = writeWithCache(fd, largeWriteData, sizeof(largeWriteData), 0);
    if (bytes_written_large < 0) 
    {
        perror("Error writing large data to cache");
        closeWithCache(fd);
        return NULL;
    }
    printf("Thread %ld wrote %ld bytes of large data to cache.\n", pthread_self(), bytes_written_large);


    printf("===== Thread %ld: Test 2: Random data read from cache =====\n", pthread_self());
    char randomReadBuf[CACHE_SIZE] = {0};
    for (off_t offset = 0; offset < sizeof(largeWriteData); offset += CACHE_SIZE / 2) 
    { 
        SET_CACHE_TYPE(offset, 1);
        ssize_t bytes_read_random = readWithCache(fd, randomReadBuf, CACHE_SIZE / 2, offset);
        if (bytes_read_random < 0) 
        {
            perror("Error reading random data from cache");
            closeWithCache(fd);
            return NULL;
        }
        printf("Thread %ld read data from offset %ld: %.*s\n", pthread_self(), offset, (int)bytes_read_random, randomReadBuf);
    }

    printf("===== Thread %ld: Test 3: Unaligned data read from cache =====\n", pthread_self());
    char unalignedReadBuf[CACHE_SIZE] = {0};
    ssize_t bytes_read_unaligned = readWithCache(fd, unalignedReadBuf, CACHE_SIZE / 2, CACHE_SIZE / 3); 
    if (bytes_read_unaligned < 0) 
    {
        perror("Error reading unaligned data from cache");
        closeWithCache(fd);
        return NULL;
    }
    printf("Thread %ld read data from unaligned offset: %.*s\n", pthread_self(), (int)bytes_read_unaligned, unalignedReadBuf);

    printf("===== Thread %ld: Test 4: Reading beyond cache size =====\n", pthread_self());
    char beyondCacheBuf[CACHE_SIZE] = {0};
    ssize_t bytes_read_beyond_cache = readWithCache(fd, beyondCacheBuf, CACHE_SIZE, sizeof(largeWriteData) + 100); // 超出文件末尾
    if (bytes_read_beyond_cache < 0) 
    {
        perror("Error reading beyond the cache size");
        closeWithCache(fd);
        return NULL;
    } 
    else if (bytes_read_beyond_cache == 0) 
    {
        printf("Thread %ld reached beyond cache size (EOF).\n", pthread_self());
    }

    printf("===== Thread %ld: Test 5: Overwriting data in cache =====\n", pthread_self());
    char overwriteData[] = "New data that overwrites existing data.";
    ssize_t bytes_written_overwrite = writeWithCache(fd, overwriteData, strlen(overwriteData) + 1, CACHE_SIZE / 2); // 在缓存中间覆盖
    if (bytes_written_overwrite < 0)
    {
        perror("Error overwriting data in cache");
        closeWithCache(fd);
        return NULL;
    }

    char afterOverwriteBuf[CACHE_SIZE] = {0};
    ssize_t bytes_read_after_overwrite = readWithCache(fd, afterOverwriteBuf, CACHE_SIZE, CACHE_SIZE / 2);
    if (bytes_read_after_overwrite < 0) 
    {
        perror("Error reading after overwrite");
        closeWithCache(fd);
        return NULL;
    }
    printf("Thread %ld read data after overwrite: %.*s\n", pthread_self(), (int)bytes_read_after_overwrite, afterOverwriteBuf);

    printf("===== Thread %ld: Closing the file and cleaning up cache =====\n", pthread_self());
    if (closeWithCache(fd) < 0) 
    {
        perror("Failed to close file");
        return NULL;
    }
    printf("Thread %ld closed the file and cleaned up cache.\n", pthread_self());

    return NULL;
}