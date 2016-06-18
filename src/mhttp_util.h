#ifndef MHTTP_UTIL_H
#define MHTTP_UTIL_H

#include <stddef.h>
#include <stdbool.h>

enum mhttp_path_flags
{
    MHTTP_PATH_ASCENDANT = 1
};

size_t mhttp_urldecode(char *str, size_t lim);
size_t mhttp_scrubpath(char *str, bool allow_ascent, enum mhttp_path_flags *flgp);

#endif
