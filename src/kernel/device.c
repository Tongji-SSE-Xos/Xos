#include "hyc.h"

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

static device_t devices[DEVICE_NR]; // 设备数组

// 获取空设备
static device_t *get_null_device()
{
    for (size_t i = 1; i < DEVICE_NR; i++)
    {
        device_t *device = &devices[i];
        if (device->type == DEV_NULL)
            return device;
    }
    panic("no more devices!!!");
}

// 控制设备
int device_ioctl(dev_t dev, int cmd, void *args, int flags)
{
    device_t *device = device_get(dev);
    if (device->ioctl)
    {
        return device->ioctl(device->ptr, cmd, args, flags);
    }
    LOGK("ioctl of device %d not implemented!!!\n", dev);
    return -ENOSYS;
}

// 读设备
int device_read(dev_t dev, void *buf, size_t count, idx_t idx, int flags)
{
    device_t *device = device_get(dev); // 获取设备
    if (device->read) // 不为空指针，则调用读函数
    {
        return device->read(device->ptr, buf, count, idx, flags);
    }
    LOGK("read of device %d not implemented!!!\n", dev);
    return -ENOSYS; // 否则报错
}

// 写设备
int device_write(dev_t dev, void *buf, size_t count, idx_t idx, int flags)
{
    device_t *device = device_get(dev); // 获取设备
    if (device->write) // 不为空指针，则调用读函数
    {
        return device->write(device->ptr, buf, count, idx, flags);
    }
    LOGK("write of device %d not implemented!!!\n", dev);
    return -ENOSYS; // 否则报错
}

// 安装设备
dev_t device_install(
    int type, int subtype,
    void *ptr, char *name, dev_t parent,
    void *ioctl, void *read, void *write)
{
    // 获取一个空的设备
    device_t *device = get_null_device();
    // 参数赋值
    device->ptr = ptr;
    device->parent = parent;
    device->type = type;
    device->subtype = subtype;
    strncpy(device->name, name, NAMELEN);
    device->ioctl = ioctl;
    device->read = read;
    device->write = write;
    // 返回设备的设备号
    return device->dev;
}

void device_init()
{
    for (size_t i = 0; i < DEVICE_NR; i++)
    {
        device_t *device = &devices[i];
        strcpy((char *)device->name, "null"); // 设备置为 null
        device->type = DEV_NULL; // 空指针
        device->subtype = DEV_NULL; // 空指针
        device->dev = i;
        device->parent = 0; // 父设备为 0
        device->ioctl = NULL; // 空指针
        device->read = NULL; // 空指针
        device->write = NULL; // 空指针

        list_init(&device->request_list);
        device->direct = DIRECT_UP;
    }
}

// 根据子类型查找设备
device_t *device_find(int subtype, idx_t idx)
{
    idx_t nr = 0;
    for (size_t i = 0; i < DEVICE_NR; i++)
    {
        if (devices[i].subtype == subtype) // 从设备表表里查出子类型是子类型中的第几个的设备
        {
            if (nr == idx)
                return &devices[i];
            nr++;
        }
    }
    return NULL;
}

// 根据设备号查找设备
device_t *device_get(dev_t dev) // 设备号
{
    assert(dev < DEVICE_NR);
    assert(devices[dev].type != DEV_NULL);  // 得到设备号对应的设备不为空指针
    return &devices[dev]; // 返回设备
}

// 执行块设备请求
static int do_request(request_t *req)
{
    LOGK("dev %d do request idx %d\n", req->dev, req->idx);

    switch (req->type)
    {
    case REQ_READ: // 执行设备读
        return device_read(req->dev, req->buf, req->count, req->idx, req->flags);
        break;
    case REQ_WRITE: // 执行设备写
        return device_write(req->dev, req->buf, req->count, req->idx, req->flags);
        break;
    default:
        panic("req type %d unknown!!!");
        break;
    }
}

// 获得下一个请求
static request_t *request_nextreq(device_t *device, request_t *req)
{
    list_t *list = &device->request_list;

    if ((device->direct == DIRECT_UP && req->node.next == &list->tail) ||
        (device->direct == DIRECT_DOWN && req->node.prev == &list->head))
    {  // 如果“向上”且下一个请求到结尾则改成“向下”
       // 如果“向下”且下一个请求到开头则改成“向上”
        device->direct = (device->direct == DIRECT_UP) ? DIRECT_DOWN : DIRECT_UP;
    }

    // 得到 next
    list_node_t *next = (device->direct == DIRECT_UP) ? req->node.next : req->node.prev;

    if (next == &list->head || next == &list->tail)
    {
        return NULL; // 结束
    }

    return element_entry(request_t, node, next); // 得到请求，返回指针
}

// 块设备请求
// "dev"：访问的设备 
// "*buf"：缓冲区
// "count"：扇区数量
// "idx"：扇区开始的位置
// "type"：访问的类型
err_t device_request(dev_t dev, void *buf, u8 count, idx_t idx, int flags, u32 type)
{
    device_t *device = device_get(dev);
    assert(device->type = DEV_BLOCK); // 是块设备
    idx_t offset = idx + device_ioctl(device->dev, DEV_CMD_SECTOR_START, 0, 0); // "offset"：磁盘扇区的位置

    if (device->parent) // 如果有父设备则找到其磁盘，对磁盘进行操作
    {
        device = device_get(device->parent);
    }

    // 申请一块内存用来存放请求的参数
    request_t *req = kmalloc(sizeof(request_t));
    memset(req, 0, sizeof(request_t));

    // 给参数赋值
    req->dev = device->dev;
    req->buf = buf;
    req->count = count;
    req->idx = offset;
    req->flags = flags;
    req->type = type;
    req->task = NULL;

    LOGK("dev %d request idx %d\n", req->dev, req->idx);

    // 判断列表是否为空
    bool empty = list_empty(&device->request_list);

    // 将请求插入链表
    list_insert_sort(&device->request_list, &req->node, element_node_offset(request_t, node, idx));

    // 如果列表不为空，则阻塞，因为已经有请求在处理了，等待处理完成；
    if (!empty)
    {
        req->task = running_task();
        assert(task_block(req->task, NULL, TASK_BLOCKED, TIMELESS) == EOK);
    }

    // 执行请求
    err_t ret = do_request(req);

    request_t *nextreq = request_nextreq(device, req);

    // 将 node 从链表中移除
    list_remove(&req->node);
    kfree(req);

    if (nextreq)
    {
        assert(nextreq->task->magic == ONIX_MAGIC);
        task_unblock(nextreq->task, EOK); // 唤醒进程
    }

    return ret;
}