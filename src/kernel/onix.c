#include "hyc.h"

// 将内核元数据保存在指定段
static u32 __attribute__((section(".onix.addr"))) kernel_base_addr = 0x20000;
static u32 __attribute__((section(".onix.size"))) kernel_total_size = 0;
static u32 __attribute__((section(".onix.chksum"))) kernel_checksum = 0;
static u32 __attribute__((section(".onix.magic"))) kernel_identifier = ONIX_MAGIC;

static u8 magic_fail_msg[] = "Kernel magic validation failed!";
static u8 crc_fail_msg[] = "Kernel CRC32 validation failed!";
static char *video_memory = (char *)0xb8000;

extern void halt_system();

static void display_message(char *msg, int length)
{
    for (int i = 0; i < length; i++) {
        video_memory[i * 2] = msg[i];
    }
}

err_t onix_initialize()
{
    if (kernel_identifier != ONIX_MAGIC) {
        display_message(magic_fail_msg, sizeof(magic_fail_msg));
        goto error;
    }

    u32 ksize = kernel_total_size;
    u32 kchecksum = kernel_checksum;

    kernel_checksum = 0;
    kernel_total_size = 0;

    u32 calc_checksum = eth_fcs((void *)kernel_base_addr, ksize);
    if (calc_checksum != kchecksum) {
        display_message(crc_fail_msg, sizeof(crc_fail_msg));
        goto error;
    }

    return EOK;

error:
    halt_system();
}

