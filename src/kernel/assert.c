#include "hyc.h"

static u8 buf[1024];

// 强制阻塞
static void spin(const char *func_name)
{
    printk("Spinning in %s ...\n", func_name);
    while (true)
    {
        // 在此处添加适当的挂起或阻塞操作
    }
}

void assertion_failure(const char *exp, const char *file, const char *base, int line)
{
    printk(
        "\n--> Assertion failed: (%s)\n"
        "--> File: %s\n"
        "--> Base: %s\n"
        "--> Line: %d\n",
        exp, file, base, line);

    spin("assertion_failure()");

    // 不可能走到这里，否则出错；
    asm volatile("ud2");
}

void panic(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    printk("!!! PANIC !!!\n--> %s\n", buf);
    spin("panic()");

    // 不可能走到这里，否则出错；
    asm volatile("ud2");
}
