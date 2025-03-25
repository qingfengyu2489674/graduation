# 指定编译器
CC = gcc

# 编译选项，包括调试信息和警告
CFLAGS = -Wall -Wextra -g

# 需要编译的源文件列表
SRCS = AVLTree.c \
       cacheIOHandler.c \
       cacheStruct.c \
       hashTable.c \
       lru.c \
       main.c \
       singleCacheHandler.c

# 将 SRCS 中的 .c 文件对应生成 .o 文件
OBJS = $(SRCS:.c=.o)

# 最终生成的可执行文件名字
TARGET = cache

# 默认目标：编译并生成可执行文件
.PHONY: all clean

# all 目标，默认执行
all: $(TARGET)

# 链接生成可执行文件，使用 -pthread 选项链接线程库
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ -pthread

# 生成每个 .o 的规则
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理编译过程中生成的文件，保留最终的可执行文件
clean:
	rm -f $(OBJS)
	rm -f $(TARGET)
