# Usage: cmake -DINPUT=<file> -DOUTPUT=<header> -DSYMBOL=<name> -P EmbedFile.cmake
# Writes a header defining `static const unsigned char SYMBOL[]` and
# `static const size_t SYMBOL_size` with the bytes of INPUT.

file(READ "${INPUT}" hex HEX)
string(LENGTH "${hex}" hex_len)
math(EXPR byte_count "${hex_len} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
string(REGEX REPLACE "((0x[0-9a-f][0-9a-f],){16})" "\\1\n" bytes "${bytes}")

set(content "// Generated from ${INPUT} by cmake/EmbedFile.cmake. Do not edit.\n#pragma once\n#include <stddef.h>\nstatic const unsigned char ${SYMBOL}[] = {\n${bytes}\n};\nstatic const size_t ${SYMBOL}_size = ${byte_count};\n")

file(WRITE "${OUTPUT}.tmp" "${content}")
file(RENAME "${OUTPUT}.tmp" "${OUTPUT}")
