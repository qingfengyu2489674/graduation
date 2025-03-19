#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/ioctl.h>

#define DEVICE_NAME "nvme_read_write"  // 设备名称
#define DEVICE_MINOR 0
#define DEVICE_MAJOR 260  // 主设备号

// 写操作类型
#define IOCTL_WRITEBACK    _IOW('r', 2, char *)

// 设备数据结构
struct simple_char_dev {
    struct cdev cdev;
    char data[1024];  // 设备内存，用于保存数据
};

static struct simple_char_dev *simple_dev = NULL;

// 打开字符设备
static int simple_char_open(struct inode *inode, struct file *filp)
{
    filp->private_data = simple_dev;
    return 0;
}

// 关闭字符设备
static int simple_char_release(struct inode *inode, struct file *filp)
{
    return 0;
}

// 读取设备数据
static ssize_t simple_char_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct simple_char_dev *dev = filp->private_data;
    ssize_t ret;

    // 限制读取字节数不超过设备缓存区剩余部分
    if (*f_pos >= sizeof(dev->data))
        return 0;  // 文件结尾

    if (*f_pos + count > sizeof(dev->data))
        count = sizeof(dev->data) - *f_pos;

    // 从设备内存复制数据到用户空间
    ret = copy_to_user(buf, dev->data + *f_pos, count);
    if (ret)
        return -EFAULT;

    *f_pos += count;
    return count;
}

// 写入数据到设备 - Normal Write
static ssize_t normal_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct simple_char_dev *dev = filp->private_data;
    ssize_t ret;

    if (*f_pos >= sizeof(dev->data))
        return -ENOSPC;  // 设备已满

    if (*f_pos + count > sizeof(dev->data))
        count = sizeof(dev->data) - *f_pos;

    // 从用户空间复制数据到设备内存
    ret = copy_from_user(dev->data + *f_pos, buf, count);
    if (ret)
        return -EFAULT;

    *f_pos += count;
    return count;
}

// 写入数据到设备 - Writeback
static ssize_t writeback(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
    struct simple_char_dev *dev = filp->private_data;
    ssize_t ret;

    printk(KERN_INFO "Writeback operation\n");
    printk(KERN_INFO "f_pos: %lld, count: %zu, dev->data size: %zu\n", *f_pos, count, sizeof(dev->data));

    // 处理设备已满的情况
    if (*f_pos >= sizeof(dev->data)) {
        printk(KERN_WARNING "Device is full, f_pos: %lld >= data size: %zu\n", *f_pos, sizeof(dev->data));
        return -ENOSPC;  // 设备已满
    }

    // 如果写入数据会超过设备内存的大小，调整 count
    if (*f_pos + count > sizeof(dev->data)) {
        printk(KERN_INFO "Adjusting count, *f_pos: %lld + count: %zu exceeds data size: %zu\n", *f_pos, count, sizeof(dev->data));
        count = sizeof(dev->data) - *f_pos;
        printk(KERN_INFO "New count: %zu\n", count);
    }

    // 从内核空间的 buf 复制数据到设备内存（writeback）
    printk(KERN_INFO "Copying from kernel space to device memory...\n");

    printk(KERN_INFO "Kernel space address: %p, count: %zu\n", buf, count);

    // 假设 buf 已经是内核空间的地址，直接拷贝数据
    memcpy(dev->data + *f_pos, buf, count);

    // 更新文件偏移量
    *f_pos += count;
    printk(KERN_INFO "Data written successfully, updated f_pos: %lld\n", *f_pos);

    // 返回写入的字节数
    return count;
}


// `ioctl` 操作
static long simple_char_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct simple_char_dev *dev = filp->private_data;
    char buf[1024];  // 假设最大写入数据大小为1024字节
    size_t len;

    pr_info("cmd: %u\n", cmd);

    switch (cmd) {
     case IOCTL_WRITEBACK:
        if (copy_from_user(buf, (char __user *)arg, sizeof(buf)))
            return -EFAULT;

        printk(KERN_INFO "Data in buf: %s\n", buf);

        len = strlen(buf);
        pr_info("Length of the string in buf: %zu\n", len);
        pr_info("sizeof(dev->data): %zu\n", sizeof(dev->data));
        
    
        // if (len > sizeof(dev->data))
        //     return -ENOSPC;

        // 执行 writeback 操作
        return writeback(filp, buf, len, &filp->f_pos);  // 传递 f_pos 来处理偏移量
        break;

     default:
        printk(KERN_INFO "ioctl dont come in the writeback\n");
        return -EINVAL;
    }

    return 0;
}

// 文件操作结构体
static const struct file_operations simple_char_fops = {
    .owner   = THIS_MODULE,
    .open    = simple_char_open,
    .release = simple_char_release,
    .read    = simple_char_read,
    .write   = normal_write,  // 使用正常写操作
    .unlocked_ioctl = simple_char_ioctl,  // 只保留写回操作
    .llseek  = default_llseek,
};

// 初始化模块
static int __init simple_char_init(void)
{
    int ret;
    dev_t devno = MKDEV(DEVICE_MAJOR, DEVICE_MINOR);

    printk(KERN_INFO "simple_char_init\n");

    simple_dev = kzalloc(sizeof(*simple_dev), GFP_KERNEL);
    if (!simple_dev)
        return -ENOMEM;

    ret = register_chrdev_region(devno, 1, DEVICE_NAME);
    if (ret < 0) {
        kfree(simple_dev);
        return ret;
    }

    cdev_init(&simple_dev->cdev, &simple_char_fops);
    simple_dev->cdev.owner = THIS_MODULE;
    ret = cdev_add(&simple_dev->cdev, devno, 1);
    if (ret) {
        unregister_chrdev_region(devno, 1);
        kfree(simple_dev);
        return ret;
    }

    printk(KERN_INFO "Simple character device loaded\n");
    return 0;
}

// 卸载模块
static void __exit simple_char_exit(void)
{
    cdev_del(&simple_dev->cdev);
    unregister_chrdev_region(MKDEV(DEVICE_MAJOR, DEVICE_MINOR), 1);
    kfree(simple_dev);
    printk(KERN_INFO "Simple character device unloaded\n");
}

module_init(simple_char_init);
module_exit(simple_char_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple character device with normal write and writeback operations");
