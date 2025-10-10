#include "string_utils.h"

int str_case_cmp(const char *a, const char *b)
{
    if (!a || !b)
        return (a == b) ? 0 : (a ? 1 : -1);

    while (*a && *b)
    {
        unsigned char ca = ascii_tolower_uc((unsigned char)*a++);
        unsigned char cb = ascii_tolower_uc((unsigned char)*b++);
        if (ca != cb)
            return (int)ca - (int)cb;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strn_case_cmp(const char *a, const char *b, size_t n)
{
    if (!a || !b)
        return (a == b) ? 0 : (a ? 1 : -1);

    for (size_t i = 0; i < n; i++)
    {
        unsigned char ca = ascii_tolower_uc((unsigned char)a[i]);
        unsigned char cb = ascii_tolower_uc((unsigned char)b[i]);
        if (ca != cb)
            return (int)ca - (int)cb;
        if (a[i] == '\0' || b[i] == '\0')
            return 0;
    }
    return 0;
}
