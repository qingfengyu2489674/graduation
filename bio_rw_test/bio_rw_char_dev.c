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
#include <linux/slab.h>
#include <linux/completion.h>

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

// ----- 两块设备相关指针 -----
// 默认设备（给 read/write 使用）
static struct block_device *bdev_def = NULL;
static struct page *page_def = NULL;

// 第二设备（给 ioctl 使用）
static struct block_device *bdev_second = NULL;
static struct page *page_second = NULL;

/**
 * 小结构，用来在提交 BIO 以后等待其完成。
 * 每次提交一个 BIO，就分配一个 my_bio_info，在 bio_end_io 回调里 complete()，等待方 wait_for_completion()。
 */
struct my_bio_info {
    struct completion done;
    // 如果还需存放出错信息或别的，可以放在这里
};

/**
 * 自定义的 BIO 完成回调
 */
static void my_bio_end_io(struct bio *bio)
{
    struct my_bio_info *info = bio->bi_private;

    printk(KERN_INFO "[bio_rw_char_dev] BIO operation completed\n");

    // 通知等待者，这个 BIO 已经完成
    complete(&info->done);

    // bio_put 释放掉 BIO 结构
    bio_put(bio);
}

/**
 * @brief 同步提交一个 bio
 * 
 * 1. 分配 my_bio_info 并 init_completion()
 * 2. 设置 bio->bi_private = info，bio->bi_end_io = my_bio_end_io
 * 3. submit_bio()
 * 4. wait_for_completion(&info->done)
 * 5. kfree(info)
 * 
 * 这样就能保证在函数返回前，BIO 已经真正完成了。
 */
static void submit_bio_sync(struct bio *bio)
{
    struct my_bio_info *info = kzalloc(sizeof(*info), GFP_KERNEL);

    if (!info) {
        // 如果分配失败，实际上没法安全提交 BIO
        // 这里简单处理：不提交就返回了。
        printk(KERN_ERR "[bio_rw_char_dev] alloc my_bio_info failed\n");
        bio_put(bio);
        return;
    }

    init_completion(&info->done);
    bio->bi_private = info;
    bio->bi_end_io  = my_bio_end_io;

    submit_bio(bio);
    // 等待 BIO 完成回调
    wait_for_completion(&info->done);

    // 回调里已经 bio_put(bio) 了
    kfree(info);
}

//---------------------- 设备1操作：open/read/write/release ----------------------//

// 打开时，同时打开两块设备
static int bio_rw_open(struct inode *inode, struct file *file)
{
    int ret = 0;

    printk(KERN_INFO "[bio_rw_char_dev] open() called\n");
    printk(KERN_INFO "  => default device: %s\n", DEFAULT_DEVICE);
    printk(KERN_INFO "  => second device:  %s\n", SECOND_DEVICE);

    // (1) 打开默认设备
    bdev_def = blkdev_get_by_path(DEFAULT_DEVICE, FMODE_READ | FMODE_WRITE, NULL);
    if (IS_ERR(bdev_def)) {
        printk(KERN_ERR "[bio_rw_char_dev] Failed to open block device: %s\n", DEFAULT_DEVICE);
        ret = PTR_ERR(bdev_def);
        bdev_def = NULL;
        goto fail_open;
    }

    // (2) 打开第二个设备
    bdev_second = blkdev_get_by_path(SECOND_DEVICE, FMODE_READ | FMODE_WRITE, NULL);
    if (IS_ERR(bdev_second)) {
        printk(KERN_ERR "[bio_rw_char_dev] Failed to open block device: %s\n", SECOND_DEVICE);
        ret = PTR_ERR(bdev_second);
        bdev_second = NULL;
        goto fail_open_second;
    }

    // (3) 分配 page 给默认设备
    page_def = alloc_page(GFP_KERNEL);
    if (!page_def) {
        printk(KERN_ERR "[bio_rw_char_dev] Failed to allocate page for default device\n");
        ret = -ENOMEM;
        goto fail_page_def;
    }

    // (4) 分配 page 给第二个设备
    page_second = alloc_page(GFP_KERNEL);
    if (!page_second) {
        printk(KERN_ERR "[bio_rw_char_dev] Failed to allocate page for second device\n");
        ret = -ENOMEM;
        goto fail_page_second;
    }

    printk(KERN_INFO "[bio_rw_char_dev] open() success\n");
    return 0;

fail_page_second:
    __free_page(page_def);
    page_def = NULL;
fail_page_def:
    blkdev_put(bdev_second, FMODE_READ | FMODE_WRITE);
    bdev_second = NULL;
fail_open_second:
    blkdev_put(bdev_def, FMODE_READ | FMODE_WRITE);
    bdev_def = NULL;
fail_open:
    return ret;
}

// 从默认设备读
static ssize_t bio_rw_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    struct bio *bio;
    char *kbuf;
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

    // 设置 BIO
    bio_set_dev(bio, bdev_def);
    bio->bi_bdev  = bdev_def;
    bio->bi_opf   = REQ_OP_READ;

    // 将 page_def 加入到 bio
    bio_add_page(bio, page_def, SECTOR_SIZE, 0);

    // 使用同步函数提交 BIO，并等待完成
    submit_bio_sync(bio);

    // 读操作完成后，再映射内核地址读取内容
    kbuf = kmap(page_def);

    // 打印读取到的数据（仅演示）
    printk(KERN_INFO "[bio_rw_char_dev] Data read from default device: ");
    for (i = 0; i < 16; i++) {
        printk(KERN_CONT "%02x ", (unsigned char)kbuf[i]);
    }
    printk(KERN_CONT "... (showing first 16 bytes)\n");

    // 将数据拷贝到用户空间
    if (len > SECTOR_SIZE)
        len = SECTOR_SIZE;

    if (copy_to_user(buf, kbuf, len)) {
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
    char *kbuf;

    if (!bdev_def || !page_def) {
        printk(KERN_ERR "[bio_rw_char_dev] No default device or page\n");
        return -ENODEV;
    }

    if (len > SECTOR_SIZE)
        len = SECTOR_SIZE;

    // 映射 page_def
    kbuf = kmap(page_def);

    // 把用户空间的数据拷贝到内核
    if (copy_from_user(kbuf, buf, len)) {
        kunmap(page_def);
        return -EFAULT;
    }

    // 将剩余部分用 0x55 填充
    memset(kbuf + len, 0x55, SECTOR_SIZE - len);
    kunmap(page_def);

    // 分配 BIO
    bio = bio_alloc(GFP_KERNEL, 1);
    if (!bio) {
        printk(KERN_ERR "[bio_rw_char_dev] Failed to alloc bio\n");
        return -ENOMEM;
    }

    bio_set_dev(bio, bdev_def);
    bio->bi_bdev  = bdev_def;
    bio->bi_opf   = REQ_OP_WRITE;

    // 提交写操作（同步等待完成）
    bio_add_page(bio, page_def, SECTOR_SIZE, 0);
    submit_bio_sync(bio);

    printk(KERN_INFO "[bio_rw_char_dev] Wrote %zu bytes to default device (sync done)\n", len);
    return len;
}

// 关闭时一次性释放两块设备和 page
static int bio_rw_release(struct inode *inode, struct file *file)
{
    if (bdev_def) {
        blkdev_put(bdev_def, FMODE_READ | FMODE_WRITE);
        bdev_def = NULL;
    }
    if (bdev_second) {
        blkdev_put(bdev_second, FMODE_READ | FMODE_WRITE);
        bdev_second = NULL;
    }
    if (page_def) {
        __free_page(page_def);
        page_def = NULL;
    }
    if (page_second) {
        __free_page(page_second);
        page_second = NULL;
    }

    printk(KERN_INFO "[bio_rw_char_dev] release() -> closed both devices\n");
    return 0;
}

//---------------------- 设备2操作：通过 IOCTL 实现 ----------------------//

// 从第二个设备读 (同步)
static long read_second_device(unsigned long arg)
{
    struct bio *bio;
    char *kbuf;
    size_t len = SECTOR_SIZE;
    long ret = 0;

    if (!bdev_second || !page_second) {
        printk(KERN_ERR "[bio_rw_char_dev] Second device not initialized\n");
        return -ENODEV;
    }

    // 构造读 BIO
    bio = bio_alloc(GFP_KERNEL, 1);
    if (!bio) {
        printk(KERN_ERR "[bio_rw_char_dev] Failed to alloc bio\n");
        return -ENOMEM;
    }

    bio_set_dev(bio, bdev_second);
    bio->bi_bdev  = bdev_second;
    bio->bi_opf   = REQ_OP_READ;

    bio_add_page(bio, page_second, SECTOR_SIZE, 0);

    // 同步等待读完成
    submit_bio_sync(bio);

    // 读完后，把数据拷到用户空间
    kbuf = kmap(page_second);

    if (copy_to_user((char __user *)arg, kbuf, len))
        ret = -EFAULT;

    kunmap(page_second);

    printk(KERN_INFO "[bio_rw_char_dev] IOCTL read from second device (sync)\n");
    return ret;
}

// 写到第二个设备 (同步)
static long write_second_device(unsigned long arg)
{
    struct bio *bio;
    char *kbuf;
    size_t len = SECTOR_SIZE;
    long ret = 0;

    if (!bdev_second || !page_second) {
        printk(KERN_ERR "[bio_rw_char_dev] Second device not initialized\n");
        return -ENODEV;
    }

    // 将用户数据拷贝到 page_second
    kbuf = kmap(page_second);

    if (copy_from_user(kbuf, (char __user *)arg, len)) {
        ret = -EFAULT;
        goto out_unmap;
    }

    // 仅调试打印前 16 个字节
    printk(KERN_INFO "[bio_rw_char_dev] buff from userspace (first 16 bytes): ");
    {
        int i;
        for (i = 0; i < 16 && i < len; i++) {
            printk(KERN_CONT "%02x ", (unsigned char)kbuf[i]);
        }
    }
    printk(KERN_CONT "\n");

    // 剩余部分填充 0x55
    memset(kbuf + len, 0x55, SECTOR_SIZE - len);

out_unmap:
    kunmap(page_second);
    if (ret)
        return ret;

    // 分配 BIO
    bio = bio_alloc(GFP_KERNEL, 1);
    if (!bio) {
        printk(KERN_ERR "[bio_rw_char_dev] Failed to alloc bio\n");
        return -ENOMEM;
    }

    bio_set_dev(bio, bdev_second);
    bio->bi_bdev  = bdev_second;
    bio->bi_opf   = REQ_OP_WRITE;

    bio_add_page(bio, page_second, SECTOR_SIZE, 0);

    // 同步等待写完成
    submit_bio_sync(bio);

    printk(KERN_INFO "[bio_rw_char_dev] IOCTL write to second device (sync)\n");
    return ret;
}

//---------------------- IOCTL 调度 ----------------------//
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
MODULE_AUTHOR("Example Author");
MODULE_DESCRIPTION("Synchronous BIO read/write to two block devices with completion wait.");
