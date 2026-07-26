#include "string.h"

char* strstr(const char* str, const char* pat)
{
    register const unsigned char* s;
    register const unsigned char* p;
    register unsigned long c;
    register unsigned long ch;

    s = (const unsigned char*) str - 1;
    p = (const unsigned char*) pat - 1;
    if (pat == 0) {
        return (char*) str;
    }
    c = *++p;
    if (c == 0) {
        return (char*) str;
    }
    while ((ch = *++s) != 0) {
        if (ch == c) {
            register unsigned long b;
            register const unsigned char* s2 = s - 1;
            register const unsigned char* p2 = p - 1;

            do {
                ch = *++s2;
                b = *++p2;
                if (ch != b) {
                    break;
                }
            } while (ch != 0);
            if (b == 0) {
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
