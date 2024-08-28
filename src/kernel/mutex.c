#include "hyc.h"

void mutex_init(mutex_t *mutex) {
    mutex->value = false; // 初始化时互斥量未被占用
    list_init(&mutex->waiters);
}

// 尝试获取互斥量
void mutex_lock(mutex_t *mutex) {
    // 关闭中断，保证原子操作
    bool intr_state = interrupt_disable();

    task_t *current_task = running_task();
    while (mutex->value) {
        // 如果互斥量已经被占用，将当前任务加入等待队列
        task_block(current_task, &mutex->waiters, TASK_BLOCKED, TIMELESS);
    }

    // 确保没有其他任务持有互斥量
    assert(!mutex->value);

    // 当前任务占用互斥量
    mutex->value = true;
    assert(mutex->value);

    // 恢复之前的中断状态
    set_interrupt_state(intr_state);
}

// 释放互斥量
void mutex_unlock(mutex_t *mutex) {
    // 关闭中断，保证原子操作
    bool intr_state = interrupt_disable();

    // 确保当前任务确实持有互斥量
    assert(mutex->value);

    // 释放互斥量
    mutex->value = false;
    assert(!mutex->value);

    // 如果有等待任务，唤醒其中一个
    if (!list_empty(&mutex->waiters)) {
        task_t *next_task = element_entry(task_t, node, mutex->waiters.tail.prev);
        assert(next_task->magic == ONIX_MAGIC);
        task_unblock(next_task, EOK);

        // 切换到新任务，防止死锁
        task_yield();
    }

    // 恢复之前的中断状态
    set_interrupt_state(intr_state);
}

// 自旋锁初始化
void lock_init(lock_t *lock) {
    lock->holder = NULL;
    lock->repeat = 0;
    mutex_init(&lock->mutex);
}

// 获取锁
void lock_acquire(lock_t *lock) {
    task_t *current_task = running_task();
    if (lock->holder != current_task) {
        mutex_lock(&lock->mutex);
        lock->holder = current_task;
        assert(!lock->repeat);
        lock->repeat = 1;
    } else {
        lock->repeat++;
    }
}

// 释放锁
void lock_release(lock_t *lock) {
    task_t *current_task = running_task();
    assert(lock->holder == current_task);

    if (lock->repeat > 1) {
        lock->repeat--;
    } else {
        assert(lock->repeat == 1);
        lock->holder = NULL;
        lock->repeat = 0;
        mutex_unlock(&lock->mutex);
    }
}

