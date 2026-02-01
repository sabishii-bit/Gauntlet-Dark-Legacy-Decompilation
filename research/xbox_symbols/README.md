# Xbox PDB Symbol Headers

These headers were extracted from Xbox PDB debug symbols for Gauntlet Dark Legacy.

## Categories

- **audio.h** - 301 types
- **d3d.h** - 381 types
- **game.h** - 52 types
- **graphics.h** - 42 types
- **math.h** - 37 types
- **misc.h** - 2220 types
- **ps2.h** - 80 types
- **util.h** - 11 types
- **windows_gdi.h** - 60 types
- **windows_kernel.h** - 392 types
- **xbox.h** - 45 types

## Usage for GameCube Decompilation

Since Gauntlet Dark Legacy was a multiplatform title, these Xbox symbols can help with:

1. **Struct Layouts**: Many game structs are likely identical or very similar across platforms
2. **Function Signatures**: Cross-reference function names and parameters
3. **Type Names**: Discover original type names for structs/classes
4. **Code Organization**: Understand subsystem boundaries (audio, graphics, gameplay)

### Important Notes

- **Platform Differences**: Xbox uses different graphics API (D3D vs GX), different memory layout, different endianness
- **Pointer Sizes**: Both Xbox and GameCube use 32-bit pointers
- **Alignment**: May differ - verify struct sizes against GameCube binary
- **Platform-Specific Code**: Filter out `d3d.h`, `xbox.h`, `windows_*.h` - these won't apply to GC

### Recommended Workflow

1. Focus on `game.h`, `math.h`, `graphics.h`, `audio.h` - these likely contain shared game logic
2. Compare struct sizes/offsets against your GameCube symbols
3. Use type names as hints, but verify layouts with actual GC data
4. Watch for interesting finds like PS2 types - indicates shared codebase!

### Integration with decomp-toolkit

You can use these headers to:
- Add known struct definitions to `include/types.h`
- Update `config/GUNE5D/symbols.txt` with discovered function/variable names
- Cross-reference with objdiff to identify matching functions

## Type Index

See `type_index.txt` for a complete alphabetical listing of all types and their categories.
