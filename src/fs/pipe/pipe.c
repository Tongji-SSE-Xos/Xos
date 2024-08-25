#include "../../include/xos/fs.h"
#include "../../include/xos/task.h"
#include "../../include/xos/memory.h"
#include "../../include/xos/arena.h"
#include "../../include/xos/stat.h"
#include "../../include/xos/fifo.h"
#include "../../include/xos/assert.h"
#include "../../include/xos/debug.h"
#include "../../include/xos/errno.h"

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// 通用的 inode 初始化函数
static inode_t *init_inode(int type, size_t desc_size, size_t page_count) {
    inode_t *inode = get_free_inode();
    inode->dev = -type;
    inode->desc = kmalloc(desc_size);
    inode->addr = alloc_kpage(page_count);
    inode->count = 2;
    inode->type = type;
    inode->op = fs_get_op(type);
    return inode;
}

// 打开管道
static inode_t *pipe_open() {
    inode_t *inode = init_inode(FS_TYPE_PIPE, sizeof(fifo_t), 1);
    fifo_init((fifo_t *)inode->desc, (char *)inode->addr, PAGE_SIZE);
    return inode;
}

// 关闭管道
static void pipe_close(inode_t *inode) {
    if (inode && --inode->count == 0) {
        inode->type = FS_TYPE_NONE;
        kfree(inode->desc);
        free_kpage((u32)inode->addr, 1);
        put_free_inode(inode);
    }
}

// 通用的 IO 操作处理函数
static int pipe_io(inode_t *inode, char *data, int count, int (*condition)(fifo_t *), void (*action)(fifo_t *, char), task_t **waiter) {
    fifo_t *fifo = (fifo_t *)inode->desc;
    int nr = 0;
    while (nr < count) {
        if (condition(fifo)) {
            assert(*waiter == NULL);
            *waiter = running_task();
            task_block(*waiter, NULL, TASK_BLOCKED, TIMELESS);
        }
        action(fifo, data[nr++]);
        if (*waiter) {
            task_unblock(*waiter, EOK);
            *waiter = NULL;
        }
    }
    return nr;
}

// 读管道
static int pipe_read(inode_t *inode, char *data, int count, off_t offset) {
    return pipe_io(inode, data, count, fifo_empty, fifo_get, &inode->rxwaiter);
}

// 写管道
static int pipe_write(inode_t *inode, char *data, int count, off_t offset) {
    return pipe_io(inode, data, count, fifo_full, fifo_put, &inode->txwaiter);
}

// 管道系统调用
int sys_pipe(fd_t pipefd[2]) {
    inode_t *inode = pipe_open();
    task_t *task = running_task();
    file_t *files[2];
    for (size_t i = 0; i < 2; i++) {
        pipefd[i] = fd_get(&files[i]);
        if (pipefd[i] < EOK) {
            while (i > 0) fd_put(pipefd[--i]);
            return -EMFILE;
        }
        files[i]->inode = inode;
        files[i]->flags = (i == 0) ? O_RDONLY : O_WRONLY;
    }
    return EOK;
}

// 管道操作定义
static fs_op_t pipe_op = {
    fs_default_nosys, fs_default_nosys,
    fs_default_nosys, pipe_close,
    fs_default_nosys, pipe_read,
    pipe_write, fs_default_nosys,
    fs_default_nosys, fs_default_nosys,
    fs_default_nosys, fs_default_nosys,
    fs_default_nosys, fs_default_nosys,
    fs_default_nosys, fs_default_nosys,
    fs_default_nosys
};

// 初始化管道
void pipe_init() {
    fs_register_op(FS_TYPE_PIPE, &pipe_op);
}
