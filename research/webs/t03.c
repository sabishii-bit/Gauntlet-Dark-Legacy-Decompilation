/* T3: like T2 but local declared+assigned AFTER a's copy is forced; order of decl swapped via 2 locals */
extern int g(int);
int f(int a) {
    int x;
    int y;
    x = g(0);
    y = g(1);
    g(2);
    return x + y + a;
}
