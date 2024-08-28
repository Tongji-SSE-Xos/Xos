#include "hyc.h"

#define PAD_ZERO 0x01   // Zero padding
#define IS_SIGNED 0x02  // Signed/unsigned long
#define SHOW_PLUS 0x04  // Show plus sign
#define SPACE_PAD 0x08  // Pad space if no sign
#define ALIGN_LEFT 0x10 // Left alignment
#define IS_SPECIAL 0x20 // 0x or 0 prefix
#define USE_LOWER 0x40  // Use lowercase letters
#define IS_DOUBLE 0x80  // Floating-point number

#define is_num(c) ((c) >= '0' && (c) <= '9')

static int extract_int(const char **s)
{
    int result = 0;
    while (is_num(**s))
        result = result * 10 + *((*s)++) - '0';
    return result;
}

static char *convert_number(char *str, u32 *value, int radix, int width, int precision, int options)
{
    char pad_char, sign_char, buffer[36];
    const char *digit_chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int len;
    int idx;
    char *output = str;

    if (options & USE_LOWER)
        digit_chars = "0123456789abcdefghijklmnopqrstuvwxyz";

    if (options & ALIGN_LEFT)
        options &= ~PAD_ZERO;

    if (radix < 2 || radix > 36)
        return 0;

    pad_char = (options & PAD_ZERO) ? '0' : ' ';

    if (options & IS_DOUBLE && (*(double *)(value)) < 0)
    {
        sign_char = '-';
        *(double *)(value) = -(*(double *)(value));
    }
    else if (options & IS_SIGNED && !(options & IS_DOUBLE) && ((int)(*value)) < 0)
    {
        sign_char = '-';
        (*value) = -(int)(*value);
    }
    else
        sign_char = (options & SHOW_PLUS) ? '+' : ((options & SPACE_PAD) ? ' ' : 0);

    if (sign_char)
        width--;

    if (options & IS_SPECIAL)
    {
        if (radix == 16)
            width -= 2;
        else if (radix == 8)
            width--;
    }

    len = 0;

    if (options & IS_DOUBLE)
    {
        u32 int_part = (u32)(*(double *)value);
        u32 frac_part = (u32)(((*(double *)value) - int_part) * 1000000);

        int mantissa = 6;
        while (mantissa--)
        {
            idx = (frac_part) % radix;
            (frac_part) /= radix;
            buffer[len++] = digit_chars[idx];
        }

        buffer[len++] = '.';

        do
        {
            idx = (int_part) % radix;
            (int_part) /= radix;
            buffer[len++] = digit_chars[idx];
        } while (int_part);
    }
    else if ((*value) == 0)
    {
        buffer[len++] = '0';
    }
    else
    {
        while ((*value) != 0)
        {
            idx = (*value) % radix;
            (*value) /= radix;
            buffer[len++] = digit_chars[idx];
        }
    }

    if (len > precision)
        precision = len;

    width -= precision;

    if (!(options & (PAD_ZERO + ALIGN_LEFT)))
        while (width-- > 0)
            *str++ = ' ';

    if (sign_char)
        *str++ = sign_char;

    if (options & IS_SPECIAL)
    {
        if (radix == 8)
            *str++ = '0';
        else if (radix == 16)
        {
            *str++ = '0';
            *str++ = digit_chars[33];
        }
    }

    if (!(options & ALIGN_LEFT))
        while (width-- > 0)
            *str++ = pad_char;

    while (len < precision--)
        *str++ = '0';

    while (len-- > 0)
        *str++ = buffer[len];

    while (width-- > 0)
        *str++ = ' ';
    return str;
}

int vsnprintf(char *buffer, const char *format, va_list args)
{
    int length;
    int i;
    char *output;
    char *str_arg;
    int *int_arg;
    int flags;
    int field_width;
    int precision;
    int length_modifier;
    u32 value;
    u8 *byte_ptr;

    for (output = buffer; *format; ++format)
    {
        if (*format != '%')
        {
            *output++ = *format;
            continue;
        }

        flags = 0;
    repeat_flag:
        ++format;
        switch (*format)
        {
        case '-':
            flags |= ALIGN_LEFT;
            goto repeat_flag;
        case '+':
            flags |= SHOW_PLUS;
            goto repeat_flag;
        case ' ':
            flags |= SPACE_PAD;
            goto repeat_flag;
        case '#':
            flags |= IS_SPECIAL;
            goto repeat_flag;
        case '0':
            flags |= PAD_ZERO;
            goto repeat_flag;
        }

        field_width = -1;

        if (is_num(*format))
            field_width = extract_int(&format);
        else if (*format == '*')
        {
            ++format;
            field_width = va_arg(args, int);

            if (field_width < 0)
            {
                field_width = -field_width;
                flags |= ALIGN_LEFT;
            }
        }

        precision = -1;

        if (*format == '.')
        {
            ++format;
            if (is_num(*format))
                precision = extract_int(&format);
            else if (*format == '*')
                precision = va_arg(args, int);
            if (precision < 0)
                precision = 0;
        }

        length_modifier = -1;
        if (*format == 'h' || *format == 'l' || *format == 'L')
        {
            length_modifier = *format;
            ++format;
        }

        switch (*format)
        {
        case 'c':
            if (!(flags & ALIGN_LEFT))
                while (--field_width > 0)
                    *output++ = ' ';
            *output++ = (unsigned char)va_arg(args, int);
            while (--field_width > 0)
                *output++ = ' ';
            break;

        case 's':
            str_arg = va_arg(args, char *);
            length = strlen(str_arg);
            if (precision < 0)
                precision = length;
            else if (length > precision)
                length = precision;

            if (!(flags & ALIGN_LEFT))
                while (length < field_width--)
                    *output++ = ' ';
            for (i = 0; i < length; ++i)
                *output++ = *str_arg++;
            while (length < field_width--)
                *output++ = ' ';
            break;

        case 'o':
            value = va_arg(args, unsigned long);
            output = convert_number(output, &value, 8, field_width, precision, flags);
            break;

        case 'p':
            if (field_width == -1)
            {
                field_width = 8;
                flags |= PAD_ZERO;
            }
            value = va_arg(args, unsigned long);
            output = convert_number(output, &value, 16, field_width, precision, flags);
            break;

        case 'x':
            flags |= USE_LOWER;
        case 'X':
            value = va_arg(args, unsigned long);
            output = convert_number(output, &value, 16, field_width, precision, flags);
            break;

        case 'd':
        case 'i':
            flags |= IS_SIGNED;
        case 'u':
            value = va_arg(args, unsigned long);
            output = convert_number(output, &value, 10, field_width, precision, flags);
            break;

        case 'n':
            int_arg = va_arg(args, int *);
            *int_arg = (output - buffer);
            break;
        case 'f':
            flags |= IS_SIGNED | IS_DOUBLE;
            double dvalue = va_arg(args, double);
            output = convert_number(output, (u32 *)&dvalue, 10, field_width, precision, flags);
            break;
        case 'b': 
            value = va_arg(args, unsigned long);
            output = convert_number(output, &value, 2, field_width, precision, flags);
            break;
        case 'm': 
            flags |= USE_LOWER | PAD_ZERO;
            byte_ptr = va_arg(args, char *);
            for (size_t j = 0; j < 6; j++, byte_ptr++)
            {
                int temp = *byte_ptr;
                output = convert_number(output, &temp, 16, 2, precision, flags);
                *output = ':';
                output++;
            }
            output--;
            break;
        case 'r':
            flags |= USE_LOWER | PAD_ZERO;
            byte_ptr = va_arg(args, char *);
            for (size_t j = 0; j < 4; j++, byte_ptr++)
            {
                int temp = *byte_ptr;
                output = convert_number(output, &temp, 10, 2, precision, flags);
                *output = '.';
                output++;
            }
            output--;
            break;

        default:
            if (*format != '%')
                *output++ = '%';
            if (*format)
                *output++ = *format;
            else
                --format;
            break;
        }
    }
    *output = '\0';
    return output - buffer;
}

int snprintf(char *str, const char *fmt, ...)
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = vsnprintf(str, fmt, args);
    va_end(args);
    return n;
}
