#include "hyc.h"

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)
// #define LOGK(fmt, args...)

#ifdef ONIX_DEBUG
#define USER_MEMORY true
#else
#define USER_MEMORY false
#endif

#define ZONE_VALID 1    // 表示可用的内存区域
#define ZONE_RESERVED 2 // 表示保留的内存区域

#define IDX(addr) ((u32)addr >> 12)            // 计算页索引
#define DIDX(addr) (((u32)addr >> 22) & 0x3ff) // 计算页目录索引
#define TIDX(addr) (((u32)addr >> 12) & 0x3ff) // 计算页表索引
#define PAGE(idx) ((u32)idx << 12)             // 根据索引计算页的起始地址
#define ASSERT_PAGE(addr) assert((addr & 0xfff) == 0)

#define PDE_MASK 0xFFC00000

// 内核页表的索引
static u32 KERNEL_PAGE_TABLE[] = {
    0x2000,
    0x3000,
    0x4000,
    0x5000,
};

#define KERNEL_MAP_BITS 0x6000

bitmap_t kernel_map;

typedef struct ards_t
{
    u64 base; // 内存基地址
    u64 size; // 内存大小
    u32 type; // 区域类型
} _packed ards_t;

static u32 memory_base = 0; // 可用内存的基地址，通常为 1M
static u32 memory_size = 0; // 可用内存的大小
static u32 total_pages = 0; // 总页数
static u32 free_pages = 0;  // 空闲页数

#define used_pages (total_pages - free_pages) // 已用页数

void memory_init(u32 magic, u32 addr)
{
    u32 count = 0;

    // 当内核由 onix loader 启动时
    if (magic == ONIX_MAGIC)
    {
        count = *(u32 *)addr;
        ards_t *ptr = (ards_t *)(addr + 4);

        for (size_t i = 0; i < count; i++, ptr++)
        {
            LOGK("Memory base 0x%p size 0x%p type %d\n",
                 (u32)ptr->base, (u32)ptr->size, (u32)ptr->type);
            if (ptr->type == ZONE_VALID && ptr->size > memory_size)
            {
                memory_base = (u32)ptr->base;
                memory_size = (u32)ptr->size;
            }
        }
    }
    else if (magic == MULTIBOOT2_MAGIC)
    {
        u32 size = *(unsigned int *)addr;
        multi_tag_t *tag = (multi_tag_t *)(addr + 8);

        LOGK("Announced mbi size 0x%x\n", size);
        while (tag->type != MULTIBOOT_TAG_TYPE_END)
        {
            if (tag->type == MULTIBOOT_TAG_TYPE_MMAP)
                break;
            // 下一个 tag 需要对齐到 8 字节
            tag = (multi_tag_t *)((u32)tag + ((tag->size + 7) & ~7));
        }

        multi_tag_mmap_t *mtag = (multi_tag_mmap_t *)tag;
        multi_mmap_entry_t *entry = mtag->entries;
        while ((u32)entry < (u32)tag + tag->size)
        {
            LOGK("Memory base 0x%p size 0x%p type %d\n",
                 (u32)entry->addr, (u32)entry->len, (u32)entry->type);
            count++;
            if (entry->type == ZONE_VALID && entry->len > memory_size)
            {
                memory_base = (u32)entry->addr;
                memory_size = (u32)entry->len;
            }
            entry = (multi_mmap_entry_t *)((u32)entry + mtag->entry_size);
        }
    }
    else
    {
        panic("Memory init magic unknown 0x%p\n", magic);
    }

    LOGK("ARDS count %d\n", count);
    LOGK("Memory base 0x%p\n", (u32)memory_base);
    LOGK("Memory size 0x%p\n", (u32)memory_size);

    assert(memory_base == MEMORY_BASE); // 内存的起始位置应为 1M
    assert((memory_size & 0xfff) == 0); // 确保页对齐

    total_pages = IDX(memory_size) + IDX(MEMORY_BASE);
    free_pages = IDX(memory_size);

    LOGK("Total pages %d\n", total_pages);
    LOGK("Free pages %d\n", free_pages);

    if (memory_size < KERNEL_MEMORY_SIZE)
    {
        panic("System memory is %dM too small, at least %dM needed\n",
              memory_size / MEMORY_BASE, KERNEL_MEMORY_SIZE / MEMORY_BASE);
    }
}

static u32 start_page = 0;   // 可分配物理内存的起始页
static u8 *memory_map;       // 物理内存映射数组
static u32 memory_map_pages; // 物理内存映射数组占用的页数

void memory_map_init()
{
    // 初始化物理内存映射数组
    memory_map = (u8 *)memory_base;

    // 计算物理内存映射数组所占用的页数
    memory_map_pages = div_round_up(total_pages, PAGE_SIZE);
    LOGK("Memory map page count %d\n", memory_map_pages);

    free_pages -= memory_map_pages;

    // 清空物理内存映射数组
    memset((void *)memory_map, 0, memory_map_pages * PAGE_SIZE);

    // 1M 以下的内存区域以及物理内存映射数组所占的内存已被占用
    start_page = IDX(MEMORY_BASE) + memory_map_pages;
    for (size_t i = 0; i < start_page; i++)
    {
        memory_map[i] = 1;
    }

    LOGK("Total pages %d free pages %d\n", total_pages, free_pages);

    // 初始化内核虚拟内存位图，确保 8 位对齐
    u32 length = (IDX(KERNEL_RAMDISK_MEM) - IDX(MEMORY_BASE)) / 8;
    bitmap_init(&kernel_map, (u8 *)KERNEL_MAP_BITS, length, IDX(MEMORY_BASE));
    bitmap_scan(&kernel_map, memory_map_pages);
}

// 分配一页物理内存
static u32 get_page()
{
    for (size_t i = start_page; i < total_pages; i++)
    {
        // 如果该页未被占用
        if (!memory_map[i])
        {
            memory_map[i] = 1;
            assert(free_pages > 0);
            free_pages--;
            u32 page = PAGE(i);
            LOGK("GET page 0x%p\n", page);
            return page;
        }
    }
    panic("Out of Memory!!!");
}

// 释放一页物理内存
static void put_page(u32 addr)
{
    ASSERT_PAGE(addr);

    u32 idx = IDX(addr);

    // 确保地址在有效范围内
    assert(idx >= start_page && idx < total_pages);

    // 确保页面已被分配至少一次
    assert(memory_map[idx] >= 1);

    // 减少页面引用计数
    memory_map[idx]--;

    // 如果页面引用计数为零，增加空闲页计数
    if (!memory_map[idx])
    {
        free_pages++;
    }

    // 确保空闲页数在合理范围内
    assert(free_pages > 0 && free_pages < total_pages);
    LOGK("PUT page 0x%p\n", addr);
}

// 读取 cr2 寄存器值
u32 get_cr2()
{
    u32 cr2_value;
    // 使用内联汇编读取 cr2 寄存器值
    asm volatile("movl %%cr2, %0\n" : "=r"(cr2_value));
    return cr2_value;
}

// 读取 cr3 寄存器值
u32 get_cr3()
{
    u32 cr3_value;
    // 使用内联汇编读取 cr3 寄存器值
    asm volatile("movl %%cr3, %0\n" : "=r"(cr3_value));
    return cr3_value;
}

// 设置 cr3 寄存器，传入页目录地址
void set_cr3(u32 pde)
{
    ASSERT_PAGE(pde);
    // 使用内联汇编设置 cr3 寄存器
    asm volatile("movl %0, %%cr3\n" ::"r"(pde));
}

// 启用分页功能，设置 cr0 寄存器的 PG 位
static _inline void enable_page()
{
    asm volatile(
        "movl %%cr0, %%eax\n"
        "orl $0x80000000, %%eax\n"
        "movl %%eax, %%cr0\n" ::
            : "eax");
}

// 初始化页表项
static void entry_init(page_entry_t *entry, u32 index)
{
    *(u32 *)entry = 0;
    entry->present = 1;
    entry->write = 1;
    entry->user = 1;
    entry->index = index;
}

// 初始化内存映射
void mapping_init()
{
    page_entry_t *pde = (page_entry_t *)KERNEL_PAGE_DIR;
    memset(pde, 0, PAGE_SIZE);

    idx_t index = 0;

    for (idx_t didx = 0; didx < (sizeof(KERNEL_PAGE_TABLE) / 4); didx++)
    {
        page_entry_t *pte = (page_entry_t *)KERNEL_PAGE_TABLE[didx];
        memset(pte, 0, PAGE_SIZE);

        page_entry_t *dentry = &pde[didx];
        entry_init(dentry, IDX((u32)pte));
        dentry->user = USER_MEMORY; // 只能被内核访问

        for (idx_t tidx = 0; tidx < 1024; tidx++, index++)
        {
            if (index == 0)
                continue;

            page_entry_t *tentry = &pte[tidx];
            entry_init(tentry, index);
            tentry->user = USER_MEMORY; // 只能被内核访问
            if (memory_map[index] == 0)
                free_pages--;
            memory_map[index] = 1; // 设置物理内存数组，该页被占用
        }
    }

    page_entry_t *entry = &pde[1023];
    entry_init(entry, IDX(KERNEL_PAGE_DIR));

    set_cr3((u32)pde);
    enable_page();
}

// 获取页目录
static page_entry_t *get_pde()
{
    return (page_entry_t *)(0xfffff000);
}

// 获取虚拟地址 vaddr 对应的页表
static page_entry_t *get_pte(u32 vaddr, bool create)
{
    page_entry_t *pde = get_pde();
    u32 idx = DIDX(vaddr);
    page_entry_t *entry = &pde[idx];

    assert(create || (!create && entry->present));

    page_entry_t *table = (page_entry_t *)(PDE_MASK | (idx << 12));

    if (!entry->present)
    {
        LOGK("Get and create page table entry for 0x%p\n", vaddr);
        u32 page = get_page();
        entry_init(entry, IDX(page));
        memset(table, 0, PAGE_SIZE);
    }

    return table;
}

page_entry_t *get_entry(u32 vaddr, bool create)
{
    page_entry_t *pte = get_pte(vaddr, create);
    return &pte[TIDX(vaddr)];
}

// 获取虚拟地址 vaddr 对应的物理地址
u32 get_paddr(u32 vaddr)
{
    page_entry_t *pde = get_pde();
    page_entry_t *entry = &pde[DIDX(vaddr)];
    if (!entry->present)
        return 0;

    entry = get_entry(vaddr, false);
    if (!entry->present)
        return 0;

    return PAGE(entry->index) | (vaddr & 0xfff);
}

// 刷新虚拟地址 vaddr 的 TLB
void flush_tlb(u32 vaddr)
{
    asm volatile("invlpg (%0)" ::"r"(vaddr)
                 : "memory");
}

// 在位图中扫描 count 个连续的页
static u32 scan_page(bitmap_t *map, u32 count)
{
    assert(count > 0);
    int32 index = bitmap_scan(map, count);

    if (index == EOF)
    {
        panic("Scan page fail!!!");
    }

    u32 addr = PAGE(index);
    LOGK("Scan page 0x%p count %d\n", addr, count);
    return addr;
}


// 重置页位图中与给定地址相关的页面
static void reset_page(bitmap_t *map, u32 addr, u32 count)
{
    ASSERT_PAGE(addr);
    assert(count > 0);
    u32 index = IDX(addr);

    for (size_t i = 0; i < count; i++)
    {
        assert(bitmap_test(map, index + i));
        bitmap_set(map, index + i, 0);
    }
}

// 分配 count 个连续的内核页
u32 alloc_kpage(u32 count)
{
    assert(count > 0);
    u32 vaddr = scan_page(&kernel_map, count);
    LOGK("ALLOC kernel pages 0x%p count %d\n", vaddr, count);
    return vaddr;
}

// 释放 count 个连续的内核页
void free_kpage(u32 vaddr, u32 count)
{
    ASSERT_PAGE(vaddr);
    assert(count > 0);
    reset_page(&kernel_map, vaddr, count);
    LOGK("FREE kernel pages 0x%p count %d\n", vaddr, count);
}

// 拷贝一页，返回该页的物理地址
static u32 copy_page(void *page)
{
    u32 paddr = get_page();
    u32 vaddr = 0;

    page_entry_t *entry = get_pte(vaddr, false);
    entry_init(entry, IDX(paddr));
    flush_tlb(vaddr);

    memcpy((void *)vaddr, page, PAGE_SIZE);

    entry->present = false;
    flush_tlb(vaddr);

    return paddr;
}

// 页表写时拷贝
// vaddr 表示虚拟地址
// level 表示层级，页目录，页表，页框
void copy_on_write(u32 vaddr, int level)
{
    if (level == 0)
        return;

    page_entry_t *entry = get_entry(vaddr, false);
    copy_on_write((u32)entry, level - 1);

    if (entry->write)
        return;

    assert(memory_map[entry->index] > 0);

    if (memory_map[entry->index] == 1)
    {
        entry->write = true;
        LOGK("WRITE page for 0x%p\n", vaddr);
    }
    else
    {
        u32 paddr = copy_page((void *)PAGE(IDX(vaddr)));

        memory_map[entry->index]--;

        entry->index = IDX(paddr);
        entry->write = true;
        LOGK("COPY page for 0x%p\n", vaddr);
    }

    assert(memory_map[entry->index] > 0);
    flush_tlb(vaddr);
}

// 将 vaddr 映射到物理内存
void link_page(u32 vaddr)
{
    ASSERT_PAGE(vaddr);

    page_entry_t *entry = get_entry(vaddr, true);

    u32 index = IDX(vaddr);

    if (entry->present)
    {
        return;
    }

    copy_on_write((u32)entry, 2);

    u32 paddr = get_page();
    entry_init(entry, IDX(paddr));
    flush_tlb(vaddr);

    LOGK("LINK from 0x%p to 0x%p\n", vaddr, paddr);
}

// 去除 vaddr 对应的物理内存映射
void unlink_page(u32 vaddr)
{
    ASSERT_PAGE(vaddr);

    page_entry_t *pde = get_pde();
    page_entry_t *entry = &pde[DIDX(vaddr)];
    if (!entry->present)
        return;

    entry = get_entry(vaddr, false);
    if (!entry->present)
    {
        return;
    }

    copy_on_write((u32)entry, 2);

    entry->present = false;

    u32 paddr = PAGE(entry->index);

    DEBUGK("UNLINK from 0x%p to 0x%p\n", vaddr, paddr);
    put_page(paddr);

    flush_tlb(vaddr);
}

// 映射虚拟地址 vaddr 到物理地址 paddr
void map_page(u32 vaddr, u32 paddr)
{
    ASSERT_PAGE(vaddr);
    ASSERT_PAGE(paddr);

    page_entry_t *entry = get_entry(vaddr, true);

    if (entry->present)
    {
        return;
    }

    if (!paddr)
    {
        paddr = get_page();
    }

    entry_init(entry, IDX(paddr));
    flush_tlb(vaddr);
}

// 将一个区域映射到物理内存
void map_area(u32 paddr, u32 size)
{
    ASSERT_PAGE(paddr);
    u32 page_count = div_round_up(size, PAGE_SIZE);
    for (size_t i = 0; i < page_count; i++)
    {
        map_page(paddr + i * PAGE_SIZE, paddr + i * PAGE_SIZE);
    }
    LOGK("MAP memory 0x%p size 0x%X\n", paddr, size);
}


// 复制当前页目录
page_entry_t *copy_pde()
{
    task_t *task = running_task();

    page_entry_t *pde = (page_entry_t *)task->pde;
    page_entry_t *dentry = NULL;
    page_entry_t *entry = NULL;

    for (size_t didx = (sizeof(KERNEL_PAGE_TABLE) / 4); didx < (USER_STACK_TOP >> 22); didx++)
    {
        dentry = &pde[didx];
        if (!dentry->present)
            continue;

        // 将页表项设置为只读
        assert(memory_map[dentry->index] > 0);
        dentry->write = false;
        memory_map[dentry->index]++;
        assert(memory_map[dentry->index] < 255);

        page_entry_t *pte = (page_entry_t *)(PDE_MASK | (didx << 12));

        for (size_t tidx = 0; tidx < 1024; tidx++)
        {
            entry = &pte[tidx];
            if (!entry->present)
                continue;

            // 确保物理内存引用大于 0
            assert(memory_map[entry->index] > 0);

            // 如果不是共享内存，将其设置为只读
            if (!entry->shared)
            {
                entry->write = false;
            }
            // 增加物理内存引用计数
            memory_map[entry->index]++;

            assert(memory_map[entry->index] < 255);
        }
    }

    pde = (page_entry_t *)alloc_kpage(1);
    memcpy(pde, (void *)task->pde, PAGE_SIZE);

    // 将页表的最后一项指向页目录本身，方便后续修改
    entry = &pde[1023];
    entry_init(entry, IDX(pde));

    set_cr3(task->pde);

    return pde;
}

// 释放当前页目录
void free_pde()
{
    task_t *task = running_task();
    assert(task->uid != KERNEL_USER);

    page_entry_t *pde = get_pde();

    for (size_t didx = (sizeof(KERNEL_PAGE_TABLE) / 4); didx < (USER_STACK_TOP >> 22); didx++)
    {
        page_entry_t *dentry = &pde[didx];
        if (!dentry->present)
        {
            continue;
        }

        page_entry_t *pte = (page_entry_t *)(PDE_MASK | (didx << 12));

        for (size_t tidx = 0; tidx < 1024; tidx++)
        {
            page_entry_t *entry = &pte[tidx];
            if (!entry->present)
            {
                continue;
            }

            assert(memory_map[entry->index] > 0);
            put_page(PAGE(entry->index));
        }

        // 释放页表内存
        put_page(PAGE(dentry->index));
    }

    // 释放页目录内存
    free_kpage(task->pde, 1);
    LOGK("free pages %d\n", free_pages);
}

int32 sys_brk(void *addr)
{
    LOGK("task brk 0x%p\n", addr);
    u32 brk = (u32)addr;
    ASSERT_PAGE(brk);

    task_t *task = running_task();
    assert(task->uid != KERNEL_USER);

    assert(task->end <= brk && brk <= USER_MMAP_ADDR);

    u32 old_brk = task->brk;

    if (old_brk > brk)
    {
        for (u32 page = brk; page < old_brk; page += PAGE_SIZE)
        {
            unlink_page(page);
        }
    }
    else if (IDX(brk - old_brk) > free_pages)
    {
        // 内存不足
        return -1;
    }

    task->brk = brk;
    return 0;
}

void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    ASSERT_PAGE((u32)addr);

    u32 count = div_round_up(length, PAGE_SIZE);
    u32 vaddr = (u32)addr;

    task_t *task = running_task();
    if (!vaddr)
    {
        vaddr = scan_page(task->vmap, count);
    }

    assert(vaddr >= USER_MMAP_ADDR && vaddr < USER_STACK_BOTTOM);

    for (size_t i = 0; i < count; i++)
    {
        u32 page = vaddr + PAGE_SIZE * i;
        link_page(page);
        bitmap_set(task->vmap, IDX(page), true);

        page_entry_t *entry = get_entry(page, false);
        entry->user = true;
        entry->write = false;
        entry->readonly = true;

        if (prot & PROT_WRITE)
        {
            entry->readonly = false;
            entry->write = true;
        }
        if (flags & MAP_SHARED)
        {
            entry->shared = true;
        }
        if (flags & MAP_PRIVATE)
        {
            entry->privat = true;
        }
        flush_tlb(page);
    }

    if (fd != EOF)
    {
        lseek(fd, offset, SEEK_SET);
        read(fd, (char *)vaddr, length);
    }

    return (void *)vaddr;
}

int sys_munmap(void *addr, size_t length)
{
    task_t *task = running_task();
    u32 vaddr = (u32)addr;
    assert(vaddr >= USER_MMAP_ADDR && vaddr < USER_STACK_BOTTOM);

    ASSERT_PAGE(vaddr);
    u32 count = div_round_up(length, PAGE_SIZE);

    for (size_t i = 0; i < count; i++)
    {
        u32 page = vaddr + PAGE_SIZE * i;
        unlink_page(page);
        assert(bitmap_test(task->vmap, IDX(page)));
        bitmap_clear(task->vmap, IDX(page));
    }

    return 0;
}

typedef struct page_error_code_t
{
    u8 present : 1;
    u8 write : 1;
    u8 user : 1;
    u8 reserved0 : 1;
    u8 fetch : 1;
    u8 protection : 1;
    u8 shadow : 1;
    u16 reserved1 : 8;
    u8 sgx : 1;
    u16 reserved2;
} __attribute__((packed)) page_error_code_t;

void page_fault(
    u32 vector,
    u32 edi, u32 esi, u32 ebp, u32 esp,
    u32 ebx, u32 edx, u32 ecx, u32 eax,
    u32 gs, u32 fs, u32 es, u32 ds,
    u32 vector0, u32 error, u32 eip, u32 cs, u32 eflags)
{
    assert(vector == 0xe);
    u32 fault_addr = get_cr2();
    LOGK("Fault address: 0x%p, EIP: 0x%p\n", fault_addr, eip);

    page_error_code_t *code = (page_error_code_t *)&error;
    task_t *task = running_task();

    if (fault_addr < USER_EXEC_ADDR || fault_addr >= USER_STACK_TOP)
    {
        assert(task->uid);
        printk("Segmentation Fault!\n");
        task_exit(-1);
    }

    if (code->present)
    {
        assert(code->write);

        page_entry_t *entry = get_entry(fault_addr, false);

        assert(entry->present);
        assert(!entry->shared);
        assert(!entry->readonly);

        copy_on_write(fault_addr, 3);

        return;
    }

    if (!code->present && (fault_addr < task->brk || fault_addr >= USER_STACK_BOTTOM))
    {
        u32 page = PAGE(IDX(fault_addr));
        link_page(page);
        return;
    }

    LOGK("Task 0x%p (%s) encountered a page fault at 0x%p\n", task, task->name, fault_addr);
    panic("Page Fault!");
}

bool memory_access(void *vaddr, int size, bool write, bool user)
{
    u32 page = PAGE(IDX(vaddr));
    u32 end = (u32)(vaddr) + size;

    page_entry_t *entry;
    for (size_t i = 0; page < end; i++, page += PAGE_SIZE)
    {
        page_entry_t *pde = get_pde();
        idx_t idx = DIDX(page);
        entry = &pde[idx];
        if (!entry->present)
            return false;

        page_entry_t *ptable = (page_entry_t *)(PDE_MASK | (idx << 12));
        entry = &ptable[TIDX(page)];
        if (!entry->present)
            return false;

        if (write && entry->readonly)
            return false;

        if (user && !entry->user)
            return false;
    }
    return true;
}
