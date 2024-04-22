#ifndef _SCULL_H_
#define _SCULL_H_

#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/mutex.h>

#define SCULL_QUANTUM 4000  // 量子大小
#define SCULL_QSET 1000     // 量子集大小
#define SCULL_NR_DEVS 4     // 设备数量
#define SCULL_MAJOR 0    // 动态分配设备号

struct scull_qset {
    void **data;  // 数据指针数组
    struct scull_qset *next;  // 指向下一个量子集的指针
};

struct scull_dev {
    struct scull_qset *data;  // 指向第一个量子集的指针
    int quantum;  // 量子大小
    int qset;     // 量子集大小
    unsigned long size;  // 数据大小
    unsigned int access_key;  // 用于访问控制
    struct mutex lock;  // 互斥体
    struct cdev cdev;  // 字符设备结构体
};

#endif