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
target_include_directories(test_presets PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_features(test_presets PRIVATE cxx_std_17)

add_test(NAME presets COMMAND test_presets)

# Pure-function tests for the preset listbox click / arrow / Space / P-cycle
# decision logic. Has no module.h dependencies -- preset_selector.h is fully
# self-contained.
add_executable(test_selector
    test_selector.cpp
)
target_include_directories(test_selector PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_features(test_selector PRIVATE cxx_std_17)
add_test(NAME selector COMMAND test_selector)

# Integration tests: simulate one editor frame (preset apply -> widget
# refresh -> slider/data sync) end-to-end. Catches the "Enter freezes"
# / "Space selects but values don't change" class of bugs by asserting
# the freshly-applied preset survives the sync block.
add_executable(test_page_sync
    test_page_sync.cpp
    module_stub.cpp
)
target_include_directories(test_page_sync PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_features(test_page_sync PRIVATE cxx_std_17)
add_test(NAME page_sync COMMAND test_page_sync)

# Tests for the global Ctrl-S dispatch. Locks in per-page behaviour
# (Keybindings page handles it itself; F4/Shift+F4 SWALLOW; everything
# else opens Save / Save-As). Regression-tests the "Ctrl-S froze the
# page" bug from the bare-break-without-consume path.
add_executable(test_save_key
    test_save_key.cpp
)
target_include_directories(test_save_key PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_features(test_save_key PRIVATE cxx_std_17)
add_test(NAME save_key COMMAND test_save_key)

# Real KeyBuffer (src/keybuffer.{h,cpp}) compiled under sdl_stub.h with
# the ZT_TEST_NO_SDL guard. Exercises the actual SDL_KMOD -> KS_*
# translation, FIFO ordering, peek-vs-consume semantics, macOS Cmd ->
# KS_META|KS_ALT mapping, keypad-to-digit normalisation, and overflow.
add_executable(test_keybuffer
    test_keybuffer.cpp
    "${CMAKE_SOURCE_DIR}/src/keybuffer.cpp"
)
target_include_directories(test_keybuffer PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/tests"
)
target_compile_definitions(test_keybuffer PRIVATE ZT_TEST_NO_SDL)
target_compile_features(test_keybuffer PRIVATE cxx_std_17)
add_test(NAME keybuffer COMMAND test_keybuffer)

# CCizer parser, sidecar, MIDI byte builder, folder resolution. Pure C/C++
# (no SDL). The test pulls ccizer.cpp in via #include so the same translation
# unit lands in the test binary; no need to add it to the source list.
add_executable(test_ccizer
    test_ccizer.cpp
)
target_include_directories(test_ccizer PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_features(test_ccizer PRIVATE cxx_std_17)
add_test(NAME ccizer COMMAND test_ccizer)

# SysEx receive queue. Pure C++ (std::mutex), no platform MIDI deps.
add_executable(test_sysex_inq
    test_sysex_inq.cpp
)
target_include_directories(test_sysex_inq PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_features(test_sysex_inq PRIVATE cxx_std_17)
add_test(NAME sysex_inq COMMAND test_sysex_inq)
