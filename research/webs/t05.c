/* T5: a used much more than x */
extern int g(int);
int f(int a) {
    int x;
    x = g(0);
    g(1);
    return x + a * a + a;
}
