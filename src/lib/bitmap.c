#include "hyc.h"

// 构造位图
void bitmap_create(bitmap_t *bmp, char *data, u32 len, u32 ofs)
{
    bmp->bits = data;
    bmp->length = len;
    bmp->offset = ofs;
}

// 初始化位图，将所有位重置为 0
void bitmap_initialize(bitmap_t *bmp, char *data, u32 len, u32 start)
{
    memset(data, 0, len);
    bitmap_create(bmp, data, len, start);
}

// 检查位图中某一位是否为 1
bool bitmap_is_set(bitmap_t *bmp, idx_t pos)
{
    assert(pos >= bmp->offset);

    // 计算位图的具体索引
    idx_t idx = pos - bmp->offset;

    // 获取对应的字节和位
    u32 byte_idx = idx / 8;
    u8 bit_idx = idx % 8;

    assert(byte_idx < bmp->length);

    // 返回该位是否为 1
    return (bmp->bits[byte_idx] & (1 << bit_idx)) != 0;
}

// 设置位图中某一位的值
void bitmap_set_bit(bitmap_t *bmp, idx_t pos, bool val)
{
    // val 必须是 0 或 1
    assert(val == 0 || val == 1);
    assert(pos >= bmp->offset);

    // 计算位图的索引
    idx_t idx = pos - bmp->offset;

    // 计算对应的字节和位
    u32 byte_idx = idx / 8;
    u8 bit_idx = idx % 8;

    if (val)
    {
        // 将对应的位设置为 1
        bmp->bits[byte_idx] |= (1 << bit_idx);
    }
    else
    {
        // 将对应的位设置为 0
        bmp->bits[byte_idx] &= ~(1 << bit_idx);
    }
}

// 从位图中寻找连续的 count 位可用位
int bitmap_find(bitmap_t *bmp, u32 count)
{
    int start_pos = -1;            // 用于标记找到的开始位置
    u32 remaining_bits = bmp->length * 8; // 位图中的剩余位数
    u32 current_bit = 0;           // 当前位索引
    u32 match_count = 0;           // 连续匹配计数

    // 从头开始查找
    while (remaining_bits-- > 0)
    {
        if (!bitmap_is_set(bmp, bmp->offset + current_bit))
        {
            // 当前位未占用，计数加一
            match_count++;
        }
        else
        {
            // 当前位已占用，计数重置
            match_count = 0;
        }

        // 移动到下一位
        current_bit++;

        // 如果找到了连续 count 个空位
        if (match_count == count)
        {
            start_pos = current_bit - count;
            break;
        }
    }

    // 如果未找到连续空位，返回 -1
    if (start_pos == -1)
        return -1;

    // 否则将找到的位设置为 1
    remaining_bits = count;
    current_bit = start_pos;
    while (remaining_bits--)
    {
        bitmap_set_bit(bmp, bmp->offset + current_bit, true);
        current_bit++;
    }

    // 返回找到的开始位置的索引
    return start_pos + bmp->offset;
}
