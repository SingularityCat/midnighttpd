#ifndef MIG_PARSE_H
#define MIG_PARSE_H

#include <stdlib.h>
#include <stdbool.h>
#include <strings.h>

static inline int mig_char2hex(char chr)
{
    int n = -1;
    if(chr >= '0' && chr <= '9')
    {
        n = chr - '0';
    }
    else if((chr >= 'a' && chr <= 'f'))
    {
        n = (chr - 'a') + 10;
    }
    else if((chr >= 'A' && chr <= 'F'))
    {
        n = (chr - 'A') + 10;
    }
    return n;
}

static inline int mig_char2dec(char chr)
{
    int n = -1;
    if(chr >= '0' && chr <= '9')
    {
        n = chr - '0';
    }
    return n;
}

static inline int mig_char2oct(char chr)
{
    int n = -1;
    if(chr >= '0' && chr <= '7')
    {
        n = chr - '0';
    }
    return n;
}

static inline int mig_char2bin(char chr)
{
    switch(chr)
    {
        case '0': return 0;
        case '1': return 1;
    }
    return -1;
}

/* Returns:
 *  0 if false
 *  1 if true
 * -1 if error
 */
static inline int mig_parse_bool(const char *str, const char **fbp)
{
    int res = -1;
    switch(*str++)
    {
        case 't':
        case 'T':
            res = strncasecmp(str, "rue", 3) == 0 ? 1 : -1;
            str += 3;
            break;
        case 'f':
        case 'F':
            res = strncasecmp(str, "alse", 4) == 0 ? 0 : -1;
            str += 4;
            break;
        case 'y':
        case 'Y':
            res = strncasecmp(str, "es", 2) == 0 ? 1 : -1;
            str += 2;
            break;
        case 'n':
        case 'N':
            res = (*str == 'o' || *str == 'O') ? 0 : -1;
            str++;
            break;
        case 'o':
        case 'O':
            switch(*str++)
            {
                case 'f':
                case 'F':
                    res = (*str == 'f' || *str == 'F') ? 0 : -1;
                    str++;
                    break;
                case 'n':
                case 'N':
                    res = 1;
                    break;
            }
    }

    if(fbp)
    {
        *fbp = str;
    }
    return res;
}

static inline int mig_parse_int(const char *str, const char **fbp);

static inline int mig_parse_size(const char *str, const char **fbp)
{
    int mantissa = mig_parse_int(str, &str);
    int base = 1;
    if(str != NULL)
    {
        switch(*str++)
        {
            case 'K':
            case 'k':
                base = 1024;
                break;
            case 'M':
            case 'm':
                base = 1024 * 1024;
                break;
            case 'G':
            case 'g':
                base = 1024 * 1024 * 1024;
                break;
        }
        if(*str == 'i' && *(str + 1) == 'B')
        {
            str += 2;
        }
    }

    if(fbp)
    {
        *fbp = str;
    }
    return mantissa * base;
}

static inline unsigned int mig_parse_uint_bin(const char *str, const char **fbp);
static inline unsigned int mig_parse_uint_oct(const char *str, const char **fbp);
static inline unsigned int mig_parse_uint_dec(const char *str, const char **fbp);
static inline unsigned int mig_parse_uint_hex(const char *str, const char **fbp);

static inline int mig_parse_int(const char *str, const char **fbp)
{
    int sign = 1;
    if(*str == '-')
    {
        sign = -1;
        str++;
    }
    else if(*str == '+')
    {
        str++;
    }

    if(*str == '0')
    {
        str++;
        switch(*str++)
        {
            case 'b':
                return sign * mig_parse_uint_bin(str, fbp);
            case 'o':
                return sign * mig_parse_uint_oct(str, fbp);
            case 'x':
                return sign * mig_parse_uint_hex(str, fbp);
            case 0:
                if(fbp) { *fbp = NULL; }
                return 0;
            default:
                str -= 2;
        }
    }

    return sign * mig_parse_uint_dec(str, fbp);
}

#define define_si_mig_parse_uint_basep2(convfunc, base_name, base_bits) \
static inline unsigned int mig_parse_uint_##base_name(const char *str, const char **fbp) \
{ \
    int n = 0; \
    int d = convfunc(*str++); \
    if(d != -1) \
    { \
        while(d > -1) \
        { \
            n <<= base_bits; \
            n |= d; \
            d = convfunc(*str++); \
        } \
        str--;\
    } \
    else \
    { \
        str = NULL; \
    } \
 \
    if(fbp) \
    { \
        *fbp = str; \
    } \
    return n; \
}

#define define_si_mig_parse_uint_base_n(convfunc, base_name, base) \
static inline unsigned int mig_parse_uint_##base_name(const char *str, const char **fbp) \
{ \
    int n = 0; \
    int d = convfunc(*str++); \
    if(d != -1) \
    { \
        while(d > -1) \
        { \
            n *= base; \
            n += d; \
            d = convfunc(*str++); \
        } \
        str--;\
    } \
    else \
    { \
        str = NULL; \
    } \
 \
    if(fbp) \
    { \
        *fbp = str; \
    } \
    return n; \
}

define_si_mig_parse_uint_basep2(mig_char2bin, bin, 1)
define_si_mig_parse_uint_basep2(mig_char2oct, oct, 3)
define_si_mig_parse_uint_base_n(mig_char2dec, dec, 10)
define_si_mig_parse_uint_basep2(mig_char2hex, hex, 4)

#endif
