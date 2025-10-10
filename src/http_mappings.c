#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include "http_mappings.h"
#include "string_utils.h"
#include <stdio.h>

// Define allowed methods and header mappings
const char *const allowed_methods[] = {"GET", "POST", "HEAD"};
const size_t allowed_methods_count =
    sizeof(allowed_methods) / sizeof(allowed_methods[0]);

static const header_mapping_t header_mappings[] = {
    // --- General headers ---
    {"host", 1, 0},
    {"connection", 1, 1},
    {"cache-control", 1, 1},
    {"pragma", 1, 1},
    {"upgrade", 1, 1},
    {"via", 1, 1},
    {"warning", 0, 1},

    // --- Request headers ---
    {"user-agent", 0, 0},
    {"accept", 1, 1},
    {"accept-encoding", 1, 1},
    {"accept-language", 1, 1},
    {"accept-charset", 1, 1},
    {"referer", 0, 0},
    {"origin", 0, 0},
    {"content-type", 1, 1},
    {"content-length", 0, 0},
    {"transfer-encoding", 1, 1},
    {"te", 1, 1},
    {"expect", 1, 1},
    {"authorization", 0, 0},
    {"cookie", 0, 0},
    {"upgrade-insecure-requests", 0, 0},
    {"if-modified-since", 0, 0},
    {"if-none-match", 0, 1},
    {"if-unmodified-since", 0, 0},
    {"if-match", 0, 1},
    {"if-range", 0, 0},

    // --- Response headers ---
    {"server", 0, 0},
    {"date", 0, 0},
    {"last-modified", 0, 0},
    {"etag", 0, 0},
    {"content-encoding", 1, 1},
    {"content-language", 1, 1},
    {"content-location", 0, 0},
    {"content-disposition", 0, 1},
    {"content-range", 1, 1},
    {"allow", 0, 1},
    {"vary", 1, 1},
    {"set-cookie", 0, 0},
    {"www-authenticate", 0, 1},
    {"proxy-authenticate", 0, 1},
    {"location", 0, 0},
    {"retry-after", 0, 0},
    {"expires", 0, 0},
    {"content-security-policy", 0, 0},

    // --- CORS headers ---
    {"access-control-allow-origin", 0, 0},
    {"access-control-allow-headers", 1, 1},
    {"access-control-allow-methods", 0, 1},
    {"access-control-expose-headers", 1, 1},
    {"access-control-request-headers", 1, 1},
    {"access-control-request-method", 0, 0},
    {"access-control-max-age", 0, 0},
};

static const size_t header_mappings_count =
    sizeof(header_mappings) / sizeof(header_mappings[0]);

// Trim leading and trailing whitespace from a string (in place)
static void trim_outer_whitespaces(char *str)
{
    char *start = str;
    while (*start == ' ' || *start == '\t')
        start++;

    if (start != str)
        memmove(str, start, strlen(start) + 1);

    char *end = str + strlen(str);
    while (end > str && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
        *--end = '\0';
}

// Trims whitespaces around ',', ';' and '=' in header values
// Keeps quoted strings intact and preserves escape sequences
static void trim_internal_whitespaces(char *value)
{
    if (!value || !*value)
        return;

    char *read_ptr = value;  // current read position
    char *write_ptr = value; // current write position, for non-whitespace chars
    int inside_quotes = 0;
    int escape_next = 0;

    while (*read_ptr)
    {
        char current_char = *read_ptr;

        if (inside_quotes)
        {
            // Always copy characters inside quotes
            *write_ptr++ = current_char;

            if (escape_next)
            {
                // Previous character was a backslash, skip escaping logic
                escape_next = 0;
            }
            else if (current_char == '\\')
            {
                // Start escape mode for next character
                escape_next = 1;
            }
            else if (current_char == '"')
            {
                // Closing quote reached
                inside_quotes = 0;
            }

            read_ptr++;
            continue;
        }

        if (current_char == '"') // Opening quote — enter quoted mode
        {
            inside_quotes = 1;
            *write_ptr++ = *read_ptr++;
            continue;
        }

        if (current_char == ';' || current_char == ',' || current_char == '=')
        {

            // 1. Remove spaces/tabs before the delimiter
            while (write_ptr > value && (write_ptr[-1] == ' ' || write_ptr[-1] == '\t'))
                write_ptr--;

            // 2. Write the delimiter itself
            *write_ptr++ = current_char;
            read_ptr++;

            // 3. Skip spaces/tabs after the delimiter
            while (*read_ptr == ' ' || *read_ptr == '\t')
                read_ptr++;

            continue;
        }

        // Default: copy normal character
        *write_ptr++ = *read_ptr++;
    }

    // Null-terminate the cleaned string
    *write_ptr = '\0';
}

// Converts a string to lowercase (in place)
void normalize_string_lower(char *str)
{
    for (; *str; str++)
        *str = (char)tolower((unsigned char)*str);
}

// Normalizes only the header name (before ':') to lowercase
void normalize_header_name(char *header_line)
{
    if (!header_line || !*header_line)
        return;

    char *colon = strchr(header_line, ':');
    if (!colon)
        return; // Invalid header (no ':')

    // Store where the value starts
    char *value = colon + 1;

    // Temporarily split into two parts: name and value
    *colon = '\0';
    trim_outer_whitespaces(header_line); // Trim name

    size_t name_len = strlen(header_line);
    header_line[name_len] = ':'; // Restore colon

    // Put value string after the trimmed name
    size_t val_len = strlen(value);
    memmove(header_line + name_len + 1, value, val_len + 1); // +1 to copy the '\0'

    for (char *p = header_line; *p && *p != ':'; p++)
        *p = (char)tolower((unsigned char)*p);
}

// Normalizes the header value to lowercase if the header is case-insensitive
void normalize_header_value(char *header_line)
{
    char *colon = strchr(header_line, ':');
    if (!colon)
        return; // Invalid header (no ':')

    // Extract header name
    size_t name_len = colon - header_line;
    if (name_len == 0)
        return;

    char header_name[128];
    if (name_len >= sizeof(header_name))
        return;

    strncpy(header_name, header_line, name_len);
    header_name[name_len] = '\0';

    // Locate value and trim
    char *value = colon + 1;
    trim_outer_whitespaces(value);

    // If this header's value is case-insensitive, normalize to lowercase
    if (is_header_value_case_insensitive(header_name))
    {
        normalize_string_lower(value);
    }

    if (should_trim_ows(header_name))
    {
        trim_internal_whitespaces(value);
    }
}

void normalize_header(char *header_line)
{
    normalize_header_name(header_line);
    normalize_header_value(header_line);
}

int is_method_allowed(const char *method)
{
    for (size_t i = 0; i < allowed_methods_count; i++)
    {
        if (strcmp(method, allowed_methods[i]) == 0)
            return 1;
    }
    return 0;
}

int is_header_value_case_insensitive(const char *header_name)
{
    for (size_t i = 0; i < header_mappings_count; i++)
    {
        if (str_case_cmp(header_name, header_mappings[i].name) == 0)
            return header_mappings[i].value_case_insensitive;
    }
    return 0;
}

int should_trim_ows(const char *header_name)
{
    for (size_t i = 0; i < header_mappings_count; i++)
    {
        if (str_case_cmp(header_name, header_mappings[i].name) == 0)
            return header_mappings[i].trim_ows;
    }
    return 0;
}
