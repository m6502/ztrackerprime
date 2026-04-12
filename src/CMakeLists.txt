# ztracker main executable.

set(ZT_SOURCES
    third_party/rtmidi/RtMidi.cpp
    Button.cpp
    CDataBuf.cpp
    conf.cpp
    CUI_About.cpp
    CUI_Arpeggioeditor.cpp
    CUI_Config.cpp
    CUI_Help.cpp
    CUI_InstEditor.cpp
    CUI_LoadMsg.cpp
    CUI_Loadscreen.cpp
    CUI_Logoscreen.cpp
    CUI_Midimacroeditor.cpp
    CUI_NewSong.cpp
    CUI_Ordereditor.cpp
    CUI_Page.cpp
    CUI_Patterneditor.cpp
    CUI_PENature.cpp
    CUI_PEParms.cpp
    CUI_PEVol.cpp
    CUI_Playsong.cpp
    CUI_RUSure.cpp
    CUI_SaveMsg.cpp
    CUI_Savescreen.cpp
    CUI_SliderInput.cpp
    CUI_Songconfig.cpp
    CUI_SongDuration.cpp
    CUI_SongMessage.cpp
    CUI_Sysconfig.cpp
    edit_cols.cpp
    FileList.cpp
    font.cpp
    img.cpp
    import_export.cpp
    InstEditor.cpp
    it.cpp
    keybuffer.cpp
    lc_sdl_wrapper.cpp
    list.cpp
    main.cpp
    midi-io.cpp
    module.cpp
    OrderEditor.cpp
    OutputDevices.cpp
    PatternDisplay.cpp
    playback.cpp
    skins.cpp
    Sliders.cpp
    UserInterface.cpp
    zlib_wrapper.cpp
    ztfile-io.cpp
)

# Resource file (icon) — compiled on Windows/MinGW only.
set(ZT_RESOURCE_FILES)
if(WIN32)
    set(ZT_RC_FILE "${CMAKE_SOURCE_DIR}/zt.rc")
    if(EXISTS "${ZT_RC_FILE}")
        list(APPEND ZT_RESOURCE_FILES "${ZT_RC_FILE}")
    endif()
endif()

# WIN32 subsystem on Windows — SDLmain.a provides WinMain which calls our SDL_main.
# On macOS, produce a proper .app bundle with icon.
# On Linux, just a normal executable.
if(WIN32)
    add_executable(zt WIN32 ${ZT_SOURCES} ${ZT_RESOURCE_FILES})
elseif(APPLE)
    set(ZT_ICNS "${CMAKE_SOURCE_DIR}/zt.icns")
    set_source_files_properties(${ZT_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    add_executable(zt MACOSX_BUNDLE ${ZT_SOURCES} ${ZT_ICNS})
    set_target_properties(zt PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/macos/Info.plist.in"
        MACOSX_BUNDLE_ICON_FILE "zt.icns"
        MACOSX_BUNDLE_BUNDLE_NAME "zTracker"
        MACOSX_BUNDLE_GUI_IDENTIFIER "net.sourceforge.ztracker"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "2026.04.12"
    )
else()
    add_executable(zt ${ZT_SOURCES})
endif()

target_include_directories(zt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${SDL12_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rtmidi
)

# RtMidi backend selection — tells RtMidi which platform MIDI API to compile.
if(WIN32)
    target_compile_definitions(zt PRIVATE __WINDOWS_MM__)
elseif(APPLE)
    target_compile_definitions(zt PRIVATE __MACOSX_CORE__)
else()
    target_compile_definitions(zt PRIVATE __LINUX_ALSA__)
endif()

# Win32 resource compile works automatically when the .rc is listed in target sources
# and CMake has the RC compiler from the toolchain file.

# SDLmain: The pre-built libSDLmain.a from the SDL 1.2.15 dev pack uses old MSVC stdio
# symbols (_imp___iob) that MinGW-w64 GCC 15 doesn't provide. We compile our own
# minimal WinMain shim that calls SDL_main().
if(CMAKE_CROSSCOMPILING AND MINGW)
    target_sources(zt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/platform/sdl_win32_main.c")
    target_link_libraries(zt PRIVATE
        "${SDL12_LIB_DIR}/libSDL.dll.a"
        png_static
        zlibstatic
    )
else()
    target_link_directories(zt PRIVATE ${SDL12_LIBRARY_DIRS})
    target_link_libraries(zt PRIVATE
        ${SDL12_LIBRARIES}
        png_static
        zlibstatic
    )
endif()

# Platform-specific system libraries.
if(WIN32)
    # MIDI + multimedia timers + COM basics.
    target_link_libraries(zt PRIVATE winmm ole32 oleaut32 uuid comdlg32 gdi32 user32 shell32)
elseif(APPLE)
    # RtMidi needs CoreMIDI, CoreAudio, CoreFoundation on macOS.
    find_library(COREMIDI_LIBRARY CoreMIDI)
    find_library(COREAUDIO_LIBRARY CoreAudio)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    find_library(COCOA_LIBRARY Cocoa)
    target_link_libraries(zt PRIVATE ${COREMIDI_LIBRARY} ${COREAUDIO_LIBRARY} ${COREFOUNDATION_LIBRARY} ${COCOA_LIBRARY})
else()
    # Linux: RtMidi needs ALSA.
    find_package(PkgConfig)
    pkg_check_modules(ALSA alsa)
    if(ALSA_FOUND)
        target_link_libraries(zt PRIVATE ${ALSA_LIBRARIES})
    endif()
endif()

# Copy runtime data (zt.conf, skins, doc) next to the binary so it works out of the box.
# On macOS .app bundle, put them inside Contents/Resources/.
if(APPLE)
    set(ZT_RESOURCE_DIR "$<TARGET_FILE_DIR:zt>/../Resources")
    add_custom_command(TARGET zt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/zt.conf" "${ZT_RESOURCE_DIR}/zt.conf"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/skins" "${ZT_RESOURCE_DIR}/skins"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/doc" "${ZT_RESOURCE_DIR}/doc"
    )
else()
    add_custom_command(TARGET zt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/zt.conf" "$<TARGET_FILE_DIR:zt>/zt.conf"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/skins" "$<TARGET_FILE_DIR:zt>/skins"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/doc" "$<TARGET_FILE_DIR:zt>/doc"
    )
endif()

# Copy SDL.dll next to the exe (Phase 1 smoke-test artifact).
if(CMAKE_CROSSCOMPILING AND MINGW)
    if(EXISTS "${SDL12_RUNTIME_DLL}")
        add_custom_command(TARGET zt POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SDL12_RUNTIME_DLL}" "$<TARGET_FILE_DIR:zt>/SDL.dll"
        )
    endif()
endif()
