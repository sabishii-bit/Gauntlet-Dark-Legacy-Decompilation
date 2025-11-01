# Gauntlet Dark Legacy Decompilation

This project contains the decompilation of Gauntlet Dark Legacy for the GameCube.

## Assembly Sections

The project is organized into several assembly sections, each serving a specific purpose in the executable binary. These sections are located in the [asm/sections/](asm/sections/) directory.

### Memory Layout Sections

#### [.text](asm/sections/text.s)
**Address Range:** `0x8000CF40 - 0x801106E0`

The `.text` section contains the executable program code (functions). This is where all the actual game logic, algorithms, and function implementations reside. It's marked with the `"ax"` flags (allocatable, executable).

#### [.rodata](asm/sections/rodata.s)
**Address Range:** `0x80110720 - 0x80118100`

The `.rodata` (read-only data) section contains constant data that cannot be modified during program execution. This includes:
- String literals (error messages, debug text)
- Constant arrays
- Other immutable data referenced by the code

#### [.data](asm/sections/data.s)
**Address Range:** `0x80118100 - 0x8023CA40`

The `.data` section contains initialized global and static variables. This includes variables that have explicit initial values set in the source code. It's marked with `"wa"` flags (writable, allocatable).

#### [.bss](asm/sections/bss.s)
**Address Range:** `0x8023CA40 - 0x80343B00`

The `.bss` (Block Started by Symbol) section contains uninitialized global and static variables. This section doesn't take up space in the ROM; it's allocated at runtime and initialized to zero. Variables are represented with `.skip` directives that reserve space without storing actual data in the binary.

#### [.sdata](asm/sections/sdata.s)
**Address Range:** `0x80343B00 - 0x80344160`

The `.sdata` (small data) section contains small, frequently-accessed initialized global variables. On PowerPC architecture, these can be accessed more efficiently using the Small Data Area (SDA) with base register addressing (typically r13).

#### [.sdata2](asm/sections/sdata2.s)
Similar to `.sdata`, but for small read-only data. Often accessed via register r2 for efficient small constant access.

#### [.sbss](asm/sections/sbss.s)
The `.sbss` (small bss) section is the uninitialized counterpart to `.sdata`. It contains small uninitialized global variables that can be efficiently accessed through SDA addressing.

### Special Sections

#### [.init](asm/sections/init.s)
Contains initialization code that runs before the main program entry point. This typically includes startup routines and runtime initialization.

#### [.ctors](asm/sections/ctors.s)
Contains constructors - pointers to initialization functions that should be called before program execution (e.g., C++ global object constructors).

#### [.dtors](asm/sections/dtors.s)
Contains destructors - pointers to cleanup functions that should be called during program termination (e.g., C++ global object destructors).

#### [.extab_](asm/sections/extab_.s)
Contains exception table data used for C++ exception handling. Stores information about exception handlers and cleanup code.

#### [.extabindex_](asm/sections/extabindex_.s)
Contains the index for the exception table, allowing the runtime to quickly locate appropriate exception handlers.

## Section Organization

The sections are arranged in memory in a specific order that follows standard executable layout conventions:

1. **Code** (`.text`, `.init`) - Executable instructions
2. **Read-only data** (`.rodata`, `.sdata2`) - Constants and immutable data
3. **Initialized data** (`.data`, `.sdata`) - Variables with initial values
4. **Uninitialized data** (`.bss`, `.sbss`) - Zero-initialized variables
5. **Special sections** (`.ctors`, `.dtors`, `.extab_`, `.extabindex_`) - Metadata and runtime support

This organization allows for efficient memory usage and proper memory protection (executable vs. writable regions).
