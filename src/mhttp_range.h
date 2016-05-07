#ifndef MHTTP_RANGE_H
#define MHTTP_RANGE_H

#include <string.h>
#include <stdio.h>
#include <ctype.h>

enum mhttp_range_spec
{
    MHTTP_RANGE_SPEC_NONE = 0x00,
    MHTTP_RANGE_SPEC_LOW =  0x01,
    MHTTP_RANGE_SPEC_HIGH = 0x10,
    MHTTP_RANGE_SPEC_BOTH = 0x11
};

struct mhttp_range {
    size_t low;
    size_t high;
    enum mhttp_range_spec spec;
};

int mhttp_parse_range(const char *ranhdr, struct mhttp_range *range);

#endif
