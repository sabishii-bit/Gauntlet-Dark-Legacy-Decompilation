#!/usr/bin/env python3
"""
Split asm.s into separate section files based on DOL section table
"""

import os
import sys

# Section mapping based on dtk dol info output
# Maps (start_address, end_address) to section name
SECTIONS = [
    (0x80003100, 0x800054E0, '.init'),
    (0x800054E0, 0x800087A0, 'extab'),
    (0x800087A0, 0x8000CF40, 'extabindex'),
    (0x8000CF40, 0x801106E0, '.text'),
    (0x801106E0, 0x80110700, '.ctors'),
    (0x80110700, 0x80110720, '.dtors'),
    (0x80110720, 0x80118100, '.rodata'),
    (0x80118100, 0x8023CA40, '.data'),
    (0x8023CA40, 0x80343B00, '.bss'),
    (0x80343B00, 0x80344160, '.sdata'),
    (0x80344160, 0x80345718, '.sbss'),
    (0x80345720, 0x80349720, '.data9'),
]

def find_section_for_address(address):
    """Find which section an address belongs to"""
    for start, end, name in SECTIONS:
        if start <= address < end:
            return name
    return None

def parse_section_header(line):
    """Parse a .section directive to extract address range"""
    # Example: .section .text0, "ax"  # 0x80003100 - 0x800054E0
    if '# 0x' not in line:
        return None, None, None

    parts = line.split('#')[1].strip().split('-')
    if len(parts) != 2:
        return None, None, None

    start_addr = int(parts[0].strip(), 16)
    end_addr = int(parts[1].strip(), 16)
    section_name = find_section_for_address(start_addr)

    return start_addr, end_addr, section_name

def split_asm_file(input_file, output_dir):
    """Split asm.s into separate section files"""

    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)

    current_section = None
    current_file = None
    line_count = 0

    print(f"Reading {input_file}...")

    # Detect encoding - try UTF-16 LE first (has BOM 0xFF 0xFE)
    with open(input_file, 'rb') as fb:
        bom = fb.read(2)
        encoding = 'utf-16-le' if bom == b'\xff\xfe' else 'utf-8'

    print(f"  Detected encoding: {encoding}")

    with open(input_file, 'r', encoding=encoding, errors='ignore') as f:
        for line in f:
            line_count += 1

            # Check if this is a section directive
            if line.strip().startswith('.section'):
                # Close previous file if open
                if current_file:
                    current_file.close()
                    print(f"  Closed {current_section}.s")

                # Parse section header
                start_addr, end_addr, section_name = parse_section_header(line)

                if section_name:
                    current_section = section_name
                    output_path = os.path.join(output_dir, f"{section_name}.s")
                    current_file = open(output_path, 'w', encoding='utf-8')
                    print(f"  Creating {section_name}.s (0x{start_addr:08X} - 0x{end_addr:08X})")

                    # Write corrected section header
                    attrs = '"ax"' if section_name in ['.init', '.text'] else '"wa"'
                    current_file.write(f'.section {section_name}, {attrs}  # 0x{start_addr:08X} - 0x{end_addr:08X}\n')
                else:
                    print(f"Warning: Could not determine section name at line {line_count}")
                    current_section = None
                    current_file = None
            else:
                # Write line to current section file
                if current_file:
                    current_file.write(line)

    # Close last file
    if current_file:
        current_file.close()
        print(f"  Closed {current_section}.s")

    print(f"\nProcessed {line_count} lines")
    print(f"Output files created in {output_dir}/")

if __name__ == '__main__':
    input_file = 'asm/asm.s'
    output_dir = 'asm/sections'

    if len(sys.argv) > 1:
        input_file = sys.argv[1]
    if len(sys.argv) > 2:
        output_dir = sys.argv[2]

    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' not found")
        sys.exit(1)

    split_asm_file(input_file, output_dir)
    print("\nDone!")
