/* T6: param conditionally overwritten before call (like parent), plus flags-style test */
extern int g(int, int);
int f(int a, unsigned flags) {
    int x;
    if (a == 0) { a = (flags & 0x2000) ? 7 : 9; }
    x = g(a, 2);
    g(x, 3);
    return x + (int)flags;
}
