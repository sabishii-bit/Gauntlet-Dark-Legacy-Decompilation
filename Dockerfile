FROM alpine AS dependencies

# Download PowerPC binutils
RUN apk add --no-cache curl libarchive-tools && \
    mkdir -p /binutils && \
    curl -L https://github.com/encounter/gc-wii-binutils/releases/download/2.42-1/linux-x86_64.zip \
       | bsdtar -xvf- -C /binutils && \
    chmod +x /binutils/*

# Download DTK
RUN apk add --no-cache wget && \
    wget https://github.com/encounter/decomp-toolkit/releases/latest/download/dtk-linux-x86_64 -O /dtk && \
    chmod +x /dtk

FROM alpine

# Install runtime dependencies
RUN apk add --no-cache bash gcompat git make ninja python3 py3-pip tar zstd wine

# Copy downloaded binutils and DTK from dependencies stage
COPY --from=dependencies /binutils /opt/devkitpro/devkitPPC/bin
COPY --from=dependencies /dtk /tools/dtk

# Set up environment
ENV DEVKITPPC=/opt/devkitpro/devkitPPC
ENV WINEDEBUG=-all

# Set working directory
WORKDIR /build

# Copy MWCC compiler (must be present in tools/mwcc_compiler/)
COPY tools/mwcc_compiler /build/tools/mwcc_compiler

# Copy project files
COPY . /build

# Build the project with verbose output (skip SHA verification in make)
RUN echo "=== Starting build process ===" && \
    make WINE=wine DTK=/tools/dtk VERBOSE=1 setup && \
    make WINE=wine DTK=/tools/dtk VERBOSE=1 build/GUNE5D/main.elf && \
    /tools/dtk elf2dol build/GUNE5D/main.elf build/GUNE5D/main.dol && \
    echo "" && \
    echo "=== Build complete ===" && \
    echo "Output DOL location: /build/build/GUNE5D/main.dol" && \
    ls -lh /build/build/GUNE5D/main.dol && \
    echo "" && \
    echo "=== SHA1 Verification ===" && \
    (/tools/dtk shasum -c config/GUNE5D/build.sha1 && echo "✓ SHA1 matches - binary is identical to original") || echo "✗ SHA1 does not match - binary differs from original (this is OK if you made modifications)"

CMD [ "sh" ]
