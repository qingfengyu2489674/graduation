# Makefile for simple character device test

# 编译器设置
CC = gcc
CFLAGS = -Wall -g

# 可执行文件名
TARGET = cdev_test

# 源文件
SRC = cdev_test.c

# 目标文件
OBJ = $(SRC:.c=.o)

# 默认目标
all: $(TARGET)
	# 删除中间的编译文件 (.o)
	rm -f $(OBJ)

# 编译规则
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)

# 编译源文件为目标文件
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

# 清理目标文件
clean:
	rm -f $(TARGET) $(OBJ)

.PHONY: all clean
