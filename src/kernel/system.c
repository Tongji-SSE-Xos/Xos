#include "hyc.h"

extern task_t *task_table[TASK_NR];

// 设置新的 umask 并返回旧的 umask
mode_t sys_umask(mode_t mask)
{
    task_t *current_task = running_task();
    mode_t previous_umask = current_task->umask;
    current_task->umask = mask & 0777;  // 将新的 umask 限制在 0777 的范围内
    return previous_umask;
}

// 设置进程组 ID
int sys_setpgid(int pid, int pgid)
{
    task_t *current_task = running_task();
    pid = pid ? pid : current_task->pid;  // 如果 pid 为 0，使用当前进程的 pid
    pgid = pgid ? pgid : pid;  // 如果 pgid 为 0，使用 pid 作为 pgid

    for (task_t *task = task_table[0]; task < task_table[TASK_NR]; task++)  // 直接遍历任务表
    {
        if (task && task->pid == pid)  // 检查任务是否存在并匹配 pid
        {
            if (task_leader(task) || task->sid != current_task->sid)  // 检查是否为会话首领及会话 ID 是否匹配
                return -EPERM;  // 如果是会话首领或会话 ID 不匹配，返回权限错误
            task->pgid = pgid;  // 设置新的进程组 ID
            return EOK;
        }
    }
    return -ESRCH;  // 如果未找到匹配的任务，返回无此进程错误
}

// 获取当前进程的进程组 ID
int sys_getpgrp()
{
    return running_task()->pgid;
}

// 创建新会话并设置会话 ID 和进程组 ID
int sys_setsid()
{
    task_t *current_task = running_task();
    if (task_leader(current_task))  // 检查是否为会话首领
        return -EPERM;  // 如果是会话首领，返回权限错误
    current_task->sid = current_task->pgid = current_task->pid;  // 设置会话 ID 和进程组 ID 为当前进程 ID
    return current_task->sid;
}