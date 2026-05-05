# ztskin — zT skin compiler/decompiler

Standalone CLI utility for packing a directory of skin assets into a single
zT skin archive (zlib-compressed per-entry), or "exploding" an existing
archive back into individual files.

Originally written by Daniel Kahlin for the 2001-era zTracker source tree.
Ported to multiplatform CMake and modern C for zTracker Prime.

## Build

The tool builds as a side target alongside `zt`. From the repo root:

```sh
cmake -S . -B build
cmake --build build --target ztskin
```

The binary lands at `build/Program/ztskin` (Linux/macOS) or
`build\Program\ztskin.exe` (Windows). Pass `-DZT_BUILD_ZTSKIN=OFF` to skip
the target.

## Usage

```
ztskin [OPTION]... <skinfile> [<infile>]...

Pack <infile>s into <skinfile>, or with -e explode <skinfile> back into
its constituent files (written to the current working directory).

Options:
    -e   explode skin file
    -h   show help
```

Pack:

```sh
cd skins/myskin && ztskin ../myskin.zts skin.cfg font.png logo.png
```

Explode:

```sh
mkdir extracted && cd extracted && ztskin -e ../myskin.zts
```

## File format

All 32-bit fields are little-endian. Repeats per entry until EOF:

| Field            | Size       |
|------------------|------------|
| `namelen`        | u32        |
| `name`           | u8 × namelen, no terminator |
| `dataoffset`     | u32 — absolute file offset of payload |
| `realsize`       | u32 — uncompressed length |
| `compressedsize` | u32 — bytes between `dataoffset` and the next entry |
| `compressedflag` | u32 — 1 = zlib, 0 = raw |
| `payload`        | u8 × `compressedsize` |
