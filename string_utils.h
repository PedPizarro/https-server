#ifndef ASCII_STRCASECMP_H
#define ASCII_STRCASECMP_H

#include <stddef.h>

static inline unsigned char ascii_tolower_uc(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') ? (unsigned char)(c + ('a' - 'A')) : c;
}

// Portable ASCII-only replacement for strcasecmp()
int str_case_cmp(const char *a, const char *b);

// Portable ASCII-only replacement for strncasecmp()
int strn_case_cmp(const char *a, const char *b, size_t n);

#endif
