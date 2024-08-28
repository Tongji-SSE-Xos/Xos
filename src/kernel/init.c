#include "hyc.h"

// 进入用户模式并准备内核栈
// 确保栈顶具有足够的空间，类似于 char temp[100];
static void configure_stack()
{
    // 获取返回地址，即调用 user_mode_transition 的位置
    void *ret_addr = __builtin_return_address(0);
    asm volatile(
        "subl $100, %%esp\n" // 为栈顶留出足够的空间
        "pushl %%eax\n"      // 将返回地址压入栈
        "ret \n"             // 返回到调用点
        ::"a"(ret_addr));
}

extern int user_main();

int initiate_user_thread()
{
    while (true)
    {
        u32 exit_status;
        pid_t process_id = fork();
        if (process_id)
        {
            pid_t finished_pid = waitpid(process_id, &exit_status);
            printf("wait pid %d status %d %d\n", finished_pid, exit_status, time());
        }
        else
        {
            user_main();
        }
    }
    return 0;
}

extern void init_serial();
extern void init_keyboard();
extern void init_time();
extern void init_tty();
extern void init_rtc();

extern void init_ide();
extern void init_floppy();
extern void init_ramdisk();
extern void init_sb16();
extern void init_e1000();

extern void init_buffer();
extern void init_file();
extern void init_inode();
extern void init_pipe();
extern void init_minix();
extern void init_iso();
extern void init_super();
extern void init_dev();
extern void init_network();
extern void init_resolv();

void configure_thread()
{
    init_serial();   // 配置串口
    init_keyboard(); // 配置键盘
    init_time();     // 配置时间
    init_tty();      // 配置 TTY 设备，必须在键盘后初始化
    // init_rtc();   // 配置实时时钟，暂时不使用

    init_ramdisk(); // 配置内存虚拟磁盘

    init_ide();    // 配置 IDE 设备
    init_sb16();   // 配置声卡
    init_floppy(); // 配置软盘驱动器
    init_e1000();  // 配置 e1000 网卡

    init_buffer(); // 配置高速缓冲
    init_file();   // 配置文件系统
    init_inode();  // 配置 inode
    init_minix();  // 配置 minix 文件系统
    init_iso();    // 配置 iso9660 文件系统
    init_pipe();   // 配置管道
    init_super();  // 配置超级块

    init_dev();    // 配置设备文件
    init_network();    // 配置网络
    init_resolv(); // 配置域名解析

    configure_stack();     // 配置栈顶
    task_to_user_mode(); // 切换到用户模式
}
