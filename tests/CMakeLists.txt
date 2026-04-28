# Standalone unit-test harness for zTracker.
#
# These tests deliberately avoid linking against the SDL/UI parts of the
# tree -- each test executable picks up only the headers it actually
# needs plus a tiny module_stub.cpp that re-implements the trivial
# arpeggio/midimacro constructors. This keeps `make test` instant and
# lets us verify pure-data logic (preset tables, .zt file format helpers,
# etc.) in isolation.
#
# Add a test by creating tests/test_<topic>.cpp and listing it below.

add_executable(test_presets
    test_presets.cpp
    module_stub.cpp
)
target_include_directories(test_presets PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)
target_compile_features(test_presets PRIVATE cxx_std_17)

add_test(NAME presets COMMAND test_presets)
