#ifndef __CUI_FILELISTS_COMMON_
#define __CUI_FILELISTS_COMMON_


// Base position (In characters)

#define FILE_LISTS_BASE_X_CHARS            1
#define FILE_LISTS_BASE_Y_CHARS            13


// Computed max list sizes (In characters)

// Match the 55-pixel toolbar drawn by main.cpp; list frames must stay above it.
#define LOW_PANEL_SIZE_Y_PIXELS             55

#define LISTS_MAX_NUMBER_OF_ELEMENTS ((((INTERNAL_RESOLUTION_Y -  LOW_PANEL_SIZE_Y_PIXELS) / FONT_SIZE_Y ) - FILE_LISTS_BASE_Y_CHARS ) - 2)


// Interspace configuration (In characters)

#define FILE_LISTS_SEPARATION_X_CHARS      2
#define FILE_LISTS_SEPARATION_Y_CHARS      2


// File list configuration (In characters)

// Places on the left, with the remaining width used by the combined browser.
#define DRIVE_LIST_POS_X                  FILE_LISTS_BASE_X_CHARS
#define DRIVE_LIST_POS_Y                  FILE_LISTS_BASE_Y_CHARS
#define DRIVE_LIST_SIZE_X                 20
#define DRIVE_LIST_SIZE_Y                 LISTS_MAX_NUMBER_OF_ELEMENTS

#define FILE_LIST_POS_X_CHARS             (DRIVE_LIST_POS_X + DRIVE_LIST_SIZE_X + FILE_LISTS_SEPARATION_X_CHARS)
#define FILE_LIST_POS_Y_CHARS             FILE_LISTS_BASE_Y_CHARS
#define FILE_LIST_SIZE_X_CHARS            (INTERNAL_RESOLUTION_X / FONT_SIZE_X - FILE_LIST_POS_X_CHARS - 2)
#define LOAD_FILE_LIST_SIZE_Y_CHARS        LISTS_MAX_NUMBER_OF_ELEMENTS
#define SAVE_FILE_LIST_SIZE_Y_CHARS        (LISTS_MAX_NUMBER_OF_ELEMENTS - 10)

// Buttons

#define SAVE_BUTTONS_BASE_Y          (FILE_LISTS_BASE_Y_CHARS + SAVE_FILE_LIST_SIZE_Y_CHARS + FILE_LISTS_SEPARATION_Y_CHARS)


#define SAVE_TEXTINPUT_SIZE_X        FILE_LIST_SIZE_X_CHARS

#define SAVE_TEXTINPUT_POS_X         FILE_LIST_POS_X_CHARS
#define SAVE_TEXTINPUT_POS_Y         (SAVE_BUTTONS_BASE_Y + 0)


// Tighten button stack so it doesn't extend below the right-hand file
// list. TextInput->first button gap = 1 row, between buttons = 1 row.
#define SAVE_ZT_BUTTON_POS_X         FILE_LIST_POS_X_CHARS
#define SAVE_ZT_BUTTON_POS_Y         (SAVE_TEXTINPUT_POS_Y + 1)


#define SAVE_MID_BUTTON_POS_X        FILE_LIST_POS_X_CHARS
#define SAVE_MID_BUTTON_POS_Y        (SAVE_ZT_BUTTON_POS_Y + 1)

#define SAVE_MID_MT_BUTTON_POS_X     FILE_LIST_POS_X_CHARS
#define SAVE_MID_MT_BUTTON_POS_Y     (SAVE_MID_BUTTON_POS_Y + 1)

#define SAVE_MID_PT_BUTTON_POS_X     FILE_LIST_POS_X_CHARS
#define SAVE_MID_PT_BUTTON_POS_Y     (SAVE_MID_MT_BUTTON_POS_Y + 1)

//#define SAVE_GBA_BUTTON_POS_X        FILE_LIST_POS_X_CHARS
//#define SAVE_GBA_BUTTON_POS_Y        (SAVE_ZT_BUTTON_POS_Y + 1 + 1 + 1 + 1)

// Images

#define LOADORSAVE_IMAGE_X           (INTERNAL_RESOLUTION_X - CurrentSkin->bmLoad->surface->w - 16)                         // 275
#define LOADORSAVE_IMAGE_Y           (INTERNAL_RESOLUTION_Y - CurrentSkin->bmLoad->surface->h - LOW_PANEL_SIZE_Y_PIXELS - 16) // 267





inline void draw_file_browser_header(Drawable *S) {
    screenmanager.Update(0, row(FILE_LISTS_BASE_Y_CHARS - 2), INTERNAL_RESOLUTION_X, row(FILE_LISTS_BASE_Y_CHARS - 1));
    printBG(col(DRIVE_LIST_POS_X), row(FILE_LISTS_BASE_Y_CHARS - 2),
          "Places", COLORS.Text, COLORS.Background, S);
    std::error_code error;
    auto path = std::filesystem::current_path(error).string();
    if (error) path = "Folder unavailable";
    const size_t width = FILE_LIST_SIZE_X_CHARS;
    if (path.size() > width && width > 3) path = "..." + path.substr(path.size() - width + 3);
    path.resize(width, ' ');
    printBG(col(FILE_LIST_POS_X_CHARS), row(FILE_LISTS_BASE_Y_CHARS - 2),
          path.c_str(), COLORS.Text, COLORS.Background, S);
}

#endif
