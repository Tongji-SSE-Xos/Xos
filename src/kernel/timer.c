#include "hyc.h"

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern u32 volatile jiffies;
extern u32 jiffy;

static list_t timer_list;

static timer_t *timer_get()
{
    return (timer_t *)kmalloc(sizeof(timer_t));
}

void timer_put(timer_t *timer)
{
    list_remove(&timer->node);
    kfree(timer);
}

void default_timeout(timer_t *timer)
{
    task_unblock(timer->task, -ETIME);
}

timer_t *timer_add(u32 expire_ms, handler_t handler, void *arg)
{
    timer_t *timer = timer_get();
    timer->task = running_task();
    timer->expires = jiffies + expire_ms / jiffy; // 计算超时时间
    timer->handler = handler;
    timer->arg = arg;
    timer->active = false;

    // 按照超时时间插入到定时器链表中
    list_insert_sort(&timer_list, &timer->node, element_node_offset(timer_t, node, expires));
    return timer;
}

// 更新定时器超时
void timer_update(timer_t *timer, u32 expire_ms)
{
    list_remove(&timer->node);
    timer->expires = jiffies + expire_ms / jiffy; // 更新超时时间
    list_insert_sort(&timer_list, &timer->node, element_node_offset(timer_t, node, expires));
}

u32 timer_expires()
{
    if (list_empty(&timer_list))
    {
        return EOF;
    }
    timer_t *timer = element_entry(timer_t, node, timer_list.head.next);
    return timer->expires;
}

// 得到超时时间片
int timer_expire_jiffies(u32 expire_ms)
{
    return jiffies + expire_ms / jiffy;
}

// 判断是否已经超时
bool timer_is_expires(u32 expires)
{
    return jiffies > expires;
}

void timer_init()
{
    LOGK("timer init...\n");
    list_init(&timer_list);
}

// 从定时器链表中找到 task 任务的定时器，删除之，用于 task_exit
void timer_remove(task_t *task)
{
    list_node_t *node;
    while ((node = list_next(&timer_list, node)) != &timer_list.tail)
    {
        timer_t *timer = element_entry(timer_t, node, node);
        if (timer->task == task)
        {
            timer_put(timer);
        }
    }
}

// 处理到期的定时器并执行其处理函数
void timer_wakeup()
{
    while (!list_empty(&timer_list) && timer_expires() <= jiffies)
    {
        timer_t *timer = element_entry(timer_t, node, timer_list.head.next);
        timer->active = true;

        assert(timer->expires <= jiffies);

        if (timer->handler)
        {
            timer->handler(timer);
        }
        else
        {
            default_timeout(timer);
        }

        timer_put(timer);
    }
}