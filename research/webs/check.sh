#!/bin/sh
# print flags-copy reg (addi rX,r6,0) and obj reg (mr rY,r3 after bl)
for f in "$@"; do
  out=$(./harness.sh "$f" 2>/dev/null)
  fc=$(echo "$out" | grep -oE "addi *r3[01],r6" | head -1)
  ob=$(echo "$out" | grep -oE "(mr\.? *r(29|30|31),r3$|addi *r(29|30|31),r3,0)" | head -2 | tr '\n' ' ')
  echo "$f: flagscopy=[$fc] objcopy=[$ob]"
done
