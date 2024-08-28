#include "hyc.h"

extern int sys_kill();

static void handle_task_alarm(timer_t *timer)
{
    task_t *task = timer->task;
    task->alarm = NULL;
    sys_kill(task->pid, SIGALRM);
}

int sys_alarm(int seconds)
{
    task_t *current_task = running_task();
    if (current_task->alarm != NULL)
    {
        timer_put(current_task->alarm);
    }
    current_task->alarm = timer_add(seconds * 1000, handle_task_alarm, 0);
    return 0;  // 如果需要返回值，可以返回一个值
}
