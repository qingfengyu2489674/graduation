#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define DEVICE_NAME "nvme_read_write"  // 设备名称
#define DEVICE_MINOR 0
#define DEVICE_MAJOR 260                  // 主设备号

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
    printk(KERN_INFO "Reading from device, position: %lld\n", *f_pos);

    if (ret)
        return -EFAULT;

    *f_pos += count;
    return count;
}

// 写入数据到设备
static ssize_t simple_char_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct simple_char_dev *dev = filp->private_data;
    ssize_t ret;

    // 限制写入字节数不超过设备缓存区剩余部分
    if (*f_pos >= sizeof(dev->data))
        return -ENOSPC;  // 设备已满

    if (*f_pos + count > sizeof(dev->data))
        count = sizeof(dev->data) - *f_pos;

    // 从用户空间复制数据到设备内存
    ret = copy_from_user(dev->data + *f_pos, buf, count);
    printk(KERN_INFO "Writing to device, position: %lld\n", *f_pos);
    if (ret)
        return -EFAULT;

    *f_pos += count;
    return count;
}

// 文件操作结构体
static const struct file_operations simple_char_fops = {
    .owner   = THIS_MODULE,
    .open    = simple_char_open,
    .release = simple_char_release,
    .read    = simple_char_read,
    .write   = simple_char_write,
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

    printk(KERN_INFO "behind kzalloc\n");

    ret = register_chrdev_region(devno, 1, DEVICE_NAME);
    if (ret < 0) {
        kfree(simple_dev);
        return ret;
    }

    printk(KERN_INFO "behind register\n");

    cdev_init(&simple_dev->cdev, &simple_char_fops);
    simple_dev->cdev.owner = THIS_MODULE;
    ret = cdev_add(&simple_dev->cdev, devno, 1);
    if (ret) {
        unregister_chrdev_region(devno, 1);
        kfree(simple_dev);
        return ret;
    }

    printk(KERN_INFO "behind cdev init\n");

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
MODULE_DESCRIPTION("A simple character device");
