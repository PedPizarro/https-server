#ifndef RESPONSE_UTILS_H
#define RESPONSE_UTILS_H

#include <stddef.h>

// Writes a RFC 9110-compliant Date header into `headers` at position `*offset`
void add_date_header(char *headers, size_t *offset, size_t buffer_size);

// Builds an Allow: header value with the allowed methods
const char *build_allow_header(void);

#endif
