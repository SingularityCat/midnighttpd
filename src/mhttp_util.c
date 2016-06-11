#include <string.h>

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

size_t mhttp_scrubpath(char *str, bool allow_ascent, enum mhttp_path_flags *flgp)
{
    enum mhttp_path_flags flg = 0;

    int depth = 0;
    char *dest = str;
    char *psp, *sp;
    char * const end = str + strlen(str);
    psp = strchr(str, '/');
    while(psp && psp != end)
    {
        bool copy = true;
        sp = strchr(psp+1, '/');
        sp = sp ? sp : end;
        if(strncmp(psp, "/..", 3) == 0)
        {
            copy = false;
            if(depth > 0)
            {
                while(*--dest != '/');
                depth--;
            }
            else
            {
                flg |= MHTTP_PATH_ASCENDANT;
                copy = allow_ascent;
            }
        }
        else if(strncmp(psp, "/.", 2) == 0 || strncmp(psp, "//", 2) == 0)
        {
            copy = false;
        }

        if(copy)
        {
            memmove(dest, psp, sp - psp);
            dest += sp - psp;
            depth++;
        }
        psp = sp;
    }
    *dest = 0;
    if(flgp)
    {
        *flgp = flg;
    }
    return str - dest;
}

