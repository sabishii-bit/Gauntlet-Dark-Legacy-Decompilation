#!/bin/sh
# webs harness: compile a tiny .c with project cflags_demo (GC/1.2.5) and dump asm
WT=/w/Temp/claude/webs_wt
CC="$WT/build/compilers/GC/1.2.5/mwcceppc.exe"
OBJDUMP="$WT/build/binutils/powerpc-eabi-objdump.exe"
SRC="$1"
BASE="${SRC%.c}"
cd "$WT/research/webs" || exit 1
$CC -nodefaults -proc gekko -align powerpc -enum int -fp hardware \
  -Cpp_exceptions on -O4 -inline auto -pragma "cats off" \
  -pragma "warn_notinlined off" -maxerrors 1 -nosyspath -RTTI off \
  -fp_contract on -str reuse,readonly -multibyte -DNDEBUG=1 \
  -c "$SRC" -o "$BASE.o" || exit 1
$OBJDUMP -dr "$BASE.o"
