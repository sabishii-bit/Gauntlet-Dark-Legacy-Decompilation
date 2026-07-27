/* T2: param + call-result local, both live across a second call */
extern int g(int);
int f(int a) {
    int x;
    x = g(0);
    g(1);
    return x + a;
}
