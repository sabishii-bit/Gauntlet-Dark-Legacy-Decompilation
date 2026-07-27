/* m05: minimal three-web: two param copies + call-result local live across a later call */
extern int g(int);
extern void h(void);
int f(int a, int b) {
    int x;
    x = g(0);
    if (x) {
        h();
        return x + a + b;
    }
    return x + a + b;
}
