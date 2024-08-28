#include "hyc.h"

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define ENTRY_SIZE 0x30

#define PIC_MASTER_CTRL 0x20 // 主片的控制端口
#define PIC_MASTER_DATA 0x21 // 主片的数据端口
#define PIC_SLAVE_CTRL 0xa0  // 从片的控制端口
#define PIC_SLAVE_DATA 0xa1  // 从片的数据端口
#define PIC_END_OF_INTERRUPT 0x20 // 通知中断控制器中断结束

extern handler_t handler_entry_table[ENTRY_SIZE];
extern void syscall_handler();
extern void page_fault();

gate_t idt[IDT_SIZE];
pointer_t idt_ptr;
handler_t handler_table[IDT_SIZE];

static const char *exception_messages[] = {
    "#DE Divide Error", 
    "#DB RESERVED", 
    "--  NMI Interrupt", 
    "#BP Breakpoint", 
    "#OF Overflow",
    "#BR BOUND Range Exceeded", 
    "#UD Invalid Opcode (Undefined Opcode)",
    "#NM Device Not Available",
    "#DF Double Fault",
    "Coprocessor Segment Overrun (reserved)", 
    "#TS Invalid TSS",
    "#NP Segment Not Present", 
    "#SS Stack-Segment Fault", 
    "#GP General Protection", 
    "#PF Page Fault",
    "--  (Intel reserved. Do not use.)",
    "#MF x87 FPU Floating-Point Error", 
    "#AC Alignment Check",
    "#MC Machine Check", 
    "#XF SIMD Floating-Point Exception", 
    "#VE Virtualization Exception",
    "#CP Control Protection Exception"
};

// 向中断控制器发送中断结束信号
static void send_end_of_interrupt(int vector) {
    if (vector >= 0x20 && vector < 0x30) {
        outb(PIC_MASTER_CTRL, PIC_END_OF_INTERRUPT);
        if (vector >= 0x28) {
            outb(PIC_SLAVE_CTRL, PIC_END_OF_INTERRUPT);
        }
    }
}

// 设置异常处理程序
void set_exception_handler(u32 intr, handler_t handler)
{
    assert(intr <= 17 && intr >= 0);
    handler_table[intr] = handler;
}

// 设置中断处理程序
void set_interrupt_handler(u32 irq, handler_t handler)
{
    assert(irq < 16 && irq >= 0);
    handler_table[IRQ_MASTER_NR + irq] = handler;
}

// 更新中断屏蔽位
void set_interrupt_mask(u32 irq, bool enable)
{
    assert(irq < 16 && irq >= 0);
    u16 port = (irq < 8) ? PIC_MASTER_DATA : PIC_SLAVE_DATA;  // 根据 IRQ 选择端口
    irq = (irq < 8) ? irq : irq - 8;

    u8 mask = inb(port);  // 读取当前屏蔽位
    if (enable) {
        mask &= ~(1 << irq);  // 启用中断
    } else {
        mask |= (1 << irq);  // 禁用中断
    }
    outb(port, mask);
}

// 禁用中断并返回原始中断状态
bool interrupt_disable()
{
    asm volatile(
        "pushfl\n"        // 保存 eflags
        "cli\n"           // 清除 IF 位，屏蔽外部中断
        "popl %eax\n"     // 将 eflags 弹出到 eax
        "shrl $9, %eax\n" // 右移 9 位，获取 IF 位
        "andl $1, %eax\n" // 只保留 IF 位
    );
}

// 获取当前中断状态
bool get_interrupt_state()
{
    asm volatile(
        "pushfl\n"        // 保存 eflags
        "popl %eax\n"     // 弹出 eflags 到 eax
        "shrl $9, %eax\n" // 右移 9 位，获取 IF 位
        "andl $1, %eax\n" // 只保留 IF 位
    );
}

// 设置中断状态
void set_interrupt_state(bool state) {
    asm volatile(state ? "sti\n" : "cli\n");  // 根据 state 启用或禁用中断
}

// 默认中断处理程序
static void default_handler(int vector) {
    send_end_of_interrupt(vector);
    DEBUGK("[%x] default interrupt called...\n", vector);
}

void exception_handler(
    int vector,
    u32 edi, u32 esi, u32 ebp, u32 esp,
    u32 ebx, u32 edx, u32 ecx, u32 eax,
    u32 gs, u32 fs, u32 es, u32 ds,
    u32 vector0, u32 error, u32 eip, u32 cs, u32 eflags)
{
    // 获取异常信息
    const char *message = (vector < 22) ? exception_messages[vector] : exception_messages[15];

    printk("\nEXCEPTION : %s \n", message);
    printk("   VECTOR : 0x%02X\n", vector);
    printk("    ERROR : 0x%08X\n", error);
    printk("   EFLAGS : 0x%08X\n", eflags);
    printk("       CS : 0x%02X\n", cs);
    printk("      EIP : 0x%08X\n", eip);
    printk("      ESP : 0x%08X\n", esp);

    while (true) {} // 进入死循环以阻止系统继续运行
}

// 初始化 8259A 中断控制器
static void pic_init() {
    // 初始化主片 8259A
    outb(PIC_MASTER_CTRL, 0x11); // ICW1: 边沿触发, 级联8259, 需要ICW4
    outb(PIC_MASTER_DATA, 0x20); // ICW2: 起始中断向量号0x20
    outb(PIC_MASTER_DATA, 0x04); // ICW3: IR2接从片
    outb(PIC_MASTER_DATA, 0x01); // ICW4: 8086模式, 正常EOI

    // 初始化从片 8259A
    outb(PIC_SLAVE_CTRL, 0x11); // ICW1: 边沿触发, 级联8259, 需要ICW4
    outb(PIC_SLAVE_DATA, 0x28); // ICW2: 起始中断向量号0x28
    outb(PIC_SLAVE_DATA, 0x02); // ICW3: 设置从片连接到主片的 IR2 引脚
    outb(PIC_SLAVE_DATA, 0x01); // ICW4: 8086模式, 正常EOI

    // 屏蔽所有中断
    outb(PIC_MASTER_DATA, 0xFF);
    outb(PIC_SLAVE_DATA, 0xFF);
}


// 初始化中断描述符表 (IDT)
static void idt_init() {
    // 初始化中断门
    for (size_t i = 0; i < ENTRY_SIZE; i++) {
        gate_t *gate = &idt[i];
        handler_t handler = handler_entry_table[i];

        gate->offset0 = (u32)handler & 0xFFFF;
        gate->offset1 = ((u32)handler >> 16) & 0xFFFF;
        gate->selector = 1 << 3;  // 选择器设置为代码段
        gate->reserved = 0;       // 保留位设置为0
        gate->type = 0b1110;      // 设置中断门类型
        gate->segment = 0;        // 系统段选择
        gate->DPL = 0;            // 描述符特权级为0（内核态）
        gate->present = 1;        // 标记门描述符为有效
    }

    // 设置异常处理程序
    for (size_t i = 0; i < 0x20; i++) {
        handler_table[i] = exception_handler;
    }

    // 设置页面错误处理程序
    handler_table[0x0E] = page_fault;

    // 为其余中断向量设置默认处理程序
    for (size_t i = 0x20; i < ENTRY_SIZE; i++) {
        handler_table[i] = default_handler;
    }

    // 初始化系统调用门
    gate_t *syscall_gate = &idt[0x80];
    syscall_gate->offset0 = (u32)syscall_handler & 0xFFFF;
    syscall_gate->offset1 = ((u32)syscall_handler >> 16) & 0xFFFF;
    syscall_gate->selector = 1 << 3;  // 选择器设置为代码段
    syscall_gate->reserved = 0;       // 保留位设置为0
    syscall_gate->type = 0b1110;      // 设置中断门类型
    syscall_gate->segment = 0;        // 系统段选择
    syscall_gate->DPL = 3;            // 描述符特权级为3（用户态）
    syscall_gate->present = 1;        // 标记门描述符为有效

    // 加载 IDT
    idt_ptr.base = (u32)idt;
    idt_ptr.limit = sizeof(idt) - 1;
    asm volatile("lidt idt_ptr\n");  // 加载 IDT 表地址
}

// 中断初始化函数
void interrupt_init() {
    pic_init(); // 初始化 8259A 可编程中断控制器
    idt_init(); // 初始化中断描述符表
}
