#!/bin/bash

# 创建设备文件
echo "Creating device file /dev/nvme_read_write with major number 260 and minor number 0"
sudo mknod /dev/nvme_read_write c 260 0

# 设置权限
echo "Setting permissions for /dev/nvme_read_write"
sudo chmod 666 /dev/nvme_read_write

echo "Device /dev/nvme_read_write created and permissions set."
