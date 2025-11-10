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


def get_asm_functions(fileText, include_global_labels=False):
    """
    Extract all function labels from assembly file.

    Args:
        fileText: The assembly file content
        include_global_labels: If True, include lbl_* labels that have .global declarations

    Returns:
        List of function labels (with colons)
    """
    # Find all labels
    labelRegex = r"\b.+:"
    matches = re.findall(labelRegex, fileText)

    if not include_global_labels:
        # Filter out all data labels (lbl_*)
        return list(filter(lambda x: "lbl_" not in x, matches))

    # Find all .global declarations
    global_regex = r"\.global\s+(\w+)"
    global_labels = set(re.findall(global_regex, fileText))

    # Filter: keep non-lbl labels OR lbl labels that are declared .global
    result = []
    for label in matches:
        label_name = label.rstrip(":")
        if "lbl_" not in label or label_name in global_labels:
            result.append(label)

    return result


def wrap_functions_with_if0(asm_text, functions):
    """
    Wrap global function blocks with .if 0 and .endif directives.
    Wraps both the .global declaration and the function implementation.

    Args:
        asm_text: The assembly file content
        functions: List of function labels to wrap

    Returns:
        Modified assembly text with functions wrapped
    """
    lines = asm_text.split('\n')
    result = []
    func_names = {func.rstrip(':'): func for func in functions}

    i = 0
    while i < len(lines):
        line = lines[i]

        # Check if this line contains a .global declaration for one of our functions
        global_match = re.match(r'\.global\s+(\w+)', line.strip())
        if global_match:
            func_name = global_match.group(1)
            if func_name in func_names:
                # Insert .if 0 before the .global declaration
                result.append('.if 0')

                # Add the .global line
                result.append(line)
                i += 1

                # Find the end of this function (next .global or end of file)
                while i < len(lines):
                    current_line = lines[i]
                    result.append(current_line)
                    i += 1

                    # Check if we hit the next .global
                    if re.match(r'\.global\s+', current_line.strip()):
                        # Back up one line since we need to process this .global
                        i -= 1
                        result.pop()
                        break

                # Insert .endif after the function
                result.append('.endif')
                continue

        result.append(line)
        i += 1

    return '\n'.join(result)


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
  python pragmagen.py asm/game/functions.s src/game/functions.cpp --include-global-labels
  python pragmagen.py asm/game/functions.s src/game/functions.cpp --wrap-asm asm/game/functions_wrapped.s
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

    parser.add_argument(
        "-g", "--include-global-labels",
        help="Include lbl_* labels that have .global declarations",
        action="store_true"
    )

    parser.add_argument(
        "-w", "--wrap-asm",
        help="Wrap global function blocks in the assembly file with .if 0/.endif directives",
        metavar="OUTPUT_ASM",
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
    functions = get_asm_functions(asm_text, args.include_global_labels)

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

    # Optionally wrap functions in assembly file with .if 0/.endif
    if args.wrap_asm:
        wrapped_asm = wrap_functions_with_if0(asm_text, functions)
        wrapped_path = Path(args.wrap_asm)

        # Create parent directories if they don't exist
        wrapped_path.parent.mkdir(parents=True, exist_ok=True)
        wrapped_path.write_text(wrapped_asm)

        print(f"Generated wrapped assembly file: {wrapped_path}")

    return 0


if __name__ == "__main__":
    exit(main())
