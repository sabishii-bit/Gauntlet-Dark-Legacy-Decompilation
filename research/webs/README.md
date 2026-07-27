# Register-web coloring micro-corpus (webs sweep, 2026-07)

Harness: `./harness.sh t01.c` compiles one file with the exact cflags_demo +
GC/1.2.5 pipeline and disassembles it. `./check.sh` prints the flags-copy /
obj-copy register picks for the MBNewObject-shaped m-series.

Series (findings distilled in docs/matching-recipes.md "Register-web
coloring laws"):
- t01-t06: baseline law — nonvolatile colors ascend in web-creation order
  (params in param order, then call-crossing locals by first def);
  use counts irrelevant (t04/t05).
- m01-m17: MBNewObject bisect. m01 = repo shape (rotated); the flip factor
  is the else-arm romobj expression (m07 store-a-variable = baseline,
  m08/m13/m15 any expression temp = rotated). h1.c = the answer: shared
  static SetObjectGuts inlined -> byte-exact target coloring.
- v1-v4: named temps / reassociation of the expression — all color-neutral.
- b/p/q series: statement reorder, arm swap, register keyword, padding
  stores — coloring responds to WHERE instructions grow (q2 pre-branch
  growth rotates deeper; q3 sibling-arm growth is neutral), proving the
  order is a whole-function property, not a local one.
