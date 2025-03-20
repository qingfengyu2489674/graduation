#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define DEVICE_NAME "/dev/nvme0n1p1" // 需要操作的设备
#define CHAR_DEV_NAME "bio_rw_char_dev"

static dev_t dev_num;
static struct cdev char_dev;
static struct class *dev_class;
static struct block_device *bdev;
static struct page *page;
static char *buffer;

static void bio_end_io(struct bio *bio)
{
    // 完成 I/O 后的处理函数
    printk(KERN_INFO "BIO operation completed\n");
    bio_put(bio); // 释放 BIO
}

static int bio_rw_open(struct inode *inode, struct file *file)
{
    // 打开设备时的处理
    printk(KERN_INFO "Opened BIO read/write character device\n");

    // 获取块设备
    bdev = blkdev_get_by_path(DEVICE_NAME, FMODE_READ | FMODE_WRITE, NULL);
    if (IS_ERR(bdev)) {
        printk(KERN_ERR "Failed to open block device\n");
        return PTR_ERR(bdev);
    }

    // 分配一个内存页面
    page = alloc_page(GFP_KERNEL);
    if (!page) {
        printk(KERN_ERR "Failed to allocate memory for page\n");
        blkdev_put(bdev, FMODE_READ | FMODE_WRITE);
        return -ENOMEM;
    }

    return 0;
}

static ssize_t bio_rw_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    struct bio *bio;
    int ret;
    unsigned int i;

    // 提交读取操作
    bio = bio_alloc(GFP_KERNEL, 1);  // 分配一个新的 BIO 请求
    bio->bi_bdev = bdev;
    bio->bi_end_io = bio_end_io;  // 设置 BIO 完成时的回调函数
    bio_set_dev(bio, bdev);

    // 设置为读请求
    bio->bi_opf = REQ_OP_READ;  // 设置为读操作

    // 读取操作：将页面添加到 BIO 请求中
    bio_add_page(bio, page, 512, 0); // 将页面添加到 BIO 请求中

    // 提交读操作
    submit_bio(bio);

    // 等待读操作完成后，在 bio_end_io 中处理读取数据
    buffer = kmap(page);  // 映射页面到内核地址空间
    printk(KERN_INFO "Data read from block device: ");
    for (i = 0; i < 512; i++) {
        printk(KERN_CONT "%02x ", buffer[i]); // 打印读取的数据
    }
    printk(KERN_CONT "\n");

    // 将数据复制到用户空间
    if (copy_to_user(buf, buffer, len)) {
        kunmap(page);
        return -EFAULT;
    }

    kunmap(page);  // 解锁页面映射
    return len;
}

static ssize_t bio_rw_write(struct file *file, const char __user *buf, size_t len, loff_t *offset)
{
    struct bio *bio;

    // 从用户空间复制数据
    buffer = kmap(page);  // 映射页面到内核地址空间
    if (copy_from_user(buffer, buf, len)) {
        kunmap(page);
        return -EFAULT;
    }
    memset(buffer + len, 0x55, 512 - len); // 填充数据至页面剩余部分
    kunmap(page);  // 解锁页面映射

    // 提交写操作
    bio = bio_alloc(GFP_KERNEL, 1);  // 分配一个新的 BIO 请求
    bio->bi_bdev = bdev;
    bio->bi_end_io = bio_end_io;  // 设置 BIO 完成时的回调函数
    bio_set_dev(bio, bdev);

    // 设置为写请求
    bio->bi_opf = REQ_OP_WRITE;  // 设置为写操作

    // 将页面添加到 BIO 请求中
    bio_add_page(bio, page, 512, 0); // 将页面添加到 BIO 请求中

    // 提交写操作
    submit_bio(bio);

    return len;
}

static int bio_rw_release(struct inode *inode, struct file *file)
{
    // 释放资源
    if (bdev) {
        blkdev_put(bdev, FMODE_READ | FMODE_WRITE);
    }
    if (page) {
        __free_page(page);  // 释放页面
    }

    printk(KERN_INFO "Closed BIO read/write character device\n");
    return 0;
}

static struct file_operations bio_rw_fops = {
    .owner = THIS_MODULE,
    .open = bio_rw_open,
    .read = bio_rw_read,
    .write = bio_rw_write,
    .release = bio_rw_release,
};

static int __init bio_rw_init(void)
{
    int ret;

    printk(KERN_INFO "Initializing BIO read/write character device module\n");

    // 动态分配设备号
    ret = alloc_chrdev_region(&dev_num, 0, 1, CHAR_DEV_NAME);
    if (ret < 0) {
        printk(KERN_ERR "Failed to allocate chrdev region\n");
        return ret;
    }

    // 创建字符设备
    cdev_init(&char_dev, &bio_rw_fops);
    char_dev.owner = THIS_MODULE;
    ret = cdev_add(&char_dev, dev_num, 1);
    if (ret) {
        printk(KERN_ERR "Failed to add character device\n");
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }

    // 创建设备类和设备文件
    dev_class = class_create(THIS_MODULE, CHAR_DEV_NAME);
    if (IS_ERR(dev_class)) {
        printk(KERN_ERR "Failed to create device class\n");
        cdev_del(&char_dev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(dev_class);
    }

    device_create(dev_class, NULL, dev_num, NULL, CHAR_DEV_NAME);

    return 0;
}

static void __exit bio_rw_exit(void)
{
    // 清理操作
    device_destroy(dev_class, dev_num);
    class_destroy(dev_class);
    cdev_del(&char_dev);
    unregister_chrdev_region(dev_num, 1);

    printk(KERN_INFO "Exiting BIO read/write character device module\n");
}

module_init(bio_rw_init);
module_exit(bio_rw_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Character device driver for BIO read/write to block device");
