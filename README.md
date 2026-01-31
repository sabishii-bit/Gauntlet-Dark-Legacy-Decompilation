Gauntlet Dark Legacy Decompilation
===================================

A work-in-progress decompilation of Gauntlet Dark Legacy for GameCube.

This project uses [decomp-toolkit](https://github.com/encounter/decomp-toolkit).


## Building

### Dependencies

- Python 3.8+
- Ninja build system
- Metrowerks CodeWarrior compilers (see [tools/mwcc_compiler/](tools/mwcc_compiler/))

### Setup

1. Extract your game disc and place `main.dol` at:
   ```
   orig/GUNE5D/sys/main.dol
   ```

2. Configure and build:
   ```bash
   python configure.py -v GUNE5D
   ninja
   ```

3. Verify the build matches:
   ```bash
   # The build will automatically verify against config/GUNE5D/build.sha1
   # If successful, you'll see: "Build succeeded!"
   ```

## Decompilation Workflow

### Understanding the Project Structure

When you run `python configure.py`, decomp-toolkit (dtk) analyzes the original DOL and:

1. **Splits it into assembly files** in `build/GUNE5D/asm/`
2. **Determines the original link order** by analyzing exception tables (extab)
3. **Generates `build/GUNE5D/config.json`** containing the complete link order
4. **Creates auto-generated object files** that will be replaced as you decompile

### How Link Order and Matching Work

**Key Concept:** Each auto-generated `.s` file represents **one original compilation unit** (object file) from the game.

When you write source code and mark it as `Matching`, the build system:
- Compiles your C/C++ code to an object file
- Compares function addresses and symbols with auto-generated objects
- **Replaces** the matching auto-generated object in the link order
- Links everything in the original order to produce a bit-perfect binary

**Example:**
```
Original link order (from config.json):
  [auto_fn_8000CF40.o, auto_fn_8000CFA0.o, auto_fn_8000D1E0.o, ...]

After you decompile and mark Game/math.c as Matching:
  [auto_fn_8000CF40.o, Game/math.o, auto_fn_8000D1E0.o, ...]
                        ^^^^^^^^^^^^
                        Replaced!
```

### Step-by-Step: Decompiling a Function

#### 1. Find a Function to Decompile

Browse the auto-generated assembly files:

```bash
# List all split functions
ls build/GUNE5D/asm/

# View a specific function
cat build/GUNE5D/asm/auto_fn_8000D034_text.s
```

Each file contains one or more functions. Check which functions are in a file:

```bash
grep -E "^\.fn" build/GUNE5D/asm/auto_fn_8000D034_text.s
```

**Important:** All functions in the same `.s` file were compiled together in the original game and must be decompiled together in the same source file.

#### 2. Create Your Source File

Create a C/C++ file under `src/`. You can organize however you want:

```bash
mkdir -p src/Game
touch src/Game/math.c
```

Write your decompiled code:

```c
// src/Game/math.c
#include "types.h"

// Function at 0x8000D034
float CalculateDistance(Vec3* a, Vec3* b, Vec3* c, Vec3* d, float threshold) {
    // Your reverse-engineered code here
}
```

#### 3. Configure the Object in `configure.py`

Edit [configure.py](configure.py) and add your source file to `config.libs`:

```python
config.libs = [
    # Existing libraries...
    {
        "lib": "Runtime.PPCEABI.H",
        "mw_version": config.linker_version,
        "cflags": cflags_runtime,
        "progress_category": "sdk",
        "objects": [
            Object(NonMatching, "Runtime.PPCEABI.H/global_destructor_chain.c"),
            Object(NonMatching, "Runtime.PPCEABI.H/__init_cpp_exceptions.cpp"),
        ],
    },

    # Your game code
    {
        "lib": "Game",                   # Files go in src/Game/
        "mw_version": "GC/1.2.5n",       # Compiler version
        "cflags": cflags_base,           # Compiler flags
        "progress_category": "game",     # Progress tracking category
        "objects": [
            Object(NonMatching, "Game/math.c"),  # Start as NonMatching
        ],
    },
]
```

**Understanding `config.libs`:**
- **`lib`**: Subdirectory name under `src/` (e.g., `"Game"` → `src/Game/`)
- **`mw_version`**: Which Metrowerks compiler version to use
- **`cflags`**: Compiler flags (optimization level, etc.)
- **`objects`**: List of source files with their matching status

**Object Status Types:**
- `NonMatching`: Doesn't match yet, excluded from final build by default
- `Matching`: Assembly matches perfectly, included in final build
- `Equivalent`: Functionally equivalent but different assembly (rare)

#### 4. Build and Compare

Reconfigure and build:

```bash
python configure.py -v GUNE5D
ninja
```

This compiles your C code to `build/GUNE5D/obj/Game/math.o`.

#### 5. Use objdiff to Match

Open objdiff and load the project (`objdiff.json` is auto-generated):

```bash
# On Windows
objdiff

# On Linux with GUI
objdiff
```

Select your function (`fn_8000D034` or `CalculateDistance` if you renamed it) to see:
- **Left pane:** Original assembly from the game
- **Right pane:** Your decompiled code's assembly
- **Diff view:** Highlighted differences

#### 6. Iterate Until Matching

Adjust your C code based on the assembly differences. Common techniques:

- **Reorder operations** (especially floating-point math)
- **Use specific types** (signed vs unsigned, int vs long)
- **Adjust loop conditions** (do-while vs while)
- **Match register allocation** (declare variables in specific order)
- **Use inline assembly** for stubborn instructions (last resort)

Useful resources:
- [decomp.me](https://decomp.me) - Collaborate on matches online
- [Discord: GameCube Decompilation](https://discord.gg/hKx3FJJgrV) - Ask for help in `#dtk`

#### 7. Mark as Matching

Once objdiff shows 100% match (green), update [configure.py](configure.py):

```python
Object(Matching, "Game/math.c"),  # Changed from NonMatching!
```

Rebuild to verify:

```bash
python configure.py -v GUNE5D
ninja
```

If the final SHA-1 hash still matches, your decompilation is correct!

#### 8. Rename Symbols (Optional)

Give functions meaningful names in `config/GUNE5D/symbols.txt`:

```
CalculateDistance = .text:0x8000D034; // type:function size:0x1AC
```

Reconfigure to apply the rename:

```bash
python configure.py -v GUNE5D
ninja
```

### Building with Non-Matching Code

To include non-matching code in the build (useful for testing):

```bash
python configure.py -v GUNE5D --non-matching
ninja
```

This links both `Matching` and `NonMatching` objects, but the final hash won't match.

### Understanding One File = One Object

**Critical Rule:** Each source file must correspond to **exactly one** original compilation unit.

If `auto_fn_8000D034_text.s` contains three functions (at 0x8000D034, 0x8000D100, 0x8000D200), your source file must also contain all three:

```c
// src/Game/math.c - All functions from auto_fn_8000D034_text.s
void fn_8000D034() { /* ... */ }
void fn_8000D100() { /* ... */ }
void fn_8000D200() { /* ... */ }
```

When compiled, your `math.o` will replace `auto_fn_8000D034_text.o` in the link order.

To check which functions belong together:

```bash
grep -E "^\.fn" build/GUNE5D/asm/auto_fn_8000D034_text.s
```

## Project Structure

```
.
├── config/              # Configuration for GUNE5D
│   └── GUNE5D/
│       ├── config.yml   # Main configuration
│       ├── symbols.txt  # Symbol names and addresses
│       ├── splits.txt   # Section definitions
│       └── build.sha1   # Expected hash for verification
├── src/                 # Your decompiled C/C++ source code
│   └── Game/
├── include/             # Header files
├── build/               # Build artifacts (generated, not committed)
│   └── GUNE5D/
│       ├── asm/         # Auto-generated assembly files
│       ├── obj/         # Compiled object files
│       ├── config.json  # Link order determined by dtk
│       ├── main.dol     # Final built DOL
│       └── main.elf     # Intermediate ELF
├── orig/                # Original game files (not committed)
│   └── GUNE5D/
│       └── sys/
│           └── main.dol
├── tools/               # Build system scripts
│   ├── project.py       # Main build script
│   ├── configure.py     # Configuration generator
│   └── mwcc_compiler/   # Metrowerks compilers
├── configure.py         # Project configuration script
└── build.ninja          # Generated build file
```

## Useful Commands

| Command | Description |
|---------|-------------|
| `python configure.py -v GUNE5D` | Configure and build |
| `ninja` | Build the project |
| `ninja -t clean` | Clean build artifacts |
| `python configure.py -v GUNE5D progress` | Show decompilation progress |
| `python configure.py --non-matching` | Include non-matching code in build |

## Advanced: .ctors Patching

This project includes automatic `.ctors` (constructor) section patching for cases where the linker's file order doesn't match the original. See [decomp-toolkit/CTORS_PATCHING.md](decomp-toolkit/CTORS_PATCHING.md) for technical details.

The patching is fully automatic - just build normally and it applies when needed.
