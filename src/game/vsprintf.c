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
            u32 lo;
        } i;
    } u;

    u.d = d;
    if ((u.i.hi & 0x7ff00000) == 0x7ff00000) {
        return (u.i.hi & 0x000fffff) == 0 && u.i.lo == 0 ? FP_INFINITE : FP_NAN;
    }
    if ((int) (u.i.hi & 0x7ff00000) < 0x7ff00000 && (u.i.hi & 0x7ff00000) == 0) {
        return (u.i.hi & 0x000fffff) == 0 && u.i.lo == 0 ? FP_ZERO : FP_SUBNORMAL;
    }
    return FP_NORMAL;
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

    *p = fmtch;
    if (exp < 0) {
        p[1] = '-';
        exp = -exp;
    } else {
        p[1] = '+';
    }
    p += 2;
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

    if (fract == 0.0) {
        tmp = (double) to_digit(ch);
    } else {
        modf(fract * 10.0, &tmp);
    }
    if (tmp <= 4.0) {
        /* ``"%.3f", (double)-0.0004'' gives you a negative 0. */
        if (*signp == '-') {
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
    } else {
        for (;; --end) {
            if (*end == '.') {
                --end;
            }
            if (++*end <= '9') {
                return start;
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
    if (value >= 0.0) {
        *sign = 0;
        fract = value;
    } else {
        fract = -value;
        *sign = '-';
    }

    fract = modf(fract, &integer);
    t = ++startp;

    /* get an extra slot for rounding. */
    p = endp;
    if (fract < 1.0) {
        --p;
    } else {
        for (--p; integer != 0.0; ++expcnt) {
            tmp = modf(integer / 10.0, &integer);
            *p-- = to_char((int) (10.0 * (tmp + 0.005)));
        }
    }

    switch (ch) {
    case 'f':
    case 'F':
        goto f_fmt;
    case 'e':
    case 'E':
        goto e_fmt;
    case 'g':
    case 'G':
        goto g_fmt;
    default:
        goto done;
    }
f_fmt:
    goto done;
e_fmt:
    goto done;
g_fmt:
    goto done;
done:
    return (int) (t - startp);
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
    char* digs;
    char* t;
    char ox[2];
    char buf[BUF];
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
            ch = *fmt++;
            if (ch == '*') {
                prec = va_arg(ap, int);
                if (prec < 0) {
                    prec = -1;
                }
                goto rflag;
            }
            prec = 0;
            while (is_digit(ch)) {
                prec = to_digit(ch) + prec * 10;
                ch = *fmt++;
            }
            if (prec < 0) {
                prec = -1;
            }
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
            width = 0;
            do {
                width = to_digit(ch) + width * 10;
                ch = *fmt++;
            } while (is_digit(ch));
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
            break;
        case 'p':
            _ulong = (u32) va_arg(ap, void*);
            digs = (char*) "0123456789abcdef";
            flags |= HEXPREFIX;
            base = 2;
            ch = 'x';
            break;
        case 's':
            t = va_arg(ap, char*);
            if (t == 0) {
                t = (char*) "(null)";
            }
            if (prec < 0) {
                size = strlen(t);
            } else {
                char* p = memchr(t, 0, prec);

                size = prec;
                if (p != 0 && (size = p - t, prec < p - t)) {
                    size = prec;
                }
            }
            sign = '\0';
            goto emit;
        case 'U':
            flags |= LONGINT;
            /* FALLTHROUGH */
        case 'u':
            _ulong = UARG();
            base = 1;
            break;
        case 'b':
            _ulong = UARG();
            base = 3;
            break;
        case 'X':
            digs = (char*) "0123456789ABCDEF";
            goto hex;
        case 'x':
            digs = (char*) "0123456789abcdef";
        hex:
            _ulong = UARG();
            base = 2;
            if ((flags & ALT) && _ulong != 0) {
                flags |= HEXPREFIX;
            }
            break;
        default:
            if (ch == '\0') {
                goto done;
            }
            buf[0] = ch;
            t = buf;
            sign = '\0';
            size = 1;
            goto emit;
        }
        sign = '\0';

    number:
        if (prec >= 0) {
            flags &= ~ZEROPAD;
        }
        dprec = prec;
        t = buf + BUF;
        if (_ulong == 0 && prec == 0) {
            size = 0;
            goto emit0;
        }
        switch (base) {
        case 0: /* octal */
            do {
                *--t = to_char(_ulong & 7);
                _ulong >>= 3;
            } while (_ulong);
            if ((flags & ALT) && *t != '0') {
                *--t = '0';
            }
            break;
        case 1: /* decimal */
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
    emit0:
        size = buf + BUF - t;

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
        cnt += width > realsz ? width : realsz;
    }
done:
    *str = '\0';
    return strlen(s0);
}
