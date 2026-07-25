#include "string.h"

char* strstr(const char* str, const char* pat)
{
    const unsigned char* s;
    const unsigned char* p;
    unsigned long c;
    unsigned long ch;

    if (pat == 0) {
        return (char*) str;
    }
    s = (const unsigned char*) str - 1;
    p = (const unsigned char*) pat - 1;
    c = *++p;
    if (c == 0) {
        return (char*) str;
    }
    while ((ch = *++s) != 0) {
        if (ch == c) {
            const unsigned char* s2 = s - 1;
            const unsigned char* p2 = p - 1;
            unsigned long a;
            unsigned long b;

            do {
                a = *++s2;
                b = *++p2;
                if (a != b) {
                    break;
                }
            } while (a != 0);
            if (a == b || b == 0) {
                return (char*) s;
            }
        }
    }
    return 0;
}

char* strrchr(const char* str, int chr)
{
    const unsigned char* s = (const unsigned char*) str - 1;
    unsigned long c = chr & 0xff;
    unsigned long ch;
    char* result = 0;

    while ((ch = *++s) != 0) {
        if (ch == c) {
            result = (char*) s;
        }
    }
    if (result != 0) {
        return result;
    }
    return c ? 0 : (char*) s;
}
