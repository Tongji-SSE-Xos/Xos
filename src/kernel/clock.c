#include "hyc.h"

// 定义PIT（可编程间隔计时器）寄存器地址
#define PIT_CHANNEL_0 0x40
#define PIT_CHANNEL_2 0x42
#define PIT_CONTROL 0x43

// 时钟频率和定时器设置
#define SYSTEM_FREQUENCY_HZ 100
#define OSCILLATOR_FREQUENCY 1193182
#define CLOCK_TICK (OSCILLATOR_FREQUENCY / SYSTEM_FREQUENCY_HZ)
#define JIFFY_DURATION_MS (1000 / SYSTEM_FREQUENCY_HZ)

// 蜂鸣器相关设置
#define SPEAKER_CONTROL 0x61
#define DEFAULT_BEEP_HZ 440
#define BEEP_TICK (OSCILLATOR_FREQUENCY / DEFAULT_BEEP_HZ)
#define BEEP_DURATION_MS 100

// 定义系统时钟变量
volatile u32 jiffies = 0; // 系统时钟中断次数
u32 jiffy = JIFFY_DURATION_MS; // 每次中断的时间间隔

// 蜂鸣器状态标志
volatile bool is_beeping = false;

// 启动蜂鸣器
void start_beep()
{
    if (!is_beeping)
    {
        // 打开蜂鸣器
        outb(SPEAKER_CONTROL, inb(SPEAKER_CONTROL) | 0b11);
        is_beeping = true;

        // 让任务睡眠指定的时间
        task_sleep(BEEP_DURATION_MS);

        // 关闭蜂鸣器
        outb(SPEAKER_CONTROL, inb(SPEAKER_CONTROL) & 0xfc);
        is_beeping = false;
    }
}

// 时钟中断处理程序
void clock_handler(int vector)
{
    assert(vector == 0x20); // 确认中断向量为时钟中断
    send_eoi(vector); // 发送中断结束信号

    jiffies++; // 增加时钟计数
    // DEBUGK("clock jiffies %d ...\n", jiffies);

    timer_wakeup(); // 唤醒任何等待的定时器任务

    task_t *current_task = running_task();
    assert(current_task->magic == ONIX_MAGIC);

    current_task->jiffies = jiffies;
    current_task->ticks--;
    if (current_task->ticks == 0)
    {
        schedule(); // 调度下一个任务
    }
}

// 系统启动时间（外部变量）
extern u32 startup_time;

// 获取当前系统时间
time_t sys_time()
{
    return startup_time + (jiffies * JIFFY_DURATION_MS) / 1000;
}

// 初始化PIT计时器
void pit_init()
{
    // 设置通道0为计时器
    outb(PIT_CONTROL, 0b00110100);
    outb(PIT_CHANNEL_0, CLOCK_TICK & 0xff);
    outb(PIT_CHANNEL_0, (CLOCK_TICK >> 8) & 0xff);

    // 设置通道2为蜂鸣器
    outb(PIT_CONTROL, 0b10110110);
    outb(PIT_CHANNEL_2, (u8)BEEP_TICK);
    outb(PIT_CHANNEL_2, (u8)(BEEP_TICK >> 8));
}

// 初始化时钟
void clock_init()
{
    pit_init(); // 初始化PIT
    set_interrupt_handler(IRQ_CLOCK, clock_handler); // 设置时钟中断处理程序
    set_interrupt_mask(IRQ_CLOCK, true); // 启用时钟中断
}
