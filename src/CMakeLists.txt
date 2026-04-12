# ztracker main executable.

set(ZT_SOURCES
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

# WIN32 subsystem — SDLmain.a provides WinMain which calls our SDL_main (aliased from main).
# Without this, the MinGW CRT expects WinMain@16 and can't find it.
add_executable(zt WIN32 ${ZT_SOURCES} ${ZT_RESOURCE_FILES})

target_include_directories(zt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${SDL12_INCLUDE_DIR}
)

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
    target_link_libraries(zt PRIVATE
        ${SDL12_LIBRARIES}
        png_static
        zlibstatic
    )
endif()

# Win32 system libraries needed for MIDI + multimedia timers + COM basics.
if(WIN32)
    target_link_libraries(zt PRIVATE winmm ole32 oleaut32 uuid comdlg32 gdi32 user32 shell32)
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
