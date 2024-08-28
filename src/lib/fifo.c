#include "hyc.h"

// 计算FIFO下一个位置
static _inline u32 fifo_next_pos(fifo_t *queue, u32 pos)
{
    return (pos + 1) % queue->length;
}

// 初始化FIFO缓冲区
void fifo_initialize(fifo_t *queue, char *buffer, u32 size)
{
    queue->buf = buffer;
    queue->length = size;
    queue->head = 0;
    queue->tail = 0;
}

// 判断FIFO是否已满
bool fifo_is_full(fifo_t *queue)
{
    // 当下一个位置是尾部时，FIFO已满
    return (fifo_next_pos(queue, queue->head) == queue->tail);
}

// 判断FIFO是否为空
bool fifo_is_empty(fifo_t *queue)
{
    // 当头部和尾部相等时，FIFO为空
    return (queue->head == queue->tail);
}

// 从FIFO中获取一个字节
char fifo_read(fifo_t *queue)
{
    // 获取之前必须确保FIFO非空
    assert(!fifo_is_empty(queue));
    char data = queue->buf[queue->tail];
    queue->tail = fifo_next_pos(queue, queue->tail);
    return data;
}

// 将一个字节写入FIFO
void fifo_write(fifo_t *queue, char data)
{
    // 如果FIFO已满，自动读取一个字节以腾出空间
    while (fifo_is_full(queue))
    {
        fifo_read(queue);
    }
    queue->buf[queue->head] = data;
    queue->head = fifo_next_pos(queue, queue->head);
}
