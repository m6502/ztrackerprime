#!/usr/bin/env bash
# fetch-extlibs.sh
#
# Populate extlibs/libpng/ and extlibs/zlib/ with the exact files that
# CMakeLists.txt (lines ~38-62) references. The upstream repo does NOT ship
# these sources -- the maintainer's local tree has curated copies dropped in
# manually. CI must reproduce that layout on every run.
#
# Works on GitHub Actions runners with bash available (Linux, macOS, and
# Windows via git-bash which provides curl + tar).
#
# Env vars (supplied by the workflow, with sane defaults here):
#   LIBPNG_VERSION   default 1.6.44
#   ZLIB_VERSION     default 1.3.1
#
# Layout produced:
#   extlibs/libpng/png.c, pngerror.c, ..., png.h, pnglibconf.h, ...
#   extlibs/zlib/adler32.c, ..., zlib.h, zconf.h, ...
#
# Notes on the libpng / zlib source layouts:
#   * libpng's tarball top-level already contains all the .c/.h files we need
#     directly -- we just strip the top-level versioned dir on extract.
#   * libpng does NOT ship pnglibconf.h; it ships scripts/pnglibconf.h.prebuilt
#     which is the canonical prebuilt copy. Rename it.
#   * zlib's tarball ships zconf.h already in the top-level (no .in step
#     needed for a source drop; the .in is used only when running its own
#     configure). We verify that and fall back to zconf.h.in -> zconf.h if a
#     future upstream rearrangement ever removes the pregenerated copy.

set -euo pipefail

: "${LIBPNG_VERSION:=1.6.44}"
: "${ZLIB_VERSION:=1.3.1}"

root="$(pwd)"
workdir="$root/.extlibs-download"
mkdir -p "$workdir"

echo "=== Fetching libpng $LIBPNG_VERSION ==="
libpng_tarball="$workdir/libpng-$LIBPNG_VERSION.tar.gz"
if [ ! -f "$libpng_tarball" ]; then
  # SourceForge mirrors return transient 502s. Try multiple sources with
  # retry/backoff — same pattern as zlib below.
  urls=(
    "https://download.sourceforge.net/libpng/libpng-${LIBPNG_VERSION}.tar.gz"
    "https://prdownloads.sourceforge.net/libpng/libpng-${LIBPNG_VERSION}.tar.gz"
    "https://github.com/pnggroup/libpng/archive/refs/tags/v${LIBPNG_VERSION}.tar.gz"
    "https://download.savannah.nongnu.org/releases/libpng/libpng-${LIBPNG_VERSION}.tar.gz"
  )
  ok=0
  for u in "${urls[@]}"; do
    echo "  trying: $u"
    if curl -fL --retry 5 --retry-delay 5 --retry-all-errors \
            --connect-timeout 30 --max-time 300 \
            -o "$libpng_tarball" "$u"; then
      ok=1
      break
    fi
    rm -f "$libpng_tarball"
  done
  if [ "$ok" -ne 1 ]; then
    echo "ERROR: could not fetch libpng $LIBPNG_VERSION from any known mirror" >&2
    exit 1
  fi
fi

rm -rf "$root/extlibs/libpng"
mkdir -p "$root/extlibs/libpng"
tar xf "$libpng_tarball" -C "$root/extlibs/libpng" --strip-components=1

# libpng does not ship pnglibconf.h -- it's generated. Use the prebuilt copy.
if [ ! -f "$root/extlibs/libpng/pnglibconf.h" ]; then
  if [ -f "$root/extlibs/libpng/scripts/pnglibconf.h.prebuilt" ]; then
    cp "$root/extlibs/libpng/scripts/pnglibconf.h.prebuilt" \
       "$root/extlibs/libpng/pnglibconf.h"
  else
    echo "ERROR: libpng source lacks both pnglibconf.h and scripts/pnglibconf.h.prebuilt" >&2
    exit 1
  fi
fi

echo "=== Fetching zlib $ZLIB_VERSION ==="
zlib_tarball="$workdir/zlib-$ZLIB_VERSION.tar.gz"
if [ ! -f "$zlib_tarball" ]; then
  # zlib.net keeps only the current release at the top-level URL; older
  # versions move to /fossils/. Try GitHub release (stable, versioned) first,
  # then zlib.net/fossils, then zlib.net root as a last resort.
  urls=(
    "https://github.com/madler/zlib/releases/download/v${ZLIB_VERSION}/zlib-${ZLIB_VERSION}.tar.gz"
    "https://www.zlib.net/fossils/zlib-${ZLIB_VERSION}.tar.gz"
    "https://www.zlib.net/zlib-${ZLIB_VERSION}.tar.gz"
  )
  ok=0
  for u in "${urls[@]}"; do
    echo "  trying: $u"
    if curl -fL --retry 5 --retry-delay 5 --retry-all-errors \
            --connect-timeout 30 --max-time 300 \
            -o "$zlib_tarball" "$u"; then
      ok=1
      break
    fi
    rm -f "$zlib_tarball"
  done
  if [ "$ok" -ne 1 ]; then
    echo "ERROR: could not fetch zlib $ZLIB_VERSION from any known mirror" >&2
    exit 1
  fi
fi

rm -rf "$root/extlibs/zlib"
mkdir -p "$root/extlibs/zlib"
tar xf "$zlib_tarball" -C "$root/extlibs/zlib" --strip-components=1

# zlib ships zconf.h pregenerated in recent releases. If an upstream change
# ever drops it, fall back to zconf.h.in (which is acceptable for the curated
# subset of zlib ztracker uses).
if [ ! -f "$root/extlibs/zlib/zconf.h" ] && [ -f "$root/extlibs/zlib/zconf.h.in" ]; then
  cp "$root/extlibs/zlib/zconf.h.in" "$root/extlibs/zlib/zconf.h"
fi

echo "=== Verifying required sources ==="
required=(
  # libpng C sources referenced by CMakeLists.txt
  extlibs/libpng/png.c
  extlibs/libpng/pngerror.c
  extlibs/libpng/pngget.c
  extlibs/libpng/pngmem.c
  extlibs/libpng/pngpread.c
  extlibs/libpng/pngread.c
  extlibs/libpng/pngrio.c
  extlibs/libpng/pngrtran.c
  extlibs/libpng/pngrutil.c
  extlibs/libpng/pngset.c
  extlibs/libpng/pngtrans.c
  extlibs/libpng/pngwio.c
  extlibs/libpng/pngwrite.c
  extlibs/libpng/pngwtran.c
  extlibs/libpng/pngwutil.c
  # libpng headers
  extlibs/libpng/png.h
  extlibs/libpng/pngconf.h
  extlibs/libpng/pngdebug.h
  extlibs/libpng/pnginfo.h
  extlibs/libpng/pnglibconf.h
  extlibs/libpng/pngpriv.h
  extlibs/libpng/pngstruct.h
  # zlib C sources referenced by CMakeLists.txt
  extlibs/zlib/adler32.c
  extlibs/zlib/compress.c
  extlibs/zlib/crc32.c
  extlibs/zlib/deflate.c
  extlibs/zlib/infback.c
  extlibs/zlib/inffast.c
  extlibs/zlib/inflate.c
  extlibs/zlib/inftrees.c
  extlibs/zlib/trees.c
  extlibs/zlib/zutil.c
  # zlib headers (transitive)
  extlibs/zlib/zlib.h
  extlibs/zlib/zconf.h
  extlibs/zlib/zutil.h
  extlibs/zlib/deflate.h
  extlibs/zlib/inflate.h
  extlibs/zlib/inffast.h
  extlibs/zlib/inffixed.h
  extlibs/zlib/inftrees.h
  extlibs/zlib/crc32.h
  extlibs/zlib/trees.h
)

missing=0
for f in "${required[@]}"; do
  if [ ! -f "$root/$f" ]; then
    echo "  MISSING: $f" >&2
    missing=1
  fi
done

if [ "$missing" -ne 0 ]; then
  echo "ERROR: one or more required extlib files are missing after extraction" >&2
  exit 1
fi

echo "=== extlibs OK (libpng $LIBPNG_VERSION, zlib $ZLIB_VERSION) ==="
