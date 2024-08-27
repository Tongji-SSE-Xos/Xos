#include "hyc.h"

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

typedef struct signal_frame_t
{
    u32 restorer; // 恢复函数
    u32 sig;      // 信号
    u32 blocked;  // 屏蔽位图
    // 保存调用时的寄存器，用于恢复执行信号之前的代码
    u32 eax;
    u32 ecx;
    u32 edx;
    u32 eflags;
    u32 eip;
} signal_frame_t;

// 获取信号屏蔽位图
int sys_sgetmask()
{
    task_t *task = running_task(); // 获取正在运行的任务
    return task->blocked; // 返回任务的屏蔽位图
}

// 设置信号屏蔽位图
int sys_ssetmask(int newmask)
{
    if (newmask == EOF)
    {
        return -EPERM; // 不允许的操作
    }

    task_t *task = running_task(); // 获取正在运行的任务
    int old = task->blocked; // 保存旧的屏蔽位图
    task->blocked = newmask & ~SIGMASK(SIGKILL); // 更新屏蔽位图，确保 SIGKILL 信号未被屏蔽
    return old; // 返回旧的屏蔽位图
}

// 注册信号处理函数
int sys_signal(int sig, int handler, int restorer)
{
    if (sig < MINSIG || sig > MAXSIG || sig == SIGKILL)
        return EOF;

    task_t *task = running_task(); // 获取正在运行的任务
    sigaction_t *ptr = &task->actions[sig - 1]; // 获取信号的处理结构

    ptr->mask = 0; // 默认不屏蔽信号
    ptr->handler = (void (*)(int))handler; // 设置处理函数
    ptr->flags = SIG_ONESHOT | SIG_NOMASK; // 信号处理标志
    ptr->restorer = (void (*)())restorer; // 设置恢复函数

    return handler; // 返回处理函数地址
}

// 注册信号处理函数，更高级的一种方式
int sys_sigaction(int sig, sigaction_t *action, sigaction_t *oldaction)
{
    if (sig < MINSIG || sig > MAXSIG || sig == SIGKILL)
        return EOF; // 无效信号

    task_t *task = running_task(); // 获取正在运行的任务
    sigaction_t *ptr = &task->actions[sig - 1]; // 获取信号的处理结构

    if (oldaction)
        *oldaction = *ptr; // 保存旧的处理结构

    *ptr = *action; // 更新处理结构

    // 使用位运算更新屏蔽位图，根据标志位设置屏蔽位图
    ptr->mask = (ptr->flags & SIG_NOMASK) ? 0 : (ptr->mask | SIGMASK(sig));
    
    return 0; // 成功
}

// 发送信号
int sys_kill(pid_t pid, int sig)
{
    if (sig < MINSIG || sig > MAXSIG)
        return EOF; // 无效信号

    task_t *task = get_task(pid); // 获取任务
    if (!task)
        return EOF; // 任务不存在
        
    // 确保对内核用户和 init 进程的保护
    if (task->uid == KERNEL_USER || task->pid == 1)
        return EOF;

    LOGK("Sending signal %d to task %s with PID %d\n", sig, task->name, pid);
    task->signal |= SIGMASK(sig); // 设置信号位
    if (task->state == TASK_WAITING || task->state == TASK_SLEEPING)
    {
        task_unblock(task, -EINTR); // 解锁任务
    }
    return 0; // 成功
}

// 内核信号处理函数
void task_signal()
{
    task_t *task = running_task(); // 获取正在运行的任务
    u32 map = task->signal & (~task->blocked); // 计算可用信号
    if (!map)
        return; // 没有信号需要处理

    assert(task->uid); // 确保任务用户 ID 合法
    int sig = 1;
    for (; sig <= MAXSIG; sig++)
    {
        if (map & SIGMASK(sig))
        {
            task->signal &= (~SIGMASK(sig)); // 清除已处理信号
            break;
        }
    }

    sigaction_t *action = &task->actions[sig - 1]; // 获取信号处理结构
    if (action->handler == SIG_IGN)
        return; // 忽略信号

    if (action->handler == SIG_DFL && sig == SIGCHLD)
        return; // 默认处理 SIGCHLD 信号，不做任何操作

    if (action->handler == SIG_DFL)
        task_exit(SIGMASK(sig)); // 默认处理其他信号，退出任务

    // 处理用户态栈，保存当前状态并设置信号处理函数
    intr_frame_t *iframe = (intr_frame_t *)((u32)task + PAGE_SIZE - sizeof(intr_frame_t));

    signal_frame_t *frame = (signal_frame_t *)(iframe->esp - sizeof(signal_frame_t));

    frame->eip = iframe->eip; // 保存指令指针
    frame->eflags = iframe->eflags; // 保存标志寄存器
    frame->edx = iframe->edx; // 保存寄存器 edx
    frame->ecx = iframe->ecx; // 保存寄存器 ecx
    frame->eax = iframe->eax; // 保存寄存器 eax

    frame->blocked = EOF; // 默认屏蔽所有信号

    if (action->flags & SIG_NOMASK)
        frame->blocked = task->blocked; // 不屏蔽信号

    frame->sig = sig; // 设置信号编号
    frame->restorer = (u32)action->restorer; // 设置恢复函数

    LOGK("Old stack pointer: 0x%p\n", iframe->esp);
    iframe->esp = (u32)frame; // 更新栈指针
    LOGK("New stack pointer: 0x%p\n", iframe->esp);
    iframe->eip = (u32)action->handler; // 设置处理函数地址

    if (action->flags & SIG_ONESHOT)
        action->handler = SIG_DFL; // 设置信号处理函数为默认处理方式

    task->blocked |= action->mask; // 更新任务的屏蔽位图
}
