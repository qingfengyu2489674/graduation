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
#include <linux/ioctl.h>

//---------------------- 配置部分 ----------------------//
#define CHAR_DEV_NAME "bio_rw_char_dev"

// 默认设备（通过 open/read/write/release 操作）
#define DEFAULT_DEVICE "/dev/nvme0n1p1"

// 第二个设备（通过 ioctl 操作）
#define SECOND_DEVICE  "/dev/nvme0n1p2"

// 这里我们只演示操作一个扇区的大小（512字节）
#ifndef SECTOR_SIZE
    #define SECTOR_SIZE    512
#endif

// 定义新的 ioctl 命令，用于对第二块设备读写
#define IOCTL_READ_SECOND  _IOR('b', 2, char*)   // 从第二个设备读
#define IOCTL_WRITE_SECOND _IOW('b', 3, char*)   // 写数据到第二个设备

//----------------------------------------------------//

static dev_t dev_num;
static struct cdev char_dev;
static struct class *dev_class;

// ----- 默认设备相关变量 -----
static struct block_device *bdev_def;   // 默认设备引用
static struct page *page_def;           // 默认设备使用的 page
static char *buffer_def;                // 指向 page_def 映射后的地址

// 完成回调
static void bio_end_io(struct bio *bio)
{
    printk(KERN_INFO "[bio_rw_char_dev] BIO operation completed\n");
    bio_put(bio); // 必须释放 bio
}

//---------------------- 设备1操作：open/read/write/release ----------------------//

// 打开默认设备
static int bio_rw_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "[bio_rw_char_dev] open() called -> default device: %s\n", DEFAULT_DEVICE);

    // 获取块设备
    bdev_def = blkdev_get_by_path(DEFAULT_DEVICE, FMODE_READ | FMODE_WRITE, NULL);
    if (IS_ERR(bdev_def)) {
        printk(KERN_ERR "[bio_rw_char_dev] Failed to open block device: %s\n", DEFAULT_DEVICE);
        return PTR_ERR(bdev_def);
    }

    // 分配一个 page
    page_def = alloc_page(GFP_KERNEL);
    if (!page_def) {
        printk(KERN_ERR "[bio_rw_char_dev] Failed to allocate page\n");
        blkdev_put(bdev_def, FMODE_READ | FMODE_WRITE);
        return -ENOMEM;
    }

    return 0;
}

// 从默认设备读
static ssize_t bio_rw_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    struct bio *bio;
    unsigned int i;

    if (!bdev_def || !page_def) {
        printk(KERN_ERR "[bio_rw_char_dev] No default device or page\n");
        return -ENODEV;
    }

    // 分配 BIO
    bio = bio_alloc(GFP_KERNEL, 1);
    if (!bio) {
        printk(KERN_ERR "[bio_rw_char_dev] Failed to alloc bio\n");
        return -ENOMEM;
    }

    bio->bi_bdev  = bdev_def;
    bio->bi_end_io = bio_end_io;
    bio_set_dev(bio, bdev_def);
    bio->bi_opf   = REQ_OP_READ;

    // 加入一个 page
    bio_add_page(bio, page_def, SECTOR_SIZE, 0);

    // 提交读取
    submit_bio(bio);

    // 读完之后，将 page 映射到内核地址空间
    buffer_def = kmap(page_def);

    printk(KERN_INFO "[bio_rw_char_dev] Data read from default device (%s): ", DEFAULT_DEVICE);
    for (i = 0; i < SECTOR_SIZE; i++) {
        printk(KERN_CONT "%02x ", (unsigned char)buffer_def[i]);
    }
    printk(KERN_CONT "\n");

    // 将数据拷贝到用户空间
    if (len > SECTOR_SIZE)
        len = SECTOR_SIZE;

    if (copy_to_user(buf, buffer_def, len)) {
        kunmap(page_def);
        return -EFAULT;
    }

    kunmap(page_def);
    return len;
}

// 写到默认设备
static ssize_t bio_rw_write(struct file *file, const char __user *buf, size_t len, loff_t *offset)
{
    struct bio *bio;

    if (!bdev_def || !page_def) {
        printk(KERN_ERR "[bio_rw_char_dev] No default device or page\n");
        return -ENODEV;
    }

    // 映射 page
    buffer_def = kmap(page_def);

    // 把用户空间的数据拷贝到内核
    if (len > SECTOR_SIZE)
        len = SECTOR_SIZE;

    if (copy_from_user(buffer_def, buf, len)) {
        kunmap(page_def);
        return -EFAULT;
    }

    // 将剩余部分用 0x55 填充
    memset(buffer_def + len, 0x55, SECTOR_SIZE - len);

    // 取消映射
    kunmap(page_def);

    // 分配 BIO
    bio = bio_alloc(GFP_KERNEL, 1);
    if (!bio) {
        printk(KERN_ERR "[bio_rw_char_dev] Failed to alloc bio\n");
        return -ENOMEM;
    }

    bio->bi_bdev  = bdev_def;
    bio->bi_end_io = bio_end_io;
    bio_set_dev(bio, bdev_def);
    bio->bi_opf   = REQ_OP_WRITE;

    bio_add_page(bio, page_def, SECTOR_SIZE, 0);

    // 提交写操作
    submit_bio(bio);

    return len;
}

// 释放默认设备
static int bio_rw_release(struct inode *inode, struct file *file)
{
    if (bdev_def) {
        blkdev_put(bdev_def, FMODE_READ | FMODE_WRITE);
        bdev_def = NULL;
    }
    if (page_def) {
        __free_page(page_def);
        page_def = NULL;
    }
    printk(KERN_INFO "[bio_rw_char_dev] release() -> closed default device\n");
    return 0;
}

//---------------------- 设备2操作：通过 IOCTL 实现 ----------------------//
//
// 提供两个新的 ioctl 命令，IOCTL_READ_SECOND 和 IOCTL_WRITE_SECOND，
// 分别对 SECOND_DEVICE (/dev/nvme0n1p2) 进行读写。

// 函数: read_second_device
// 从 SECOND_DEVICE 读取一个扇区到用户缓冲区
static long read_second_device(unsigned long arg)
{
    struct block_device *bdev2;
    struct bio *bio;
    struct page *page2;
    char *kbuf;
    long ret = 0;
    size_t len = SECTOR_SIZE;

    // 打开设备2
    bdev2 = blkdev_get_by_path(SECOND_DEVICE, FMODE_READ, NULL);
    if (IS_ERR(bdev2)) {
        printk(KERN_ERR "[bio_rw_char_dev] Failed to open block device: %s\n", SECOND_DEVICE);
        return PTR_ERR(bdev2);
    }

    // 分配一个 page
    page2 = alloc_page(GFP_KERNEL);
    if (!page2) {
        blkdev_put(bdev2, FMODE_READ);
        return -ENOMEM;
    }

    // 构造读取 BIO
    bio = bio_alloc(GFP_KERNEL, 1);
    if (!bio) {
        __free_page(page2);
        blkdev_put(bdev2, FMODE_READ);
        return -ENOMEM;
    }

    bio->bi_bdev  = bdev2;
    bio->bi_end_io = bio_end_io;
    bio_set_dev(bio, bdev2);
    bio->bi_opf   = REQ_OP_READ;

    bio_add_page(bio, page2, SECTOR_SIZE, 0);

    submit_bio(bio);

    // 将 page2 映射到内核空间
    kbuf = kmap(page2);

    // 拷贝到用户空间 arg（应该是用户传进来的 buffer）
    if (copy_to_user((char __user *)arg, kbuf, len))
        ret = -EFAULT;

    printk(KERN_INFO "[bio_rw_char_dev] IOCTL read from second device (%s)\n", SECOND_DEVICE);

    kunmap(page2);
    __free_page(page2);
    blkdev_put(bdev2, FMODE_READ);

    return ret;
}

// 函数: write_second_device
// 将用户缓冲区(一个扇区)写入 SECOND_DEVICE
static long write_second_device(unsigned long arg)
{
    struct block_device *bdev2;
    struct bio *bio;
    struct page *page2;
    char *kbuf;
    long ret = 0;
    size_t len = SECTOR_SIZE;

    // 打开设备2
    bdev2 = blkdev_get_by_path(SECOND_DEVICE, FMODE_WRITE, NULL);
    if (IS_ERR(bdev2)) {
        printk(KERN_ERR "[bio_rw_char_dev] Failed to open block device: %s\n", SECOND_DEVICE);
        return PTR_ERR(bdev2);
    }

    // 分配 page
    page2 = alloc_page(GFP_KERNEL);
    if (!page2) {
        blkdev_put(bdev2, FMODE_WRITE);
        return -ENOMEM;
    }

    // 映射内核地址
    kbuf = kmap(page2);

    // 将用户数据拷贝到 kbuf
    if (copy_from_user(kbuf, (char __user *)arg, len)) {
        ret = -EFAULT;
        goto write_err;
    }

    // 剩余部分填充 0x55
    memset(kbuf + len, 0x55, SECTOR_SIZE - len);
    kunmap(page2);

    // 分配 BIO
    bio = bio_alloc(GFP_KERNEL, 1);
    if (!bio) {
        ret = -ENOMEM;
        goto write_err2;
    }

    bio->bi_bdev  = bdev2;
    bio->bi_end_io = bio_end_io;
    bio_set_dev(bio, bdev2);
    bio->bi_opf   = REQ_OP_WRITE;

    bio_add_page(bio, page2, SECTOR_SIZE, 0);

    submit_bio(bio);

    printk(KERN_INFO "DEBUG: submit_bio");

    sync_blockdev(bdev2);
    fsync_bdev(bdev2);


    printk(KERN_INFO "[bio_rw_char_dev] IOCTL write to second device (%s)\n", SECOND_DEVICE);

    // 释放资源
    blkdev_put(bdev2, FMODE_WRITE);
    __free_page(page2);
    return ret;

write_err:
    kunmap(page2);
write_err2:
    __free_page(page2);
    blkdev_put(bdev2, FMODE_WRITE);
    return ret;
}

// ioctl 操作
static long bio_rw_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
    case IOCTL_READ_SECOND:
        return read_second_device(arg);

    case IOCTL_WRITE_SECOND:
        return write_second_device(arg);

    default:
        return -EINVAL;  // 不认识的命令
    }
}

//---------------------- FOPS 结构 ----------------------//

static struct file_operations bio_rw_fops = {
    .owner          = THIS_MODULE,
    .open           = bio_rw_open,
    .read           = bio_rw_read,
    .write          = bio_rw_write,
    .release        = bio_rw_release,
    .unlocked_ioctl = bio_rw_ioctl,
};

//---------------------- 模块加载和卸载 ----------------------//

static int __init bio_rw_init(void)
{
    int ret;

    printk(KERN_INFO "[bio_rw_char_dev] Initializing.\n");

    // 分配字符设备号
    ret = alloc_chrdev_region(&dev_num, 0, 1, CHAR_DEV_NAME);
    if (ret < 0) {
        printk(KERN_ERR "[bio_rw_char_dev] Failed to allocate chrdev region\n");
        return ret;
    }

    cdev_init(&char_dev, &bio_rw_fops);
    char_dev.owner = THIS_MODULE;
    ret = cdev_add(&char_dev, dev_num, 1);
    if (ret) {
        printk(KERN_ERR "[bio_rw_char_dev] Failed to add character device\n");
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }

    dev_class = class_create(THIS_MODULE, CHAR_DEV_NAME);
    if (IS_ERR(dev_class)) {
        printk(KERN_ERR "[bio_rw_char_dev] Failed to create device class\n");
        cdev_del(&char_dev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(dev_class);
    }

    device_create(dev_class, NULL, dev_num, NULL, CHAR_DEV_NAME);
    printk(KERN_INFO "[bio_rw_char_dev] Module loaded. Device: /dev/%s\n", CHAR_DEV_NAME);
    return 0;
}

static void __exit bio_rw_exit(void)
{
    device_destroy(dev_class, dev_num);
    class_destroy(dev_class);
    cdev_del(&char_dev);
    unregister_chrdev_region(dev_num, 1);

    printk(KERN_INFO "[bio_rw_char_dev] Module unloaded.\n");
}

module_init(bio_rw_init);
module_exit(bio_rw_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Character device driver for BIO read/write to two block devices (default + second via ioctl)");
