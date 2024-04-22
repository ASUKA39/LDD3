#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>

#include "scull.h"

int scull_major = SCULL_MAJOR;  // 设备号是否动态分配
int scull_minor = 0;  // 设备号
int scull_nr_devs = SCULL_NR_DEVS;  // 设备数量
int scull_quantum = SCULL_QUANTUM;  // 量子大小
int scull_qset = SCULL_QSET;  // 量子集大小

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);
module_param(scull_qset, int, S_IRUGO);

MODULE_AUTHOR("Alessandro Rubini, Jonathan Corbet");
MODULE_LICENSE("Dual BSD/GPL");

struct scull_dev *scull_devices;

int scull_trim(struct scull_dev *dev) {     // 释放设备内存
    struct scull_qset *next, *dptr; // 量子集指针
    int qset = dev->qset;   // 量子集大小
    int i;

    for (dptr = dev->data; dptr; dptr = next) { // 释放每个量子集
        if (dptr->data) {
            for (i = 0; i < qset; i++) {    // 释放每个量子
                kfree(dptr->data[i]);     // 释放量子
            }
            kfree(dptr->data);  // 释放量子集
            dptr->data = NULL;
        }
        next = dptr->next;
        kfree(dptr);
    }
    dev->size = 0;
    dev->quantum = scull_quantum;
    dev->qset = scull_qset;
    dev->data = NULL;
    return 0;
}

int scull_open(struct inode *inode, struct file *filp) {
    struct scull_dev *dev;  // 设备结构体指针

    dev = container_of(inode->i_cdev, struct scull_dev, cdev);  // 从inode获取设备结构体指针
    filp->private_data = dev;   // 设置文件私有数据

    if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {  // 如果是以写方式打开，直接释放原有内存，等同于重写文件
        if (mutex_lock_interruptible(&dev->lock)) {  // 加锁
            return -ERESTARTSYS;
        }
        scull_trim(dev);    // 释放设备内存
        mutex_unlock(&dev->lock);   // 解锁
    }
    return 0;
}

int scull_release(struct inode *inode, struct file *filp) {
    return 0;
}

struct scull_qset *scull_follow(struct scull_dev *dev, int n) {
    struct scull_qset *qs = dev->data;  // 量子集指针

    if (!qs) {  // 如果量子集不存在
        qs = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);  // 分配量子集内存
        if (qs == NULL) {
            return NULL;
        }
        memset(qs, 0, sizeof(struct scull_qset));  // 初始化量子集
    }

    while (n--) {   // 获取量子集指针
        if (!qs->next) {
            qs->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);  // 分配量子集内存
            if (qs->next == NULL) {
                return NULL;
            }
            memset(qs->next, 0, sizeof(struct scull_qset));  // 初始化量子集
        }
        qs = qs->next;
    }
    return qs;
}

ssize_t scull_read(struct file *flip, char __user *buf, size_t count, loff_t *f_pos) {
    struct scull_dev *dev = flip->private_data;  // 获取设备结构体指针
    struct scull_qset *dptr;    // 量子集指针
    int quantum = dev->quantum, qset = dev->qset;  // 量子大小和量子集大小
    int itemsize = quantum * qset; // 量子总数
    int item, s_pos, q_pos, rest;   // 量子索引，量子内偏移，剩余量子
    ssize_t retval = 0; // 返回值

    if (mutex_lock_interruptible(&dev->lock)) {  // 加锁
        return -ERESTARTSYS;
    }
    if (*f_pos >= dev->size) {  // 如果偏移超出文件大小
        goto out;
    }
    if (*f_pos + count > dev->size) {   // 如果读取长度超出文件大小
        count = dev->size - *f_pos;
    }

    item = (long)*f_pos / itemsize; // 计算量子集索引
    rest = (long)*f_pos % itemsize; // 计算量子内偏移
    s_pos = rest / quantum; // 计算量子索引
    q_pos = rest % quantum; // 计算量子内偏移

    dptr = scull_follow(dev, item); // 获取量子集指针

    if (dptr == NULL || !dptr->data || !dptr->data[s_pos]) {  // 如果量子集不存在或者量子不存在
        goto out;
    }
    if (count > quantum - q_pos) {  // 如果读取长度超出量子内剩余长度
        count = quantum - q_pos;
    }
    if (copy_to_user(buf, dptr->data[s_pos] + rest, count)) {  // 拷贝数据到用户空间
        retval = -EFAULT;
        goto out;
    }
    *f_pos += count;    // 更新文件偏移
    retval = count;

out:
    mutex_unlock(&dev->lock);   // 解锁
    return retval;
}

ssize_t scull_write(struct file *flip, const char __user *buf, size_t count, loff_t *f_ops) {
    struct scull_dev *dev = flip->private_data;  // 获取设备结构体指针
    struct scull_qset *dptr;    // 量子集指针
    int quantum = dev->quantum, qset = dev->qset;  // 量子大小和量子集大小
    int itemsize = quantum * qset; // 量子总数
    int item, s_pos, q_pos, rest;   // 量子索引，量子内偏移，剩余量子
    ssize_t retval = -ENOMEM; // 返回值

    if (mutex_lock_interruptible(&dev->lock)) {  // 加锁
        return -ERESTARTSYS;
    }

    item = (long)*f_ops / itemsize; // 计算量子集索引
    rest = (long)*f_ops % itemsize; // 计算量子内偏移
    s_pos = rest / quantum; // 计算量子索引
    q_pos = rest % quantum; // 计算量子内偏移

    dptr = scull_follow(dev, item); // 获取量子集指针
    if (dptr == NULL) {
        goto out;
    }
    if (!dptr->data) {
        dptr->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);  // 分配量子内存
        if (!dptr->data) {
            goto out;
        }
        memset(dptr->data, 0, qset * sizeof(char *));  // 初始化量子
    }
    if (!dptr->data[s_pos]) {
        dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);  // 分配量子内存
        if (!dptr->data[s_pos]) {
            goto out;
        }
    }
    if (count > quantum - q_pos) {  // 如果写入长度超出量子内剩余长度
        count = quantum - q_pos;
    }
    if (copy_from_user(dptr->data[s_pos] + q_pos, buf, count)) {  // 拷贝数据到内核空间
        retval = -EFAULT;
        goto out;
    }
    *f_ops += count;    // 更新文件偏移
    retval = count;

    if (dev->size < *f_ops) {   // 更新文件大小
        dev->size = *f_ops;
    }

out:
    mutex_unlock(&dev->lock);   // 解锁
    return retval;
}

struct file_operations scull_fops = {
    .owner = THIS_MODULE,
    // .llseek = scull_llseek,
    .read = scull_read,
    .write = scull_write,
    // .unlocked_ioctl = scull_ioctl,
    .open = scull_open,
    .release = scull_release,
};

static void scull_setup_cdev(struct scull_dev *dev, int index) {
    int err, devno = MKDEV(scull_major, scull_minor + index);

    cdev_init(&dev->cdev, &scull_fops);  // 初始化字符设备
    dev->cdev.owner = THIS_MODULE;  // 设置字符设备拥有者
    dev->cdev.ops = &scull_fops;    // 设置字符设备操作集
    err = cdev_add(&dev->cdev, devno, 1);  // 注册字符设备
    if (err) {
        printk(KERN_NOTICE "Error %d adding scull%d", err, index);
    }
}

void scull_cleanup_module(void) {
    int i;
    dev_t devno = MKDEV(scull_major, scull_minor);  // 获取设备号

    if (scull_devices) {
        for (i = 0; i < scull_nr_devs; i++) {
            scull_trim(&scull_devices[i]);      // 释放设备内存
            cdev_del(&scull_devices[i].cdev);   // 注销字符设备
        }
        kfree(scull_devices);   // 释放设备结构体数组
    }

    unregister_chrdev_region(devno, scull_nr_devs);     // 注销设备号
}

int scull_init_module(void) {
    int result, i;
    dev_t dev = 0;

    if (scull_major) {  // 静态分配设备号
        dev = MKDEV(scull_major, scull_minor);
        result = register_chrdev_region(dev, scull_nr_devs, "scull");
    } else {        // 动态分配设备号
        result = alloc_chrdev_region(&dev, scull_minor, scull_nr_devs, "scull");
        scull_major = MAJOR(dev);
    }
    if (result < 0) {
        printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
        return result;
    }

    scull_devices = kmalloc(scull_nr_devs * sizeof(struct scull_dev), GFP_KERNEL);  // 分配设备结构体数组
    if (!scull_devices) {
        result = -ENOMEM;
        goto fail;
    }
    memset(scull_devices, 0, scull_nr_devs * sizeof(struct scull_dev));

    for (i = 0; i < scull_nr_devs; i++) {      // 初始化每个设备
        scull_devices[i].quantum = scull_quantum;   // 量子大小
        scull_devices[i].qset = scull_qset;         // 量子集大小
        mutex_init(&scull_devices[i].lock);         // 初始化互斥体
        scull_setup_cdev(&scull_devices[i], i);     // 设置字符设备
    }

    dev = MKDEV(scull_major, scull_minor + scull_nr_devs);  // 创建设备文件
    // dev += scull_p_init(dev);   // 创建proc文件
    // dev += scull_access_init(dev);  // 创建访问控制文件

    return 0;

fail:
    scull_cleanup_module();
    return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);