#ifndef HTTP_ERRORS_H
#define HTTP_ERRORS_H

typedef enum {
    // I/O layer outcomes (transport)
    HTTP_IO_EOF                =  0,  // Client closed connection
    HTTP_IO_ERROR              = -1,  // I/O error 
    HTTP_IO_TIMEOUT            = -2,  // keep-alive timeout (EAGAIN/EWOULDBLOCK)
    HTTP_IO_TIMEOUT_PARTIAL    = -3,  // timeout but some data read
    HTTP_IO_EOF_PARTIAL        = -4,  // client closed connection but some data read

    // Parsing / protocol outcomes
    HTTP_PARSE_ERROR           = -10,
    HTTP_HEADERS_TOO_LARGE     = -11,
    HTTP_URI_TOO_LONG          = -12,
    HTTP_BODY_TOO_LARGE        = -13,
    HTTP_LENGTH_REQUIRED       = -14,
    HTTP_VERSION_UNSUPPORTED   = -15,
    HTTP_METHOD_NOT_ALLOWED    = -16,
    HTTP_NOT_IMPLEMENTED       = -17,
} http_io_status_t;

#endif
