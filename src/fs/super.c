#include "../include/xos/fs.h"
#include "../include/xos/buffer.h"
#include "../include/xos/device.h"
#include "../include/xos/assert.h"
#include "../include/xos/string.h"
#include "../include/xos/stat.h"
#include "../include/xos/task.h"
#include "../include/xos/syscall.h"
#include "../include/xos/stdlib.h"
#include "../include/xos/debug.h"

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define SUPER_NR 16

static super_t super_table[SUPER_NR]; // 超级块表
static super_t *root;                 // 根文件系统超级块

// 初始化超级块结构体
static inline void init_super(super_t *super) {
    super->dev = EOF;
    super->type = FS_TYPE_NONE;
    super->desc = NULL;
    super->buf = NULL;
    super->iroot = NULL;
    super->imount = NULL;
    super->block_size = 0;
    super->sector_size = 0;
    list_init(&super->inode_list);
}

// 查找超级块表中的空闲块
super_t *get_free_super() {
    for (size_t i = 0; i < SUPER_NR; i++) {
        super_t *super = &super_table[i];
        if (super->type == FS_TYPE_NONE) {
            assert(super->count == 0);
            return super;
        }
    }
    panic("no more super block!!!");
}

// 查找设备 dev 的超级块
super_t *get_super(dev_t dev) {
    for (size_t i = 0; i < SUPER_NR; i++) {
        super_t *super = &super_table[i];
        if (super->type != FS_TYPE_NONE && super->dev == dev) {
            return super;
        }
    }
    return NULL;
}

// 释放超级块
void put_super(super_t *super) {
    if (!super) return;
    assert(super->count > 0);
    if (--super->count > 0) return;

    super->type = FS_TYPE_NONE;
    iput(super->imount);
    iput(super->iroot);
    brelse(super->buf);
}

// 读取设备 dev 的超级块
super_t *read_super(dev_t dev) {
    super_t *super = get_super(dev);
    if (super) {
        super->count++;
        return super;
    }

    LOGK("Reading super block of device %d\n", dev);

    super = get_free_super();
    super->count++;

    for (size_t i = 1; i < FS_TYPE_NUM; i++) {
        fs_op_t *op = fs_get_op(i);
        if (op && op->super(dev, super) == EOK) {
            return super;
        }
    }

    put_super(super);
    return NULL;
}

// 挂载根文件系统
static void mount_root() {
    LOGK("Mount root file system...\n");

    device_t *device = device_find(DEV_IDE_PART, 0);
    if (!device) {
        device = device_find(DEV_IDE_CD, 0);
    }
    if (!device) {
        panic("Cann't find available device.");
    }

    root = read_super(device->dev);
    assert(root);

    root->imount = root->iroot;
    root->imount->count++;
    root->iroot->mount = device->dev;
}

// 初始化超级块表并挂载根文件系统
void super_init() {
    for (size_t i = 0; i < SUPER_NR; i++) {
        init_super(&super_table[i]);
    }
    mount_root();
}
