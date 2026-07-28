# ITEMS symbol-recovery technique

For mixed GameCube/Xbox TUs, do not align names by function address order alone:
the retail GC linker reordered large portions of `ITEMS.OBJ`.

The reliable sequence used for `items.c` was:

1. Extract the complete module roster and globals from
   `research/xbox_symbols/functions_by_module.txt`.
2. Match small functions by semantic fingerprints (teardown, player-distance
   scan, generator gate, safe-rock state transitions), then validate every
   candidate from all callers in Ghidra.
3. Read the DOL bytes for string/constant labels. This immediately identified
   `KEYRING`, `CHESTSG`, `SEETHRU`, camera formats, and the lighting constants.
4. Use full data xref sets for BSS/SBSS. Writers identify table counts and
   singleton slots much more reliably than consumers.
5. Keep exact PDB names distinct from GC-only behavioral names. The latter are
   still useful, but should be explicitly documented as descriptive.

The complete old-to-new map and evidence are in
`research/items_symbol_map.md`.
