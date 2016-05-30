#include "mhttp_util.h"

size_t mhttp_urldecode(const char *src, char *dest, size_t lim)
{
    char *end = dest + lim;
    char chr, composed;
    int x;
    if(lim == 0) { return -1; }
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

