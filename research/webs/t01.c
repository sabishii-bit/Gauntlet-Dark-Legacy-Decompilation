/* T1: two params live across a call */
extern int g(int);
int f(int a, int b) { g(0); return a + b; }
