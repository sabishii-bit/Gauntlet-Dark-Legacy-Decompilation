#!/usr/bin/env python3
"""
pragmagen.py

Generates a C++ file with #pragma GLOBAL_ASM directives for each function
found in an assembly file.

Usage:
    python pragmagen.py <input.s> <output.cpp> [--relative-path <path>]

Example:
    python pragmagen.py asm/game/functions.s src/game/functions.cpp
    python pragmagen.py asm/game/functions.s src/game/functions.cpp --relative-path asm/game/functions.s
"""

import argparse
import re
from pathlib import Path


def get_asm_functions(fileText):
    """
    Extract all function labels from assembly file.
    Filters out data labels (like lbl_*) and only returns function labels.
    """
    labelRegex = r"\b.+:"
    matches = re.findall(labelRegex, fileText)
    # Filter out data labels (lbl_*) and only keep function labels
    return list(filter(lambda x: "lbl_" not in x, matches))


def generate_cpp_with_pragmas(asm_path, functions, relative_asm_path=None):
    """
    Generate C++ file content with #pragma GLOBAL_ASM directives for each function.

    Args:
        asm_path: Path to the assembly file
        functions: List of function labels (with colons)
        relative_asm_path: Optional relative path to use in pragmas (from build directory)

    Returns:
        String containing the generated C++ code
    """
    # Use relative path if provided, otherwise use the asm_path as-is
    pragma_path = relative_asm_path if relative_asm_path else str(asm_path)

    # Convert Windows paths to forward slashes for consistency
    pragma_path = pragma_path.replace("\\", "/")

    output = []
    output.append("// Generated from: " + str(asm_path))
    output.append("// This file contains #pragma GLOBAL_ASM directives for inline assembly")
    output.append("")

    # Add each function as a pragma
    for func_label in functions:
        # Remove the trailing colon from the label
        func_name = func_label.rstrip(":")

        # Generate the pragma directive
        pragma_line = f'#pragma GLOBAL_ASM("{pragma_path}", "{func_name}")'
        output.append(pragma_line)

    return "\n".join(output)


def main():
    parser = argparse.ArgumentParser(
        description="Generate C++ file with #pragma GLOBAL_ASM directives from assembly file",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python pragmagen.py asm/game/functions.s src/game/functions.cpp
  python pragmagen.py asm/game/functions.s src/game/functions.cpp --relative-path asm/game/functions.s
        """
    )

    parser.add_argument(
        "input",
        help="Input assembly file (.s)"
    )

    parser.add_argument(
        "output",
        help="Output C++ file (.cpp)"
    )

    parser.add_argument(
        "-r", "--relative-path",
        help="Relative path to assembly file to use in pragmas (default: use input path)",
        default=None
    )

    args = parser.parse_args()

    # Read input assembly file
    asm_path = Path(args.input)
    if not asm_path.exists():
        print(f"Error: Assembly file '{asm_path}' does not exist")
        return 1

    print(f"Reading assembly file: {asm_path}")
    asm_text = asm_path.read_text()

    # Extract all function labels
    functions = get_asm_functions(asm_text)

    if not functions:
        print(f"Warning: No functions found in {asm_path}")
        return 1

    print(f"Found {len(functions)} function(s):")
    for func in functions:
        print(f"  - {func.rstrip(':')}")

    # Generate C++ file with pragmas
    cpp_content = generate_cpp_with_pragmas(
        asm_path,
        functions,
        args.relative_path
    )

    # Write output file
    output_path = Path(args.output)

    # Check if output path is a directory
    if output_path.exists() and output_path.is_dir():
        print(f"Error: Output path '{output_path}' is a directory, not a file")
        print(f"Please specify a .cpp filename, e.g., {output_path / 'output.cpp'}")
        return 1

    # Create parent directories if they don't exist
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(cpp_content)

    print(f"\nGenerated C++ file: {output_path}")
    print(f"Total pragmas: {len(functions)}")

    return 0


if __name__ == "__main__":
    exit(main())
