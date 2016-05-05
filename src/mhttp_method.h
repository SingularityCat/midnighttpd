#include <string.h>

#define MHTTP_METHOD_UNSUPPORTED 0xF00
enum mhttp_method
{
    MHTTP_METHOD_NONE = 0x0,
    MHTTP_METHOD_GET,
    MHTTP_METHOD_HEAD,
    MHTTP_METHOD_OPTIONS,
    /* unsupported options, all start with 0x0F00 */
    MHTTP_METHOD_PUT = 0xF00,
    MHTTP_METHOD_DELETE,
    MHTTP_METHOD_PATCH,
    MHTTP_METHOD_POST
};

#define allowed_methods "GET,HEAD,OPTIONS"
static inline enum mhttp_method mhttp_interpret_method(char *meth)
{
    switch(*meth++)
    {
        case 'G': /* GET */
            if(strncmp("ET", meth, 2) == 0)
            {
                return MHTTP_METHOD_GET;
            }
            break;
        case 'H': /* HEAD */
            if(strncmp("EAD", meth, 3) == 0)
            {
                return  MHTTP_METHOD_HEAD;
            }
            break;
        case 'O': /* OPTIONS */
            if(strncmp("PTIONS", meth, 6) == 0)
            {
                return  MHTTP_METHOD_OPTIONS;
            }
            break;
        case 'P': /* P(UT|ATCHA|OST) */
            switch(*meth++)
            {
                case 'U': /* PUT */
                    if(*meth == 'T')
                    {
                        return MHTTP_METHOD_PUT;
                    }
                    break;
                case 'A': /* PATCH */
                    if(strncmp("TCH", meth, 3) == 0)
                    {
                        return MHTTP_METHOD_PATCH;
                    }
                    break;
                case 'O': /* POST */
                    if(strncmp("ST", meth, 2) == 0)
                    {
                        return MHTTP_METHOD_POST;
                    }
                    break;
            }
            break;
        case 'D': /* DELETE */
            if(strncmp("ELETE", meth, 5) == 0)
            {
                return MHTTP_METHOD_DELETE;
            }
            break;
    }
    return MHTTP_METHOD_NONE;
}

