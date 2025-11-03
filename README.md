# Gauntlet Dark Legacy Decompilation

This project contains a WIP decompilation of Gauntlet Dark Legacy (NTSC-U) for the GameCube.

It builds the following DOL:

main.dol: sha1: `7CBA77AA496EB0FC5FFEC60EFD9680AA9635D679`

## Building with Docker

If you don't want to set up the build environment locally, you can use Docker to build the project.

### Prerequisites

You must have the CodeWarrior MWCC compiler files in place before building:

1. Obtain CodeWarrior 1.2.5 (mwcceppc.exe and mwldeppc.exe)
2. Create the directory: `tools/mwcc_compiler/1.2.5/`
3. Place the compiler executables in that directory
4. See [tools/mwcc_compiler/README.md](tools/mwcc_compiler/README.md) for details

### Build the Docker image:
```bash
docker build -t gdl-build .
```

### Extract the built main.dol:
```bash
docker create --name gdl-temp gdl-build
docker cp gdl-temp:/build/build/GUNE5D/main.dol ./main.dol

# DOL file extracted, clean-up unnecessary containers
docker rm gdl-temp
docker rm gdl-build
```

The build process will automatically verify the SHA1 signature and report whether it matches the original. The build will succeed regardless of the SHA1 result, allowing you to build modified versions.

### Alternative: Run interactively
```bash
docker run --rm -it gdl-build sh
# Inside the container, the built DOL is at: /build/build/GUNE5D/main.dol
```
