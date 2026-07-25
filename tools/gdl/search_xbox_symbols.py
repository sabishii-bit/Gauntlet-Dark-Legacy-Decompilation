#!/usr/bin/env python3

"""
Search Xbox PDB symbols by various criteria.

Useful for cross-referencing GameCube structures with Xbox debug symbols.

Usage:
    # Find all types of a specific size
    python tools/search_xbox_symbols.py --size 0xc

    # Find types by name pattern
    python tools/search_xbox_symbols.py --name vec

    # Find types in a specific category
    python tools/search_xbox_symbols.py --category math

    # Combine filters
    python tools/search_xbox_symbols.py --size 0x10 --category graphics
"""

import argparse
import re
from pathlib import Path
from typing import List, Dict, Tuple


def parse_type_definition(lines: List[str], start_idx: int) -> Tuple[Dict, int]:
    """Parse a single type definition and return metadata + end index."""
    first_line = lines[start_idx]

    # Extract type keyword, name, and size
    match = re.match(r'^(struct|class|enum|union)\s+([A-Za-z_][A-Za-z0-9_:<>]*)(.*?)//\s*Size=(0x[0-9a-fA-F]+)', first_line)
    if not match:
        return None, start_idx + 1

    keyword = match.group(1)
    name = match.group(2)
    size_hex = match.group(4)
    size_int = int(size_hex, 16)

    # Find the end of this definition
    brace_count = first_line.count('{') - first_line.count('}')
    end_idx = start_idx + 1

    if keyword == 'enum':
        # Enums end at };
        while end_idx < len(lines):
            line = lines[end_idx]
            if line.strip().startswith('};'):
                end_idx += 1
                break
            end_idx += 1
    else:
        # Structs/classes/unions - track braces
        while end_idx < len(lines) and brace_count > 0:
            line = lines[end_idx]
            brace_count += line.count('{') - line.count('}')
            end_idx += 1

    definition = ''.join(lines[start_idx:end_idx])

    return {
        'keyword': keyword,
        'name': name,
        'size': size_int,
        'size_hex': size_hex,
        'definition': definition,
        'line_count': end_idx - start_idx
    }, end_idx


def search_symbols(symbols_dir: Path, size_filter: int = None, name_filter: str = None, category_filter: str = None):
    """Search through Xbox symbols with various filters."""

    if not symbols_dir.exists():
        print(f"Error: {symbols_dir} does not exist")
        print("Have you run: python tools/split_pdb_header.py misc/Xbox/shell3D.h ?")
        return

    results = []

    # Determine which files to search
    if category_filter:
        header_files = [symbols_dir / f"{category_filter}.h"]
    else:
        header_files = list(symbols_dir.glob("*.h"))

    for header_file in header_files:
        if not header_file.exists():
            continue

        category = header_file.stem
        print(f"Searching {category}.h...", end=' ')

        with open(header_file, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()

        idx = 0
        count = 0
        while idx < len(lines):
            line = lines[idx]

            # Check if this is a type definition
            if not line.startswith(('struct ', 'class ', 'enum ', 'union ')):
                idx += 1
                continue

            # Parse this type
            type_info, next_idx = parse_type_definition(lines, idx)
            if type_info is None:
                idx = next_idx
                continue

            # Apply filters
            if size_filter is not None and type_info['size'] != size_filter:
                idx = next_idx
                continue

            if name_filter and name_filter.lower() not in type_info['name'].lower():
                idx = next_idx
                continue

            # This type matches all filters
            type_info['category'] = category
            type_info['file'] = header_file.name
            results.append(type_info)
            count += 1

            idx = next_idx

        print(f"found {count} matching types")

    return results


def display_results(results: List[Dict], verbose: bool = False):
    """Display search results in a readable format."""

    if not results:
        print("\nNo matching types found.")
        return

    print(f"\nFound {len(results)} matching type(s):\n")

    # Sort by size, then name
    results.sort(key=lambda x: (x['size'], x['name']))

    for i, result in enumerate(results, 1):
        print(f"{i}. {result['keyword']} {result['name']}")
        print(f"   Size: {result['size_hex']} ({result['size']} bytes)")
        print(f"   Category: {result['category']} ({result['file']})")

        if verbose:
            print(f"   Lines: {result['line_count']}")
            print(f"\n   Definition:")
            # Print definition with indentation
            for line in result['definition'].split('\n'):
                if line.strip():
                    print(f"   {line}")
            print()
        else:
            # Just show first line
            first_line = result['definition'].split('\n')[0]
            print(f"   {first_line}")
            print()


def size_summary(symbols_dir: Path):
    """Generate a summary of types grouped by size."""
    print("Generating size summary...")

    all_types = search_symbols(symbols_dir)
    if not all_types:
        return

    # Group by size
    by_size = {}
    for type_info in all_types:
        size = type_info['size']
        if size not in by_size:
            by_size[size] = []
        by_size[size].append(type_info)

    # Display summary
    print(f"\nSize Distribution ({len(all_types)} total types):\n")
    print(f"{'Size (hex)':<12} {'Size (dec)':<12} {'Count':<8} Sample Types")
    print("=" * 80)

    for size in sorted(by_size.keys()):
        types_at_size = by_size[size]
        hex_size = f"0x{size:x}"
        sample_names = ', '.join(t['name'] for t in types_at_size[:3])
        if len(types_at_size) > 3:
            sample_names += f", ... (+{len(types_at_size) - 3} more)"

        print(f"{hex_size:<12} {size:<12} {len(types_at_size):<8} {sample_names}")

    # Highlight common GameCube sizes
    print("\n\nCommon GameCube struct sizes to investigate:")
    interesting_sizes = [0x4, 0x8, 0xc, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x30, 0x40]
    for size in interesting_sizes:
        if size in by_size:
            count = len(by_size[size])
            print(f"  0x{size:x} ({size} bytes): {count} types")


def main():
    parser = argparse.ArgumentParser(
        description='Search Xbox PDB symbols',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Find all 12-byte structs (possible vec3)
  python tools/search_xbox_symbols.py --size 0xc

  # Find types with 'vector' in the name
  python tools/search_xbox_symbols.py --name vector

  # Show size distribution
  python tools/search_xbox_symbols.py --summary

  # Detailed view of math types
  python tools/search_xbox_symbols.py --category math --verbose
        """
    )

    parser.add_argument('--size', type=str, help='Filter by size (hex, e.g., 0xc or 12)')
    parser.add_argument('--name', type=str, help='Filter by name (substring match)')
    parser.add_argument('--category', type=str, help='Filter by category (e.g., math, graphics)')
    parser.add_argument('--verbose', '-v', action='store_true', help='Show full type definitions')
    parser.add_argument('--summary', action='store_true', help='Show size distribution summary')
    parser.add_argument('--symbols-dir', type=Path, default=Path('include/xbox_symbols'),
                        help='Path to xbox symbols directory')

    args = parser.parse_args()

    if args.summary:
        size_summary(args.symbols_dir)
        return

    # Parse size if provided
    size_filter = None
    if args.size:
        if args.size.startswith('0x'):
            size_filter = int(args.size, 16)
        else:
            size_filter = int(args.size)

    # Search
    results = search_symbols(
        args.symbols_dir,
        size_filter=size_filter,
        name_filter=args.name,
        category_filter=args.category
    )

    # Display
    display_results(results, verbose=args.verbose)


if __name__ == '__main__':
    main()
