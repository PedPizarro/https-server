#include <stdio.h>
#include <string.h>
#include <time.h>

#include "response_utils.h"
#include "http_mappings.h"

void add_date_header(char *headers, size_t *offset, size_t buffer_size)
{
    time_t now = time(NULL);
    struct tm tm;
#if defined(_WIN32)
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    char date_str[64];
    // IMF-fixdate per RFC 9110 (Sun, 06 Nov 1994 08:49:37 GMT)
    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", &tm);

    if (*offset < buffer_size)
    {
        *offset += (size_t)snprintf(headers + *offset, buffer_size - *offset,
                                    "Date: %s\r\n", date_str);
        if (*offset > buffer_size)
            *offset = buffer_size;
    }
}

// Build Allow: header value, with the allowed methods
const char *build_allow_header()
{
    static char allow_buffer[128];
    allow_buffer[0] = '\0';

    size_t offset = 0;
    offset += snprintf(allow_buffer + offset, sizeof(allow_buffer) - offset, "Allow: ");

    for (size_t i = 0; i < allowed_methods_count; i++)
    {
        // Append method
        offset += snprintf(allow_buffer + offset, sizeof(allow_buffer) - offset, "%s", allowed_methods[i]);

        // Append delimiter if not last
        if (i + 1 < allowed_methods_count)
            offset += snprintf(allow_buffer + offset, sizeof(allow_buffer) - offset, ", ");
    }

    offset += snprintf(allow_buffer + offset, sizeof(allow_buffer) - offset, "\r\n");

    return allow_buffer;
}