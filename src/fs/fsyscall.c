#include "../include/xos/fs.h"
#include "../include/xos/assert.h"
#include "../include/xos/task.h"
#include "../include/xos/device.h"
#include "../include/xos/syscall.h"
#include "../include/xos/memory.h"
#include "../include/xos/debug.h"
#include "../include/xos/string.h"
#include "../include/xos/stat.h"

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)
#define CHECK_PERMISSION(dir, mode) \
    if (!(dir)->op->permission(dir, mode)) return -EPERM;
#define HANDLE_INODE_OPERATION(op, inode, buf, count, offset) \
    int len = (inode)->op->op(inode, buf, count, offset); \
    if (len > 0) file->offset += len; \
    return len;

static bool is_user_memory(char *buf, int count) {
    bool user = (running_task()->uid != KERNEL_USER);
    return memory_access(buf, count, false, user);
}

static inode_t* get_inode(char *filename, char **next) {
    inode_t *dir = named(filename, next);
    if (!dir) return NULL;
    return dir;
}

static fd_t allocate_file(inode_t *inode, int flags) {
    file_t *file;
    fd_t fd = fd_get(&file);
    if (fd < EOK) return fd;
    file->inode = inode;
    file->flags = flags;
    file->count = 1;
    file->offset = (flags & O_APPEND) ? inode->size : 0;
    return fd;
}

fd_t sys_open(char *filename, int flags, int mode) {
    char *next;
    inode_t *dir = get_inode(filename, &next);
    if (!dir) return -ENOENT;

    inode_t *inode = NULL;
    if (!*next) {
        inode = dir;
        dir->count++;
    } else if (dir->op->open(dir, next, flags, mode, &inode) < 0) {
        iput(dir);
        return -ENOENT;
    }

    fd_t fd = allocate_file(inode, flags);
    if (fd < EOK) iput(inode);
    iput(dir);
    return fd;
}

int sys_creat(char *filename, int mode) {
    return sys_open(filename, O_CREAT | O_TRUNC, mode);
}

void sys_close(fd_t fd) {
    fd_put(fd);
}

int sys_read(fd_t fd, char *buf, int count) {
    if (count < 0 || !is_user_memory(buf, count)) return -EINVAL;
    file_t *file;
    err_t ret = fd_check(fd, &file);
    if (ret < EOK) return ret;
    if ((file->flags & O_ACCMODE) == O_WRONLY) return EOF;
    inode_t *inode = file->inode;
    HANDLE_INODE_OPERATION(read, inode, buf, count, file->offset)
}

int sys_write(unsigned int fd, char *buf, int count) {
    if (count < 0 || !is_user_memory(buf, count)) return -EINVAL;
    file_t *file;
    err_t ret = fd_check(fd, &file);
    if (ret < EOK || (file->flags & O_ACCMODE) == O_RDONLY) return -EPERM;
    inode_t *inode = file->inode;
    HANDLE_INODE_OPERATION(write, inode, buf, count, file->offset)
}

int sys_stat(char *filename, stat_t *statbuf) {
    inode_t *inode = namei(filename);
    if (!inode) return -ENOENT;
    int ret = inode->op->stat(inode, statbuf);
    iput(inode);
    return ret;
}

int sys_fstat(fd_t fd, stat_t *statbuf) {
    file_t *file;
    err_t ret = fd_check(fd, &file);
    if (ret < EOK) return ret;
    inode_t *inode = file->inode;
    return inode->op->stat(inode, statbuf);
}

int sys_mkdir(char *pathname, int mode) {
    char *next;
    inode_t *dir = get_inode(pathname, &next);
    if (!dir || !*next) return -ENOENT;
    CHECK_PERMISSION(dir, P_WRITE);
    int ret = dir->op->mkdir(dir, next, mode);
    iput(dir);
    return ret;
}

int sys_rmdir(char *pathname) {
    char *next;
    inode_t *dir = get_inode(pathname, &next);
    if (!dir || !*next) return -ENOENT;
    CHECK_PERMISSION(dir, P_WRITE);
    int ret = dir->op->rmdir(dir, next);
    iput(dir);
    return ret;
}

int sys_link(char *oldname, char *newname) {
    inode_t *odir, *ndir;
    char *oname, *nname;
    odir = get_inode(oldname, &oname);
    if (!odir) return -ENOENT;
    ndir = get_inode(newname, &nname);
    if (!ndir) {
        iput(odir);
        return -ENOENT;
    }
    int ret = ndir->op->link(odir, oname, ndir, nname);
    iput(odir);
    iput(ndir);
    return ret;
}

int sys_unlink(char *filename) {
    char *next;
    inode_t *dir = get_inode(filename, &next);
    if (!dir || !*next) return -ENOENT;
    CHECK_PERMISSION(dir, P_WRITE);
    int ret = dir->op->unlink(dir, next);
    iput(dir);
    return ret;
}
