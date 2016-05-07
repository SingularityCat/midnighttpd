#include "mhttp_util.h"

size_t mhttp_urldecode(const char *src, char *dest, size_t lim)
{
    char *end = dest + lim;
    char chr = *src++, composed;
    int x;
    while(chr && dest < end)
    {
        switch(chr)
        {
            case '%':
                x = mhttp_char2hex(*src++);
                if(x == -1) { goto urldec_cont; }
                composed = x << 4;
                x = mhttp_char2hex(*src++);
                if(x == -1) { goto urldec_cont; }
                composed |= x;
                *dest++ = composed;
                break;
            case '+':
                *dest++ = ' ';
                break;
            default:
                *dest++ = chr;
        }
        urldec_cont: chr = *src++;
    }
    return (dest + lim) - end;
}
