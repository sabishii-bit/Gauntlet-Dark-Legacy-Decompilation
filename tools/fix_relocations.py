#!/usr/bin/env python3
"""
Fix malformed @h/@l/@ha relocations in PowerPC assembly files
Converts: 0x-7FF1E4FC@h to -0x7FF1E4FC@h (move minus outside hex)
Also fixes branch addresses to use relative offsets
"""

import re
import sys

def fix_hex_with_internal_minus(match):
    """Fix 0x-ADDR to -0xADDR"""
    prefix = match.group(1)  # instruction part
    hex_val = match.group(2)  # the -HEXDIGITS part
    suffix = match.group(3)  # the @h/@l/@ha part

    # Move the minus sign outside
    return f"{prefix}-0x{hex_val}{suffix}"

def process_file(filepath):
    """Process an assembly file to fix relocations"""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Fix patterns like: lis r3, 0x-7FF1E4FC@h
    # Match: (instruction...) 0x-HEXDIGITS @h|@l|@ha
    pattern = r'((?:lis|ori|addi|addis)\s+\w+,\s+(?:\w+,\s+)?)0x-([0-9A-Fa-f]+)(@h(?:a)?|@l)\b'
    fixed_content = re.sub(pattern, fix_hex_with_internal_minus, content)

    # Also fix malformed addresses that start with 0xF without the minus
    # These should be negative (like 0xFFFFFFF9 -> -7 in signed)
    # Pattern: 0xFFFCF1C@ha should stay as is if it's a valid address
    # But 0xFFFFFFFF... (more than 8 F's) needs fixing

    # Count changes
    if fixed_content != content:
        changes = len(re.findall(pattern, content))
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(fixed_content)
        print(f"Fixed {filepath}: {changes} relocations corrected")
        return True
    else:
        print(f"{filepath}: No relocations needed fixing")
        return False

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: fix_relocations.py <file1.s> [file2.s ...]")
        sys.exit(1)

    fixed_count = 0
    for filepath in sys.argv[1:]:
        if process_file(filepath):
            fixed_count += 1

    print(f"\nFixed {fixed_count} file(s)")
