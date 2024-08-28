#include "hyc.h"

descriptor_t gdt[GDT_SIZE]; // 全局描述符表
pointer_t gdt_ptr;          // 全局描述符表指针
tss_t tss;                  // 任务状态段（TSS）

void descriptor_init(descriptor_t *desc, u32 base, u32 limit)
{
    desc->base_low = base & 0xFFFFFF;
    desc->base_high = (base >> 24) & 0xFF;
    desc->limit_low = limit & 0xFFFF;
    desc->limit_high = (limit >> 16) & 0xF;
}

// 初始化全局描述符表
void gdt_init()
{
    DEBUGK("Initializing GDT...\n");

    memset(gdt, 0, sizeof(gdt));

    descriptor_t *desc;

    desc = &gdt[KERNEL_CODE_IDX];
    descriptor_init(desc, 0, 0xFFFFF);
    desc->segment = 1;     // 标识为代码段
    desc->granularity = 1; // 使用4K分页
    desc->big = 1;         // 32位模式
    desc->long_mode = 0;   // 不启用64位模式
    desc->present = 1;     // 段在内存中
    desc->DPL = 0;         // 内核态特权级
    desc->type = 0b1010;   // 代码段类型

    desc = &gdt[KERNEL_DATA_IDX];
    descriptor_init(desc, 0, 0xFFFFF);
    desc->segment = 1;     // 标识为数据段
    desc->granularity = 1; // 使用4K分页
    desc->big = 1;         // 32位模式
    desc->long_mode = 0;   // 不启用64位模式
    desc->present = 1;     // 段在内存中
    desc->DPL = 0;         // 内核态特权级
    desc->type = 0b0010;   // 数据段类型

    desc = &gdt[USER_CODE_IDX];
    descriptor_init(desc, 0, 0xFFFFF);
    desc->segment = 1;     // 标识为代码段
    desc->granularity = 1; // 使用4K分页
    desc->big = 1;         // 32位模式
    desc->long_mode = 0;   // 不启用64位模式
    desc->present = 1;     // 段在内存中
    desc->DPL = 3;         // 用户态特权级
    desc->type = 0b1010;   // 代码段类型

    desc = &gdt[USER_DATA_IDX];
    descriptor_init(desc, 0, 0xFFFFF);
    desc->segment = 1;     // 标识为数据段
    desc->granularity = 1; // 使用4K分页
    desc->big = 1;         // 32位模式
    desc->long_mode = 0;   // 不启用64位模式
    desc->present = 1;     // 段在内存中
    desc->DPL = 3;         // 用户态特权级
    desc->type = 0b0010;   // 数据段类型

    gdt_ptr.base = (u32)&gdt;
    gdt_ptr.limit = sizeof(gdt) - 1;
}

void tss_init()
{
    memset(&tss, 0, sizeof(tss));

    tss.ss0 = KERNEL_DATA_SELECTOR;
    tss.iobase = sizeof(tss);

    descriptor_t *desc = &gdt[KERNEL_TSS_IDX];
    descriptor_init(desc, (u32)&tss, sizeof(tss) - 1);
    desc->segment = 0;     // 标识为系统段
    desc->granularity = 0; // 使用字节单位
    desc->big = 0;         // 固定为0
    desc->long_mode = 0;   // 固定为0
    desc->present = 1;     // 段在内存中
    desc->DPL = 0;         // 内核态特权级
    desc->type = 0b1001;   // 可用32位TSS

    // 加载任务状态段寄存器
    asm volatile(
        "ltr %%ax\n" ::"a"(KERNEL_TSS_SELECTOR));
}

