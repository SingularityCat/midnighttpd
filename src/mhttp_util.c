#include "mhttp_util.h"

size_t mhttp_urldecode(char *str, size_t lim)
{
    char *src, *dest;
    char *end = str + lim;
    char chr, composed;
    int x;
    if(lim == 0 || str == NULL) { return -1; }
    src = dest = str;
    do
    {
        chr = *src++;
        switch(chr)
        {
            case '%':
                x = mhttp_char2hex(chr = *src++);
                if(x == -1) { continue; }
                composed = x << 4;
                x = mhttp_char2hex(chr = *src++);
                if(x == -1) { continue; }
                composed |= x;
                *dest++ = composed;
                break;
            case '+':
                *dest++ = ' ';
                break;
            default:
                *dest++ = chr;
        }
    }
    while(chr && dest < end);
    *(dest - (dest < end ? 0 : 1)) = 0;
    return (dest + lim) - end;
}

