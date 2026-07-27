/* T4: x used more often than a - does use count reorder colors? */
extern int g(int);
int f(int a) {
    int x;
    x = g(0);
    g(1);
    return x + x * x + a;
}
