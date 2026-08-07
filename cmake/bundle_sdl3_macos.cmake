# Make the macOS .app self-contained by bundling the SDL3 dylib it links
# against into Contents/Frameworks, rewriting install names, adding an
# @executable_path rpath, and ad-hoc re-signing. Invoked at POST_BUILD via
# `cmake -P` (see CMakeLists.txt, ZT_MACOS_BUNDLE_SDL3).
#
# Robust to both SDL3 reference styles:
#   * an absolute id  (Homebrew: /opt/homebrew/opt/sdl3/lib/libSDL3.0.dylib)
#   * an @rpath id     (built-from-source SDL3, with ZT_SDL3_HINT supplying the
#                       real on-disk path)
# If SDL3 cannot be resolved it WARNS but does not fail the build, so a
# developer build never breaks over bundling.
#
# Args (passed with -D):
#   ZT_BIN        full path to the executable (…/zt.app/Contents/MacOS/zt)
#   ZT_APP        bundle root (…/zt.app)
#   ZT_SDL3_HINT  optional absolute path to the source libSDL3*.dylib

if(NOT EXISTS "${ZT_BIN}")
  message(WARNING "bundle_sdl3_macos: binary not found: ${ZT_BIN}")
  return()
endif()

# 1. Find the SDL3 dependency reference inside the binary.
execute_process(COMMAND otool -L "${ZT_BIN}" OUTPUT_VARIABLE _otool ERROR_QUIET)
string(REGEX MATCH "[^\n\t ]*libSDL3[^\n\t ]*\\.dylib" _ref "${_otool}")
if(NOT _ref)
  message(STATUS "bundle_sdl3_macos: no dynamic SDL3 dependency (static link?) — nothing to bundle")
  return()
endif()
get_filename_component(_refname "${_ref}" NAME)

# 2. Resolve the real on-disk dylib: absolute reference wins, else the hint.
set(_src "")
if(IS_ABSOLUTE "${_ref}" AND EXISTS "${_ref}")
  set(_src "${_ref}")
elseif(ZT_SDL3_HINT AND EXISTS "${ZT_SDL3_HINT}")
  set(_src "${ZT_SDL3_HINT}")
endif()
if(NOT _src)
  message(WARNING "bundle_sdl3_macos: cannot resolve SDL3 dylib "
                  "(ref='${_ref}', hint='${ZT_SDL3_HINT}') — app is NOT self-contained")
  return()
endif()

# 3. Copy the real file (resolve symlinks) into Contents/Frameworks.
get_filename_component(_real "${_src}" REALPATH)
set(_fw "${ZT_APP}/Contents/Frameworks")
file(MAKE_DIRECTORY "${_fw}")
set(_bundled "${_fw}/${_refname}")
file(REMOVE "${_bundled}")
configure_file("${_real}" "${_bundled}" COPYONLY)
execute_process(COMMAND chmod u+w "${_bundled}")

# 4. Rewrite install names so the binary finds the bundled copy via rpath.
execute_process(COMMAND install_name_tool -id "@rpath/${_refname}" "${_bundled}")
execute_process(COMMAND install_name_tool -change "${_ref}" "@rpath/${_refname}" "${ZT_BIN}")
execute_process(COMMAND install_name_tool -add_rpath "@executable_path/../Frameworks" "${ZT_BIN}"
                ERROR_QUIET)   # harmless if the rpath already exists

# 5. Re-sign (ad-hoc): editing the Mach-O invalidates any prior signature.
execute_process(COMMAND codesign --remove-signature "${_bundled}" ERROR_QUIET)
execute_process(COMMAND codesign --sign - --force "${_bundled}" ERROR_QUIET)
execute_process(COMMAND codesign --sign - --force --deep "${ZT_APP}" ERROR_QUIET)

# 6. Verify the result is self-contained (no non-system absolute dep left).
execute_process(COMMAND otool -L "${ZT_BIN}" OUTPUT_VARIABLE _after ERROR_QUIET)
if(_after MATCHES "/opt/|/usr/local/")
  message(WARNING "bundle_sdl3_macos: a non-system absolute dependency remains — check otool -L")
endif()
message(STATUS "bundle_sdl3_macos: bundled ${_refname} from ${_real}")
