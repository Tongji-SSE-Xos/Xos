#include "hyc.h"

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define COM1_IOBASE 0x3F8 // 串口 1 基地址
#define COM2_IOBASE 0x2F8 // 串口 2 基地址

// 串口寄存器偏移
#define COM_DATA 0          // 数据寄存器
#define COM_INTR_ENABLE 1   // 中断允许
#define COM_BAUD_LSB 0      // 波特率低字节
#define COM_BAUD_MSB 1      // 波特率高字节
#define COM_INTR_IDENTIFY 2 // 中断识别
#define COM_LINE_CONTROL 3  // 线控制
#define COM_MODEM_CONTROL 4 // 调制解调器控制
#define COM_LINE_STATUS 5   // 线状态
#define COM_MODEM_STATUS 6  // 调制解调器状态

// 线状态寄存器标志
#define LSR_DR 0x01  // 数据可读
#define LSR_OE 0x02  // 溢出错误
#define LSR_PE 0x04  // 奇偶校验错误
#define LSR_FE 0x08  // 帧错误
#define LSR_BI 0x10  // 中断标志
#define LSR_THRE 0x20 // 发送保持寄存器空
#define LSR_TEMT 0x40 // 发送移位寄存器空
#define LSR_IE 0x80  // 中断使能

#define BUF_LEN 64

typedef struct serial_t
{
    u16 iobase;           // 端口号基地址
    fifo_t rx_fifo;       // 读 fifo
    char rx_buf[BUF_LEN]; // 读 缓冲
    lock_t rlock;         // 读锁
    task_t *rx_waiter;    // 读等待任务
    lock_t wlock;         // 写锁
    task_t *tx_waiter;    // 写等待任务
} serial_t;

static serial_t serials[2];

void recv_data(serial_t *serial)
{
    char ch = inb(serial->iobase);
    if (ch == '\r') // 特殊处理，回车键直接换行
    {
        ch = '\n';
    }
    fifo_put(&serial->rx_fifo, ch);
    if (serial->rx_waiter != NULL) // 读进程不为空
    {
        task_unblock(serial->rx_waiter, EOK); // 解除阻塞
        serial->rx_waiter = NULL;
    }
}

// 中断处理函数
void serial_handler(int vector)
{
    u32 irq = vector - 0x20;
    assert(irq == IRQ_SERIAL_1 || irq == IRQ_SERIAL_2);
    send_eoi(vector);

    serial_t *serial = &serials[irq - IRQ_SERIAL_1];
    u8 state = inb(serial->iobase + COM_LINE_STATUS);

    if (state & LSR_DR) // 数据可读
    {
        recv_data(serial);
    }

    // 发送数据可用且写进程阻塞
    if ((state & LSR_THRE) && serial->tx_waiter)
    {
        task_unblock(serial->tx_waiter, EOK);
        serial->tx_waiter = NULL;
    }
}

// 读取数据
int serial_read(serial_t *serial, char *buf, u32 count)
{
    lock_acquire(&serial->rlock); // 读锁加锁
    int nr = 0;
    while (nr < count)
    {
        while (fifo_empty(&serial->rx_fifo)) // 为空则阻塞
        {
            assert(serial->rx_waiter == NULL);
            serial->rx_waiter = running_task();
            task_block(serial->rx_waiter, NULL, TASK_BLOCKED, TIMELESS);
        }
        buf[nr++] = fifo_get(&serial->rx_fifo); // 不为空则读
    }
    lock_release(&serial->rlock); // 读锁释放
    return nr;
}

// 写数据
int serial_write(serial_t *serial, char *buf, u32 count)
{
    lock_acquire(&serial->wlock); // 写锁加锁
    int nr = 0;
    while (nr < count)
    {
        u8 state = inb(serial->iobase + COM_LINE_STATUS);
        if (state & LSR_THRE) // 串口可写
        {
            outb(serial->iobase + COM_DATA, buf[nr++]);
        }
        else
        {
            // 写进程阻塞处理
            serial->tx_waiter = running_task();
            task_block(serial->tx_waiter, NULL, TASK_BLOCKED, TIMELESS);
        }
    }
    lock_release(&serial->wlock); // 写锁释放
    return nr;
}

// 初始化串口
void serial_init()
{
    for (size_t i = 0; i < 2; i++)
    {
        serial_t *serial = &serials[i]; // 得到串口的结构
        fifo_init(&serial->rx_fifo, serial->rx_buf, BUF_LEN); // 初始化队列
        serial->rx_waiter = NULL; // 进程置为空
        lock_init(&serial->rlock); // 读锁
        serial->tx_waiter = NULL; // 进程置为空
        lock_init(&serial->wlock); // 写锁

        if (i == 0)
        {
            serial->iobase = COM1_IOBASE; // 中断号：4
            set_interrupt_handler(IRQ_SERIAL_1, serial_handler); // 串口 1 的基地址
        }
        else
        {
            serial->iobase = COM2_IOBASE; // 中断号：3
            set_interrupt_handler(IRQ_SERIAL_2, serial_handler); // 串口 2 的基地址
        }

        // 初始化串口
        outb(serial->iobase + COM_INTR_ENABLE, 0); // 禁用中断

        outb(serial->iobase + COM_LINE_CONTROL, 0x80); // 激活 DLAB

        outb(serial->iobase + COM_BAUD_LSB, 0x30); // 设置波特率因子低字节
        outb(serial->iobase + COM_BAUD_MSB, 0x00); // 设置波特率因子高字节

        outb(serial->iobase + COM_LINE_CONTROL, 0x03); // 复位 DLAB 位，数据位为 8 位

        outb(serial->iobase + COM_INTR_IDENTIFY, 0xC7); // 启用 FIFO, 清空 FIFO, 14 字节触发电平

        outb(serial->iobase + COM_MODEM_CONTROL, 0b11011); // 设置回环模式，测试串口芯片

        // 收到的内容与发送的不一致，则串口不可用
        if (inb(serial->iobase + COM_DATA) != 0xAE)
        {
            continue;
        }

        outb(serial->iobase + COM_MODEM_CONTROL, 0b1011); // 设置回原来的模式

        // 打开中断屏蔽
        set_interrupt_mask(i == 0 ? IRQ_SERIAL_1 : IRQ_SERIAL_2, true);

        // 启用中断
        outb(serial->iobase + COM_INTR_ENABLE, 0x0F); // 数据可用 + 中断/错误 + 状态变化都发送中断

        char name[16];
        snprintf(name, sizeof(name), "com%d", i + 1);

        device_install(
            DEV_CHAR, DEV_SERIAL, serial, name, 0,
            NULL, serial_read, serial_write);

        LOGK("Serial 0x%x init...\n", serial->iobase);
    }
}