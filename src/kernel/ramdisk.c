#include "hyc.h"

#define SECTOR_SIZE 512

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define MAX_RAMDISKS 4

typedef struct {
    u8 *base_address; // 内存基地址
    u32 total_size;   // 总内存大小
} RamDisk;

static ramdisk_t ramdisks[MAX_RAMDISKS];

int handle_ramdisk_ioctl(RamDisk *disk, int command, void *args, int flags)
{
    switch (command)
    {
    case DEV_CMD_SECTOR_START:
        return 0;
    case DEV_CMD_SECTOR_COUNT:
        return disk->total_size / SECTOR_SIZE;
    case DEV_CMD_SECTOR_SIZE:
        return SECTOR_SIZE;
    default:
        panic("Unrecognized device command: %d", command);
    }
}

int ramdisk_read(RamDisk *disk, void *buffer, u8 sector_count, idx_t lba)
{
    void *address = disk->base_address + lba * SECTOR_SIZE;
    u32 length = sector_count * SECTOR_SIZE;
    assert(((u32)address + length) < (KERNEL_RAMDISK_MEM + KERNEL_MEMORY_SIZE));
    memcpy(buffer, address, length);
    return EOK;
}

int ramdisk_write(RamDisk *disk, void *buffer, u8 sector_count, idx_t lba)
{
    void *address = disk->base_address + lba * SECTOR_SIZE;
    u32 length = sector_count * SECTOR_SIZE;
    assert(((u32)address + length) < (KERNEL_RAMDISK_MEM + KERNEL_MEMORY_SIZE));
    memcpy(address, buffer, length);
    return EOK;
}

void ramdisk_init()
{
    LOGK("Initializing ramdisks...\n");

    u32 partition_size = KERNEL_RAMDISK_SIZE / MAX_RAMDISKS;
    assert(partition_size % SECTOR_SIZE == 0);

    char device_name[32];

    for (size_t i = 0; i < MAX_RAMDISKS; i++)
    {
        RamDisk *ramdisk = &ramdisks[i];
        ramdisk->base_address = (u8 *)(KERNEL_RAMDISK_MEM + partition_size * i);
        ramdisk->total_size = partition_size;
        memset(ramdisk->base_address, 0, ramdisk->total_size);

        snprintf(device_name, sizeof(device_name), "md%c", i + 'a');
        device_install(DEV_BLOCK, DEV_RAMDISK, ramdisk, device_name, 0,
                       handle_ramdisk_ioctl, ramdisk_read, ramdisk_write);
    }
}