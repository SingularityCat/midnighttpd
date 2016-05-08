#include "mhttp_range.h"

int mhttp_parse_range(const char *ranhdr, struct mhttp_range *range)
{
    while(*ranhdr == ' ') { ranhdr++; } /* Skip whitespace */
    if(strncmp("bytes=", ranhdr, 6) == 0)
    {
        ranhdr += 6;
        range->spec = MHTTP_RANGE_SPEC_NONE;
        /* Extract lower number, if applicable. */
        if(isdigit(*ranhdr))
        {
            range->spec |= MHTTP_RANGE_SPEC_LOW;
            range->low = 0;
            while(isdigit(*ranhdr))
            {
                range->low *= 10;
                range->low += *ranhdr++ - 48;
            }
        }
        if(*ranhdr++ != '-')
        {
            /* Malformed header? */
            goto ranpar_err;
        }
        if(isdigit(*ranhdr))
        {
            range->spec |= MHTTP_RANGE_SPEC_HIGH;
            range->high = 0;
            while(isdigit(*ranhdr))
            {
                range->high *= 10;
                range->high += *ranhdr++ - 48;
            }
        }
        return 0;
    }
    ranpar_err: return 1;
}
