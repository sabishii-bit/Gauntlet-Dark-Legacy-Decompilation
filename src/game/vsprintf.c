#include "types.h"

/* Midway's vsprintf, derived from 4.3BSD-Net2 vfprintf.c (helper names
 * sRound/exponent/cvt confirmed by the Xbox build's VSPRINTF.OBJ), adapted
 * for buffer output, with a %b binary conversion added. isnan/isinf existed
 * as functions in the Xbox build; here they inline away. */

#include "__va_arg.h"

int sprintf(char* s, const char* fmt, ...);
int vsprintf(char* str, const char* fmt, va_list ap);

u32 strlen(const char* s);
void* memcpy(void* dst, const void* src, u32 n);
void* memchr(const void* s, int c, u32 n);
double modf(double x, double* iptr);

#define LONGINT   0x01 /* l: long integer */
#define LONGDBL   0x02 /* L: long double */
#define SHORTINT  0x04 /* h: short integer */
#define ALT       0x08 /* alternate form */
#define LADJUST   0x10 /* left adjustment */
#define ZEROPAD   0x20 /* zero (as opposed to blank) pad */
#define HEXPREFIX 0x40 /* add 0x or 0X prefix */

#define DEFPREC 6
#define MAXEXP 308
#define MAXFRACT 39
#define BUF (MAXEXP + MAXFRACT + 1)

#define PADSIZE 16

#define ARG() (flags & LONGINT ? va_arg(ap, long) : flags & SHORTINT ? (long) (short) va_arg(ap, int) : (long) va_arg(ap, int))

#define UARG() (flags & LONGINT ? va_arg(ap, u32) : flags & SHORTINT ? (u32) (unsigned short) va_arg(ap, int) : (u32) va_arg(ap, unsigned int))

#define to_digit(c) ((c) - '0')
#define is_digit(c) ((unsigned) to_digit(c) <= 9)
#define to_char(n) ((n) + '0')

#define PRINT(p, len)                        \
    {                                        \
        memcpy(str, (char*) (p), (u32) (len)); \
        str += (len);                        \
    }

#define PAD(howmany, with)              \
    {                                   \
        if ((n = (howmany)) > 0) {      \
            while (n > PADSIZE) {       \
                PRINT(with, PADSIZE);   \
                n -= PADSIZE;           \
            }                           \
            PRINT(with, n);             \
        }                               \
    }

/* double classification for the %eEfgG path (Xbox build: isnan/isinf) */
#define FP_NAN 1
#define FP_INFINITE 2
#define FP_ZERO 3
#define FP_NORMAL 4
#define FP_SUBNORMAL 5

static int fpclassify(double d)
{
    union {
        double d;
        struct {
            u32 hi;
            int lo;
        } i;
    } u;

    u.d = d;
    switch (u.i.hi & 0x7ff00000) {
    case 0x7ff00000:
        if ((u.i.hi & 0x000fffff) == 0) {
            if (u.i.lo == 0) {
                goto infinite;
            }
        }
        return FP_NAN;
    infinite:
        return FP_INFINITE;
    case 0:
        if ((u.i.hi & 0x000fffff) == 0) {
            if (u.i.lo == 0) {
                goto zero;
            }
        }
        return FP_SUBNORMAL;
    zero:
        return FP_ZERO;
    default:
        return FP_NORMAL;
    }
}

static char* sRound(double fract, int* exp, char* start, char* end, char ch, char* signp);
static char* exponent(char* p, int exp, int fmtch);
static int cvt(double value, int prec, int flags, char* sign, int ch, char* startp, char* endp);

int sprintf(char* s, const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    return vsprintf(s, fmt, ap);
}

static char* exponent(char* p, int exp, int fmtch)
{
    char* t;
    char expbuf[MAXEXP];

    *p++ = fmtch;
    if (exp < 0) {
        exp = -exp;
        *p++ = '-';
    } else {
        *p++ = '+';
    }
    t = expbuf + MAXEXP;
    if (exp > 9) {
        do {
            *--t = to_char(exp % 10);
        } while ((exp /= 10) > 9);
        *--t = to_char(exp);
        for (; t < expbuf + MAXEXP; *p++ = *t++)
            ;
    } else {
        *p++ = '0';
        *p++ = to_char(exp);
    }
    return p;
}

static char* sRound(double fract, int* exp, char* start, char* end, char ch, char* signp)
{
    double tmp;

    if (fract) {
        modf(fract * 10.0, &tmp);
    } else {
        tmp = to_digit(ch);
    }
    if (tmp > 4.0) {
        for (;; --end) {
            if (*end == '.') {
                --end;
            }
            if (++*end <= '9') {
                break;
            }
            *end = '0';
            if (end == start) {
                if (exp) { /* e/E; increment exponent */
                    *end = '1';
                    ++*exp;
                } else { /* f; add extra digit */
                    *--end = '1';
                    --start;
                }
                break;
            }
        }
    }
    /* ``"%.3f", (double)-0.0004'' gives you a negative 0. */
    else if (*signp == '-') {
        for (;; --end) {
            if (*end == '.') {
                --end;
            }
            if (*end != '0') {
                break;
            }
            if (end == start) {
                *signp = 0;
            }
        }
    }
    return start;
}

static int cvt(double value, int prec, int flags, char* sign, int ch, char* startp, char* endp)
{
    char* p;
    char* t;
    double fract;
    int dotrim;
    int expcnt;
    int gformat;
    double integer;
    double tmp;

    dotrim = expcnt = gformat = 0;
    if (value < 0.0) {
        *sign = '-';
        value = -value;
    } else {
        *sign = 0;
    }

    fract = modf(value, &integer);

    /* get an extra slot for rounding. */
    t = ++startp;

    /*
     * get integer portion of value; put into the end of the buffer; the
     * .01 is added for modf(356.0 / 10, &integer) returning .59999999...
     */
    if (value >= 1.0) {
        for (p = endp - 1; integer; ++expcnt) {
            tmp = modf(integer / 10.0, &integer);
            *p-- = to_char((int) (10.0 * (0.01 + tmp)));
        }
    } else {
        p = endp - 1;
    }

    switch (ch) {
    case 'f':
        /* reverse integer into beginning of buffer */
        if (expcnt) {
            for (; ++p < endp; *t++ = *p)
                ;
        } else {
            *t++ = '0';
        }
        /*
         * if precision required or alternate flag set, add in a
         * decimal point.
         */
        if (prec || (flags & ALT)) {
            *t++ = '.';
        }
        /* if requires more precision and some fraction left */
        if (fract) {
            if (prec) {
                do {
                    fract = modf(fract * 10.0, &tmp);
                    *t++ = to_char((int) tmp);
                } while (--prec && fract);
            }
            if (fract) {
                startp = sRound(fract, (int*) 0, startp, t - 1, '\0', sign);
            }
        }
        for (; prec; --prec) {
            *t++ = '0';
        }
        break;
    case 'e':
    case 'E':
    eformat:
        if (expcnt) {
            *t++ = *++p;
            if (prec || (flags & ALT)) {
                *t++ = '.';
            }
            /* if requires more precision and some integer left */
            for (; prec && ++p < endp; --prec) {
                *t++ = *p;
            }
            /*
             * if done precision and more of the integer component,
             * round using it; adjust fract so we don't re-round
             * later.
             */
            if (!prec && ++p < endp) {
                fract = 0.0;
                startp = sRound((double) 0, &expcnt, startp, t - 1, *p, sign);
            }
            /* adjust expcnt for digit in front of decimal */
            --expcnt;
        } else if (fract) {
            /* until first fractional digit, decrement exponent */
            expcnt = -1;
            for (;;) {
                fract = modf(fract * 10.0, &tmp);
                if (tmp) {
                    break;
                }
                --expcnt;
            }
            *t++ = to_char((int) tmp);
            if (prec || (flags & ALT)) {
                *t++ = '.';
            }
        } else {
            *t++ = '0';
            if (prec || (flags & ALT)) {
                *t++ = '.';
            }
        }
        /* if requires more precision and some fraction left */
        if (fract) {
            if (prec) {
                do {
                    fract = modf(fract * 10.0, &tmp);
                    *t++ = to_char((int) tmp);
                } while (--prec && fract);
            }
            if (fract) {
                startp = sRound(fract, &expcnt, startp, t - 1, '\0', sign);
            }
        }
        /* if requires more precision */
        for (; prec; --prec) {
            *t++ = '0';
        }
        /* unless alternate flag, trim any g/G format trailing 0's */
        if (gformat && !(flags & ALT)) {
            while (t > startp && *--t == '0')
                ;
            if (*t == '.') {
                --t;
            }
            ++t;
        }
        t = exponent(t, expcnt, ch);
        break;
    case 'g':
    case 'G':
        /* a precision of 0 is treated as a precision of 1. */
        if (!prec) {
            ++prec;
        }
        /*
         * ``The style used depends on the value converted; style e
         * will be used only if the exponent resulting from the
         * conversion is less than -4 or greater than the precision.''
         *	-- ANSI X3.159-1989
         */
        if (expcnt > prec || (!expcnt && fract && fract < 0.0001)) {
            --prec;
            ch -= 2; /* G->E, g->e */
            gformat = 1;
            goto eformat;
        }
        /*
         * reverse integer into beginning of buffer,
         * note, decrement precision
         */
        if (expcnt) {
            for (; ++p < endp; *t++ = *p, --prec)
                ;
        } else {
            *t++ = '0';
        }
        /*
         * if precision required or alternate flag set, add in a
         * decimal point.
         */
        if (prec || (flags & ALT)) {
            dotrim = 1;
            *t++ = '.';
        } else {
            dotrim = 0;
        }
        /* if requires more precision and some fraction left */
        if (fract) {
            if (prec) {
                do {
                    fract = modf(fract * 10.0, &tmp);
                    *t++ = to_char((int) tmp);
                } while (!tmp);
                while (--prec && fract) {
                    fract = modf(fract * 10.0, &tmp);
                    *t++ = to_char((int) tmp);
                }
            }
            if (fract) {
                startp = sRound(fract, (int*) 0, startp, t - 1, '\0', sign);
            }
        }
        /* alternate format, adds 0's for precision, else trim 0's */
        if (flags & ALT) {
            for (; prec; --prec) {
                *t++ = '0';
            }
        } else if (dotrim) {
            while (t > startp && *--t == '0')
                ;
            if (*t != '.') {
                ++t;
            }
        }
    }
    return t - startp;
}

int vsprintf(char* str, const char* fmt, va_list ap)
{
    char* s0 = str;
    int ch;
    int cnt;
    char* fmark;
    u32 flags;
    int fieldsz;
    int width;
    int prec;
    int dprec;
    int fpprec;
    int size;
    int realsz;
    int n;
    int base;
    u32 _ulong;
    char sign;
    char softsign;
    char unused[4];
    char* digs = 0;
    char* t;
    char buf[BUF];
    char ox[2];
    char blanks[PADSIZE] = "                ";
    char zeroes[PADSIZE] = "0000000000000000";
    double _double;

    cnt = 0;
    for (;;) {
        for (fmark = (char*) fmt; (ch = *fmt) != '\0' && ch != '%'; fmt++)
            ;
        if ((n = (char*) fmt - fmark) != 0) {
            PRINT(fmark, n);
            cnt += n;
        }
        if (ch == '\0') {
            break;
        }
        fmt++;

        sign = '\0';
        flags = 0;
        dprec = 0;
        fpprec = 0;
        width = 0;
        prec = -1;

    rflag:
        ch = *fmt++;
    reswitch:
        switch (ch) {
        case ' ':
            if (!sign) {
                sign = ' ';
            }
            goto rflag;
        case '#':
            flags |= ALT;
            goto rflag;
        case '*':
            width = va_arg(ap, int);
            if (width >= 0) {
                goto rflag;
            }
            width = -width;
            /* FALLTHROUGH */
        case '-':
            flags |= LADJUST;
            goto rflag;
        case '+':
            sign = '+';
            goto rflag;
        case '.':
            if ((ch = *fmt++) == '*') {
                n = va_arg(ap, int);
                prec = n < 0 ? -1 : n;
                goto rflag;
            }
            n = 0;
            while (is_digit(ch)) {
                n = 10 * n + to_digit(ch);
                ch = *fmt++;
            }
            prec = n < 0 ? -1 : n;
            goto reswitch;
        case '0':
            flags |= ZEROPAD;
            goto rflag;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            n = 0;
            do {
                n = 10 * n + to_digit(ch);
                ch = *fmt++;
            } while (is_digit(ch));
            width = n;
            goto reswitch;
        case 'L':
            flags |= LONGDBL;
            goto rflag;
        case 'h':
            flags |= SHORTINT;
            goto rflag;
        case 'l':
            flags |= LONGINT;
            goto rflag;
        case 'c':
            *(t = buf) = va_arg(ap, int);
            size = 1;
            sign = '\0';
            goto emit;
        case 'D':
            flags |= LONGINT;
            /* FALLTHROUGH */
        case 'd':
        case 'i':
            _ulong = ARG();
            if ((long) _ulong < 0) {
                sign = '-';
                _ulong = -_ulong;
            }
            base = 1;
            goto number;
        case 'e':
        case 'E':
        case 'f':
        case 'g':
        case 'G':
            _double = va_arg(ap, double);
            if (fpclassify(_double) == FP_INFINITE) {
                if (_double < 0.0) {
                    sign = '-';
                }
                t = (char*) "Inf";
                size = 3;
            } else if (fpclassify(_double) == FP_NAN) {
                t = (char*) "NaN";
                size = 3;
            } else {
                if (prec > MAXFRACT) {
                    if ((ch != 'g' && ch != 'G') || (flags & ALT)) {
                        fpprec = prec - MAXFRACT;
                    }
                    prec = MAXFRACT;
                } else if (prec == -1) {
                    prec = DEFPREC;
                }
                t = buf;
                *t = '\0';
                size = cvt(_double, prec, flags, &softsign, ch, t, buf + BUF);
                if (softsign) {
                    sign = '-';
                }
                if (*t == '\0') {
                    t++;
                }
            }
            goto emit;
        case 'n':
            if (flags & LONGINT) {
                *va_arg(ap, long*) = cnt;
            } else if (flags & SHORTINT) {
                *va_arg(ap, short*) = cnt;
            } else {
                *va_arg(ap, int*) = cnt;
            }
            continue;
        case 'O':
            flags |= LONGINT;
            /* FALLTHROUGH */
        case 'o':
            _ulong = UARG();
            base = 0;
            goto nosign;
        case 'p':
            _ulong = (u32) va_arg(ap, void*);
            digs = (char*) "0123456789abcdef";
            flags |= HEXPREFIX;
            base = 2;
            ch = 'x';
            goto nosign;
        case 's':
            if ((t = va_arg(ap, char*)) == 0) {
                t = (char*) "(null)";
            }
            if (prec >= 0) {
                /*
                 * can't use strlen; can only look for the
                 * NUL in the first `prec' characters, and
                 * strlen() will go further.
                 */
                char* p = memchr(t, 0, prec);

                if (p != 0) {
                    size = p - t;
                    if (size > prec) {
                        size = prec;
                    }
                } else {
                    size = prec;
                }
            } else {
                size = strlen(t);
            }
            sign = '\0';
            goto emit;
        case 'U':
            flags |= LONGINT;
            /* FALLTHROUGH */
        case 'u':
            _ulong = UARG();
            base = 1;
            goto nosign;
        case 'b':
            _ulong = UARG();
            base = 3;
            goto nosign;
        case 'X':
            digs = (char*) "0123456789ABCDEF";
            goto hex;
        case 'x':
            digs = (char*) "0123456789abcdef";
        hex:
            _ulong = UARG();
            base = 2;
            /* leading 0x/X only if non-zero */
            if ((flags & ALT) && _ulong != 0) {
                flags |= HEXPREFIX;
            }
            /* unsigned conversions */
        nosign:
            sign = '\0';
            /*
             * ``... diouXx conversions ... if a precision is
             * specified, the 0 flag will be ignored.''
             *	-- ANSI X3.159-1989
             */
        number:
            if ((dprec = prec) >= 0) {
                flags &= ~ZEROPAD;
            }
            /*
             * ``The result of converting a zero value with an
             * explicit precision of zero is no characters.''
             *	-- ANSI X3.159-1989
             */
            t = buf + BUF;
            if (_ulong != 0 || prec != 0) {
                switch (base) {
                case 0: /* octal */
                    do {
                        *--t = to_char(_ulong & 7);
                        _ulong >>= 3;
                    } while (_ulong);
                    /* handle octal leading 0 */
                    if ((flags & ALT) && *t != '0') {
                        *--t = '0';
                    }
                    break;
                case 1: /* decimal */
                    /* many numbers are 1 digit */
                    while (_ulong >= 10) {
                        *--t = to_char(_ulong % 10);
                        _ulong /= 10;
                    }
                    *--t = to_char(_ulong);
                    break;
                case 2: /* hex */
                    do {
                        *--t = digs[_ulong & 15];
                        _ulong >>= 4;
                    } while (_ulong);
                    break;
                case 3: /* binary */
                    do {
                        *--t = to_char(_ulong & 1);
                        _ulong >>= 1;
                    } while (_ulong);
                    break;
                default:
                    t = (char*) "bug in vsprintf: bad base.";
                    size = strlen(t);
                    goto emit;
                }
            }
            size = buf + BUF - t;
            break;
        default: /* "%?" prints ?, unless ? is NUL */
            if (ch == '\0') {
                goto done;
            }
            /* pretend it was %c with argument ch */
            t = buf;
            *t = ch;
            size = 1;
            sign = '\0';
            break;
        }

    emit:
        fieldsz = size + fpprec;
        if (sign) {
            fieldsz++;
        } else if (flags & HEXPREFIX) {
            fieldsz += 2;
        }
        realsz = dprec > fieldsz ? dprec : fieldsz;

        if ((flags & (LADJUST | ZEROPAD)) == 0) {
            PAD(width - realsz, blanks);
        }
        if (sign) {
            PRINT(&sign, 1);
        } else if (flags & HEXPREFIX) {
            ox[0] = '0';
            ox[1] = ch;
            PRINT(ox, 2);
        }
        if ((flags & (LADJUST | ZEROPAD)) == ZEROPAD) {
            PAD(width - realsz, zeroes);
        }
        PAD(dprec - fieldsz, zeroes);
        PRINT(t, size);
        PAD(fpprec, zeroes);
        if (flags & LADJUST) {
            PAD(width - realsz, blanks);
        }
        if (width > realsz) {
            realsz = width;
        }
        *str = '\0';
        cnt += realsz;
    }
done:
    *str = '\0';
    return strlen(s0);
}
