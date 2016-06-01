#ifndef MHTTP_UTIL_H
#define MHTTP_UTIL_H

#include <stdlib.h>

static inline int mhttp_char2hex(char chr)
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

size_t mhttp_urldecode(char *str, size_t lim);

#endif
