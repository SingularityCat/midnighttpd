#ifndef MHTTP_STATUS_H
#define MHTTP_STATUS_H

#include <stdio.h>
#include <errno.h>

#include <unistd.h>

#include "mig_io.h"

/* A nice hacro - expand and concatenate */
#define expcat(a, b) inner_expcat(a, b)
#define inner_expcat(a, b) a ## b

#define http_v1_0 "HTTP/1.0"
#define http_v1_1 "HTTP/1.1"

#define http200 "200 OK"
#define http204 "204 No content"
#define http206 "206 Partial Content"
#define http400 "400 Malformed Request"
#define http403 "403 Forbidden"
#define http404 "404 Not Found"
#define http405 "405 Method Not Allowed"
#define http414 "414 Request-URI Too Long"
#define http416 "416 Requested Range Not Satisfiable"
#define http431 "431 Request Header Fields Too Large"
#define http500 "500 Internal Server Error"
#define http501 "501 Not Implemented"
#define http503 "503 Service Unavailable"
#define http508 "508 Loop Detected"

enum mhttp_version
{
    MHTTP_VERSION_1_0 = 10,
    MHTTP_VERSION_1_1 = 11
};

/* Note: sizeof includes null byte. Remember this. */

#define mhttp_str_ver(ver) \
    ((ver) == MHTTP_VERSION_1_1 ? http_v1_1 : http_v1_0)


#define mhttp_send_ver(fd, ver) \
    ((ver) == MHTTP_VERSION_1_1 ? \
        mig_unintr_write(fd, http_v1_1, sizeof(http_v1_1) - 1) :\
        mig_unintr_write(fd, http_v1_0, sizeof(http_v1_0) - 1))


#define mhttp_error_resp(error) \
    (" " error "\r\n"\
     SERVER_HEADER\
     "Content-Length: 0\r\n"\
     "\r\n")


#define mhttp_send_error(fd, ver, error) \
    mhttp_send_ver(fd, ver) + \
    mig_unintr_write(fd, mhttp_error_resp(error),\
        sizeof(mhttp_error_resp(error)) - 1)

#endif
