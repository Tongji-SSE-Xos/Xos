#include "../include/xos/assert.h"
#include "../include/xos/fs.h"
#include "../include/xos/task.h"

#define FILE_NR 128

file_t file_table[FILE_NR];
fs_op_t *fs_ops[FS_TYPE_NUM];

// 获取文件系统操作指针，并进行类型检查
fs_op_t *fs_get_op(int type) {
    assert(type > 0 && type < FS_TYPE_NUM);
    return fs_ops[type];
}

// 注册文件系统操作
void fs_register_op(int type, fs_op_t *op) {
    assert(type > 0 && type < FS_TYPE_NUM);
    fs_ops[type] = op;
}

// 默认的不可用系统调用
int fs_default_nosys() {
    return -ENOSYS;
}

// 默认的空操作
void *fs_default_null() {
    return NULL;
}

// 获取一个空闲的文件表项
file_t *get_file() {
    for (size_t i = 3; i < FILE_NR; i++) {
        if (file_table[i].count == 0) {
            file_table[i].count++;
            return &file_table[i];
        }
    }
    panic("Exceed max open files!!!");
}

// 释放文件表项
void put_file(file_t *file) {
    assert(file->count > 0);
    if (--file->count == 0) {
        iput(file->inode);
    }
}

// 检查文件描述符是否合法并返回对应文件表项
err_t fd_check(fd_t fd, file_t **file) {
    task_t *task = running_task();
    if (fd >= TASK_FILE_NR || !task->files[fd])
        return -EINVAL;

    *file = task->files[fd];
    return EOK;
}

// 获取一个空闲的文件描述符并分配文件表项
fd_t fd_get(file_t **file) {
    task_t *task = running_task();
    for (fd_t fd = 3; fd < TASK_FILE_NR; fd++) {
        if (!task->files[fd]) {
            *file = get_file();
            task->files[fd] = *file;
            return fd;
        }
    }
    return -EMFILE;
}

// 释放文件描述符并对应的文件表项
err_t fd_put(fd_t fd) {
    file_t *file;
    err_t ret = fd_check(fd, &file);
    if (ret < EOK) return ret;

    task_t *task = running_task();
    task->files[fd] = NULL;
    put_file(file);
    return EOK;
}

// 初始化文件表
void file_init() {
    for (size_t i = 3; i < FILE_NR; i++) {
        file_table[i] = (file_t){.count = 0, .flags = 0, .offset = 0, .inode = NULL};
    }
}
