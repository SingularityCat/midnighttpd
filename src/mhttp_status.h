#ifndef MHTTP_STATUS_H
#define MHTTP_STATUS_H

#include <stdio.h>
#include <errno.h>

#include <unistd.h>

#include <mig_io.h>

/* A nice hacro - expand and concatenate */
#define expcat(a, b) inner_expcat(a, b)
#define inner_expcat(a, b) a ## b


#define http200 "200 OK"
#define http204 "204 No content"
#define http206 "206 Partial Content"
#define http400 "400 Malformed Reqeuest"
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


#define mhttp_error_resp(error) \
    ("HTTP/1.1 " error "\r\n"\
     SERVER_HEADER\
     "Content-Length: 0\r\n"\
     "\r\n")


#define mhttp_send_error(fd, error) \
    mig_unintr_write(fd, mhttp_error_resp(error),\
        sizeof(mhttp_error_resp(error)))

#endif
