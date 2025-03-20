#!/bin/bash
#
# 使用 parted 对 /dev/nvme0n1 进行分区：
#   - 第一个分区从 1MiB 到 (1 + PART1_SIZE_MB)MiB，用于系统预留
#   - 第二个分区从 (1 + PART1_SIZE_MB)MiB 到 100%（剩余空间），用于系统预留
#
# 注意：本脚本会清除 /dev/nvme0n1 上的所有数据，请确保盘名正确且无重要数据。

DISK="/dev/nvme0n1"
# 定义第一个分区的大小（单位：MB）
PART1_SIZE_MB=128

# 计算第一个分区的起始和结束位置
START=1
END=$((PART1_SIZE_MB + START))

# 1. 创建 GPT 分区表
parted -s "$DISK" mklabel gpt

# 2. 创建第一个分区，大小为 ${PART1_SIZE_MB}MB（从 ${START}MiB 到 ${END}MiB）
parted -s "$DISK" mkpart primary ${START}MiB ${END}MiB

# 3. 创建第二个分区，从 ${END}MiB 到磁盘末尾
parted -s "$DISK" mkpart primary ${END}MiB 100%

# 4. 刷新分区表
partprobe "$DISK"

# 5. 提示完成
echo "分区已创建："
echo "第一个分区：${DISK}p1 (${PART1_SIZE_MB}MB)"
echo "第二个分区：${DISK}p2 (剩余空间)"

sudo chmod 666 /dev/nvme0n1p1
sudo chmod 666 /dev/nvme0n1p2

echo "已经赋予两个分区读写权限"

