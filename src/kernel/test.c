#include "hyc.h"

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

int sys_test()
{
    // 查找设备
    device_t *cd_device = device_find(DEV_IDE_CD, 0);
    if (!cd_device)
        return 0;

    // 分配一页内存并初始化为零
    void *buffer = (void *)alloc_kpage(1);
    if (!buffer) // 确保内存分配成功
        return -ENOMEM;
    
    memset(buffer, 0, PAGE_SIZE);

    // 从设备读取数据
    if (device_read(cd_device->dev, buffer, 2, 0, 0) < 0)
    {
        free_kpage((u32)buffer, 1);
        return -EIO; // 如果读取失败，返回输入/输出错误
    }

    // 释放分配的内存页
    free_kpage((u32)buffer, 1);
    
    return EOK;
}
