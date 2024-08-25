#include "../include/xos/device.h"
#include "../include/xos/syscall.h"
#include "../include/xos/stat.h"
#include "../include/xos/stdio.h"
#include "../include/xos/assert.h"
#include "../include/xos/fs.h"
#include "../include/xos/string.h"
#include "../include/xos/net.h"

extern file_t file_table[];

// 设备节点创建的通用函数
static void create_device_nodes(int dev_type, const char *prefix, mode_t mode, int start_idx) {
    device_t *device;
    char name[32];
    for (size_t i = start_idx; (device = device_find(dev_type, i)); i++) {
        sprintf(name, "%s%s", prefix, device->name);
        assert(mknod(name, mode, device->dev) == EOK);
    }
}

// 处理网络初始化
void net_init() {
    bool dhcp = true;
    fd_t netif = open("/dev/net/eth1", O_RDONLY, 0);
    if (netif < EOK) return;

    fd_t fd = open("/etc/network.conf", O_RDONLY, 0);
    if (fd < EOK) goto rollback;

    char buf[512];
    int len = read(fd, buf, sizeof(buf));
    if (len < EOK) goto rollback;

    ifreq_t req;
    char *next = buf - 1;
    while ((next = strchr(next + 1, '\n'))) {
        *next = 0;
        char *ptr = next - strlen(next) + 1;
        if (!memcmp(ptr, "ipaddr=", 7)) {
            if (inet_aton(ptr + 7, req.ipaddr) >= EOK) {
                ioctl(netif, SIOCSIFADDR, (int)&req);
                req.ipaddr[3] = 255;
                ioctl(netif, SIOCSIFBRDADDR, (int)&req);
                dhcp = false;
            }
        } else if (!memcmp(ptr, "netmask=", 8)) {
            if (inet_aton(ptr + 8, req.netmask) >= EOK)
                ioctl(netif, SIOCSIFNETMASK, (int)&req);
        } else if (!memcmp(ptr, "gateway=", 8)) {
            if (inet_aton(ptr + 8, req.gateway) >= EOK)
                ioctl(netif, SIOCSIFGATEWAY, (int)&req);
        }
    }
rollback:
    if (dhcp && netif > 0) ioctl(netif, SIOCSIFDHCPSTART, (int)&req);
    if (netif > 0) close(netif);
    if (fd > 0) close(fd);
}

// 设备初始化
void dev_init() {
    stat_t statbuf;
    if (stat("/dev", &statbuf) < 0) assert(mkdir("/dev", 0755) == EOK);

    device_t *device = device_find(DEV_RAMDISK, 0);
    assert(device);
    fs_get_op(FS_TYPE_MINIX)->mkfs(device->dev, 0);

    super_t *super = read_super(device->dev);
    assert(super && super->iroot && super->iroot->nr == 1);
    super->imount = namei("/dev");
    assert(super->imount && (super->imount->mount = device->dev));

    struct {
        int dev_type;
        const char *prefix;
        mode_t mode;
        int start_idx;
    } devs[] = {
        {DEV_CONSOLE, "/dev/console", IFCHR | 0200, 0},
        {DEV_KEYBOARD, "/dev/keyboard", IFCHR | 0400, 0},
        {DEV_TTY, "/dev/tty", IFCHR | 0600, 0},
        {DEV_IDE_DISK, "/dev/", IFBLK | 0600, 0},
        {DEV_IDE_PART, "/dev/", IFBLK | 0600, 0},
        {DEV_IDE_CD, "/dev/cd", IFBLK | 0400, 0},
        {DEV_RAMDISK, "/dev/", IFBLK | 0600, 1},
        {DEV_FLOPPY, "/dev/", IFBLK | 0600, 0},
        {DEV_SERIAL, "/dev/", IFCHR | 0600, 0},
        {DEV_SB16, "/dev/", IFCHR | 0200, 0}
    };

    for (size_t i = 0; i < sizeof(devs)/sizeof(devs[0]); i++) {
        create_device_nodes(devs[i].dev_type, devs[i].prefix, devs[i].mode, devs[i].start_idx);
    }

    assert(mkdir("/dev/net", 0755) == EOK);
    create_device_nodes(DEV_NETIF, "/dev/net/", IFSOCK | 0400, 0);

    const char *links[] = {"/dev/tty", "/dev/stdout", "/dev/stderr", "/dev/stdin"};
    for (int i = 1; i < sizeof(links)/sizeof(links[0]); i++) {
        assert(link(links[0], links[i]) == EOK);
    }

    file_t *file;
    inode_t *inode;
    const char *std_files[] = {"/dev/stdin", "/dev/stdout", "/dev/stderr"};
    int flags[] = {O_RDONLY, O_WRONLY, O_WRONLY};

    for (int i = 0; i < 3; i++) {
        file = &file_table[i];
        inode = namei(std_files[i]);
        assert(inode);
        file->inode = inode;
        file->flags = flags[i];
        file->offset = 0;
    }
}
