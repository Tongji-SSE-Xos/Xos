#include "hyc.h"

char *copy_string(char *destination, const char *source)
{
    char *dest_ptr = destination;
    while (true)
    {
        *dest_ptr++ = *source;
        if (*source++ == EOS)
            return destination;
    }
}

char *copy_string_n(char *destination, const char *source, size_t max_chars)
{
    char *dest_ptr = destination;
    size_t i = 0;
    for (; i < max_chars; i++)
    {
        *dest_ptr++ = *source;
        if (*source++ == EOS)
            return destination;
    }
    destination[max_chars - 1] = EOS;
    return destination;
}

char *append_string(char *destination, const char *source)
{
    char *dest_ptr = destination;
    while (*dest_ptr != EOS)
    {
        dest_ptr++;
    }
    while (true)
    {
        *dest_ptr++ = *source;
        if (*source++ == EOS)
        {
            return destination;
        }
    }
}

size_t string_length_n(const char *str, size_t max_length)
{
    const char *str_ptr = str;
    while (*str_ptr != EOS && max_length--)
    {
        str_ptr++;
    }
    return str_ptr - str;
}

size_t string_length(const char *str)
{
    const char *str_ptr = str;
    while (*str_ptr != EOS)
    {
        str_ptr++;
    }
    return str_ptr - str;
}

int compare_strings(const char *str1, const char *str2)
{
    while (*str1 == *str2 && *str1 != EOS && *str2 != EOS)
    {
        str1++;
        str2++;
    }
    return *str1 < *str2 ? -1 : (*str1 > *str2);
}

char *find_char(const char *str, int character)
{
    char *str_ptr = (char *)str;
    while (true)
    {
        if (*str_ptr == character)
        {
            return str_ptr;
        }
        if (*str_ptr++ == EOS)
        {
            return NULL;
        }
    }
}

char *find_last_char(const char *str, int character)
{
    char *last_occurrence = NULL;
    char *str_ptr = (char *)str;
    while (true)
    {
        if (*str_ptr == character)
        {
            last_occurrence = str_ptr;
        }
        if (*str_ptr++ == EOS)
        {
            return last_occurrence;
        }
    }
}

int compare_memory(const void *mem1, const void *mem2, size_t num)
{
    const char *mem1_ptr = (char *)mem1;
    const char *mem2_ptr = (char *)mem2;
    while ((num > 0) && *mem1_ptr == *mem2_ptr)
    {
        mem1_ptr++;
        mem2_ptr++;
        num--;
    }
    if (num == 0)
        return 0;
    return *mem1_ptr < *mem2_ptr ? -1 : (*mem1_ptr > *mem2_ptr);
}

void *fill_memory(void *destination, int value, size_t num)
{
    char *dest_ptr = (char *)destination;
    while (num--)
    {
        *dest_ptr++ = (char)value;
    }
    return destination;
}

void *copy_memory(void *destination, const void *source, size_t num)
{
    char *dest_ptr = (char *)destination;
    const char *src_ptr = (const char *)source;
    while (num--)
    {
        *dest_ptr++ = *src_ptr++;
    }
    return destination;
}

void *find_memory(const void *str, int value, size_t num)
{
    const char *str_ptr = (const char *)str;
    while (num--)
    {
        if (*str_ptr == value)
        {
            return (void *)str_ptr;
        }
        str_ptr++;
    }
    return NULL;
}

#define PATH_SEPARATOR_1 '/'                                   // 目录分隔符 1
#define PATH_SEPARATOR_2 '\\'                                  // 目录分隔符 2
#define IS_PATH_SEPARATOR(c) (c == PATH_SEPARATOR_1 || c == PATH_SEPARATOR_2) // 判断字符是否为目录分隔符

// 获取字符串中第一个路径分隔符的位置
char *find_separator(const char *str)
{
    const char *str_ptr = str;
    while (true)
    {
        if (IS_PATH_SEPARATOR(*str_ptr))
        {
            return (char *)str_ptr;
        }
        if (*str_ptr++ == EOS)
        {
            return NULL;
        }
    }
}

// 获取字符串中最后一个路径分隔符的位置
char *find_last_separator(const char *str)
{
    char *last_sep = NULL;
    const char *str_ptr = str;
    while (true)
    {
        if (IS_PATH_SEPARATOR(*str_ptr))
        {
            last_sep = (char *)str_ptr;
        }
        if (*str_ptr++ == EOS)
        {
            return last_sep;
        }
    }
}
