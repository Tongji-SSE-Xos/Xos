#include "hyc.h"

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define ENTRY_SIZE 0x30

#define PIC_M_CTRL 0x20 // 主片的控制端口
#define PIC_M_DATA 0x21 // 主片的数据端口
#define PIC_S_CTRL 0xa0 // 从片的控制端口
#define PIC_S_DATA 0xa1 // 从片的数据端口
#define PIC_EOI 0x20    // 通知中断控制器中断结束

extern handler_t handler_entry_table[ENTRY_SIZE];
extern void syscall_handler();
extern void page_fault();

gate_t idt[IDT_SIZE];
pointer_t idt_ptr;
handler_t handler_table[IDT_SIZE];

static const char *messages[] = {
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

// 通知中断控制器，中断处理结束
static void send_eoi(int vector) {
    if (vector >= 0x20 && vector < 0x30) {
        outb(PIC_M_CTRL, PIC_EOI);
        if (vector >= 0x28) {
            outb(PIC_S_CTRL, PIC_EOI);
        }
    }
}

// 注册异常处理函数
void set_exception_handler(u32 intr, handler_t handler)
{
    assert(intr <= 17 && intr >= 0);
    handler_table[intr] = handler;
}

// 注册中断处理函数
void set_interrupt_handler(u32 irq, handler_t handler)
{
    assert(irq < 16 && irq >= 0);
    handler_table[IRQ_MASTER_NR + irq] = handler;
}

void set_interrupt_mask(u32 irq, bool enable)
{
    assert(irq < 16 && irq >= 0);
    u16 port = (irq < 8) ? PIC_M_DATA : PIC_S_DATA;  // 端口选择
    irq = (irq < 8) ? irq : irq - 8;

    u8 mask = inb(port);  // 获取当前屏蔽位
    mask = enable ? (mask & ~(1 << irq)) : (mask | (1 << irq));  // 根据 enable 设置屏蔽位
    outb(port, mask);
}

// 清除 IF 位，返回设置之前的值
bool interrupt_disable()
{
    asm volatile(
        "pushfl\n"        // 将当前 eflags 压入栈中
        "cli\n"           // 清除 IF 位，此时外中断已被屏蔽
        "popl %eax\n"     // 将刚才压入的 eflags 弹出到 eax
        "shrl $9, %eax\n" // 将 eax 右移 9 位，得到 IF 位
        "andl $1, %eax\n" // 只需要 IF 位
    );
}

// 获得 IF 位
bool get_interrupt_state()
{
    asm volatile(
        "pushfl\n"        // 将当前 eflags 压入栈中
        "popl %eax\n"     // 将压入的 eflags 弹出到 eax
        "shrl $9, %eax\n" // 将 eax 右移 9 位，得到 IF 位
        "andl $1, %eax\n" // 只需要 IF 位
    );
}

// 设置 IF 位，根据状态启用或禁用中断
void set_interrupt_state(bool state) {
    asm volatile(state ? "sti\n" : "cli\n");  // 根据 state 直接启用或禁用中断
}

// 默认中断处理程序
static void default_handler(int vector) {
    send_eoi(vector);
    DEBUGK("[%x] default interrupt called...\n", vector);
}

void exception_handler(
    int vector,
    u32 edi, u32 esi, u32 ebp, u32 esp,
    u32 ebx, u32 edx, u32 ecx, u32 eax,
    u32 gs, u32 fs, u32 es, u32 ds,
    u32 vector0, u32 error, u32 eip, u32 cs, u32 eflags)
{
    // 异常信息获取逻辑
    const char *message = (vector < 22) ? messages[vector] : messages[15];

    printk("\nEXCEPTION : %s \n", message);
    printk("   VECTOR : 0x%02X\n", vector);
    printk("    ERROR : 0x%08X\n", error);
    printk("   EFLAGS : 0x%08X\n", eflags);
    printk("       CS : 0x%02X\n", cs);
    printk("      EIP : 0x%08X\n", eip);
    printk("      ESP : 0x%08X\n", esp);

    while (true) {} // 阻塞
}

// 初始化中断控制器
static void pic_init() {
    // 初始化主片
    outb(PIC_M_CTRL, 0x11); // ICW1: 边沿触发, 级联8259, 需要ICW4
    outb(PIC_M_DATA, 0x20); // ICW2: 起始中断向量号0x20
    outb(PIC_M_DATA, 0x04); // ICW3: IR2接从片
    outb(PIC_M_DATA, 0x01); // ICW4: 8086模式, 正常EOI

    // 初始化从片
    outb(PIC_S_CTRL, 0x11); // ICW1: 边沿触发, 级联8259, 需要ICW4
    outb(PIC_S_DATA, 0x28); // ICW2: 起始中断向量号0x28
    outb(PIC_S_DATA, 0x02); // ICW3: 设置从片连接到主片的 IR2 引脚
    outb(PIC_S_DATA, 0x01); // ICW4: 8086模式, 正常EOI

    // 关闭所有中断
    outb(PIC_M_DATA, 0xFF);
    outb(PIC_S_DATA, 0xFF);
}

// 初始化中断描述符表 (IDT)
static void idt_init()
{
    // 初始化中断门
    for (size_t i = 0; i < ENTRY_SIZE; i++) {
        gate_t *gate = &idt[i];
        handler_t handler = handler_entry_table[i];

        gate->offset0 = (u32)handler & 0xFFFF;
        gate->offset1 = ((u32)handler >> 16) & 0xFFFF;
        gate->selector = 1 << 3; // 代码段
        gate->reserved = 0;      // 保留不用
        gate->type = 0b1110;     // 中断门
        gate->segment = 0;       // 系统段
        gate->DPL = 0;           // 内核态
        gate->present = 1;       // 有效
    }

    // 设置异常处理程序
    for (size_t i = 0; i < 0x20; i++) {
        handler_table[i] = exception_handler;
    }

    // 设置页面错误处理程序
    handler_table[0x0E] = page_fault;

    // 设置默认中断处理程序
    for (size_t i = 0x20; i < ENTRY_SIZE; i++){
        handler_table[i] = default_handler;
    }

    // 初始化系统调用门
    gate_t *syscall_gate = &idt[0x80];
    syscall_gate->offset0 = (u32)syscall_handler & 0xFFFF;
    syscall_gate->offset1 = ((u32)syscall_handler >> 16) & 0xFFFF;
    syscall_gate->selector = 1 << 3; // 代码段
    syscall_gate->reserved = 0;      // 保留不用
    syscall_gate->type = 0b1110;     // 中断门
    syscall_gate->segment = 0;       // 系统段
    syscall_gate->DPL = 3;           // 用户态
    syscall_gate->present = 1;       // 有效

    // 加载 IDT
    idt_ptr.base = (u32)idt;
    idt_ptr.limit = sizeof(idt) - 1;
    asm volatile("lidt idt_ptr\n");  // 使用内联汇编加载 IDT
}

// 中断初始化函数
void interrupt_init()
{
    pic_init(); // 初始化 8259A 中断控制器
    idt_init(); // 初始化 IDT
}
