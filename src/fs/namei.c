#include "../include/xos/fs.h"
#include "../include/xos/buffer.h"
#include "../include/xos/stat.h"
#include "../include/xos/syscall.h"
#include "../include/xos/string.h"
#include "../include/xos/task.h"
#include "../include/xos/assert.h"
#include "../include/xos/debug.h"

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// 匹配并更新路径
static inline bool match_and_advance(const char **name, const char *entry_name, char **next, int count) {
    const char *lhs = *name;
    const char *rhs = entry_name;

    while (*lhs == *rhs && *lhs != EOS && *rhs != EOS && count--) {
        lhs++;
        rhs++;
    }

    if (*rhs && count)
        return false;
    if (*lhs && !IS_SEPARATOR(*lhs))
        return false;
    if (IS_SEPARATOR(*lhs))
        lhs++;
    *next = (char *)lhs;
    return true;
}

// 获取父目录 inode
static inline inode_t *get_parent_inode(task_t *task, char **name) {
    inode_t *dir = NULL;
    if (IS_SEPARATOR((*name)[0])) {
        dir = task->iroot;
        (*name)++;
    } else if ((*name)[0]) {
        dir = task->ipwd;
    }
    return dir;
}

// 查找和匹配 inode
static inode_t *find_and_match_inode(inode_t *dir, char *name, char **next, char *right) {
    inode_t *inode = NULL;
    while (true) {
        if (match_and_advance(&name, "..", next, 3) && dir == dir->super->iroot) {
            super_t *super = dir->super;
            inode = super->imount;
            inode->count++;
            iput(dir);
            dir = inode;
        }

        int ret = dir->op->namei(dir, name, next, &inode);
        if (ret < EOK) {
            iput(dir);
            return NULL;
        }

        iput(dir);
        dir = inode;

        if (!ISDIR(dir->mode) || !dir->op->permission(dir, P_EXEC)) {
            iput(dir);
            return NULL;
        }

        if (right == *next) {
            return dir;
        }
        name = *next;
    }
}

// 获取 pathname 对应的父目录 inode
inode_t *named(char *pathname, char **next) {
    task_t *task = running_task();
    char *name = pathname;
    inode_t *dir = get_parent_inode(task, &name);

    if (!dir) return NULL;

    dir->count++;
    *next = name;

    if (!*name) {
        return dir;
    }

    char *right = strrsep(name);
    if (!right || right < name) {
        return dir;
    }
    right++;

    return find_and_match_inode(dir, name, next, right);
}

// 获取 pathname 对应的 inode
inode_t *namei(char *pathname) {
    char *next = NULL;
    inode_t *dir = named(pathname, &next);
    if (!dir) return NULL;

    if (!(*next)) return dir;

    inode_t *inode = NULL;

    if (match_and_advance(&next, "..", &next, 3) && dir == dir->super->iroot) {
        super_t *super = dir->super;
        inode = super->imount;
        inode->count++;
        iput(dir);
        dir = inode;
    }

    int ret = dir->op->namei(dir, next, &next, &inode);
    iput(dir);
    return (ret < EOK) ? NULL : inode;
}
