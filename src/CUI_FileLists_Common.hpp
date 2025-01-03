#ifndef __CUI_FILELISTS_COMMON_
#define __CUI_FILELISTS_COMMON_


// Base position (In characters)

#define FILE_LISTS_BASE_X_CHARS            2
#define FILE_LISTS_BASE_Y_CHARS            13


// Computed max list sizes (In characters)

#define LOW_PANEL_SIZE_Y_PIXELS             43

#define LISTS_MAX_NUMBER_OF_ELEMENTS ((((INTERNAL_RESOLUTION_Y -  LOW_PANEL_SIZE_Y_PIXELS) / FONT_SIZE_Y ) - FILE_LISTS_BASE_Y_CHARS ) - 2)


// Interspace configuration (In characters)

#define FILE_LISTS_SEPARATION_X_CHARS      2
#define FILE_LISTS_SEPARATION_Y_CHARS      2


// File list configuration (In characters)

#define FILE_LIST_POS_X_CHARS              (FILE_LISTS_BASE_X_CHARS + 0)
#define FILE_LIST_POS_Y_CHARS              13

#define FILE_LIST_SIZE_X_CHARS             40
#define LOAD_FILE_LIST_SIZE_Y_CHARS        (LISTS_MAX_NUMBER_OF_ELEMENTS)
#define SAVE_FILE_LIST_SIZE_Y_CHARS        (LISTS_MAX_NUMBER_OF_ELEMENTS - 6) // We need extra space for the text input and the two buttons


// Directory list configuration (In characters)

#define DIRECTORY_LIST_POS_X         (FILE_LIST_POS_X_CHARS + FILE_LIST_SIZE_X_CHARS + FILE_LISTS_SEPARATION_X_CHARS)
#define DIRECTORY_LIST_POS_Y         13

#define DIRECTORY_LIST_SIZE_X_CHARS        30
#define DIRECTORY_LIST_SIZE_Y_CHARS        LISTS_MAX_NUMBER_OF_ELEMENTS


// Drive list configuration (In characters)

#define DRIVE_LIST_POS_X             (DIRECTORY_LIST_POS_X + DIRECTORY_LIST_SIZE_X_CHARS + FILE_LISTS_SEPARATION_X_CHARS)
#define DRIVE_LIST_POS_Y             13

#define DRIVE_LIST_SIZE_X            4
#define DRIVE_LIST_SIZE_Y            LISTS_MAX_NUMBER_OF_ELEMENTS


// Buttons

#define SAVE_BUTTONS_BASE_Y          (FILE_LISTS_BASE_Y_CHARS + SAVE_FILE_LIST_SIZE_Y_CHARS + FILE_LISTS_SEPARATION_Y_CHARS)


#define SAVE_TEXTINPUT_SIZE_X        FILE_LIST_SIZE_X_CHARS

#define SAVE_TEXTINPUT_POS_X         FILE_LIST_POS_X_CHARS
#define SAVE_TEXTINPUT_POS_Y         (SAVE_BUTTONS_BASE_Y + 0)


#define SAVE_ZT_BUTTON_POS_X         FILE_LIST_POS_X_CHARS
#define SAVE_ZT_BUTTON_POS_Y         (SAVE_TEXTINPUT_POS_Y + 1 + 1)


#define SAVE_MID_BUTTON_POS_X        FILE_LIST_POS_X_CHARS
#define SAVE_MID_BUTTON_POS_Y        (SAVE_ZT_BUTTON_POS_Y + 1 + 1)

//#define SAVE_GBA_BUTTON_POS_X        FILE_LIST_POS_X_CHARS
//#define SAVE_GBA_BUTTON_POS_Y        (SAVE_ZT_BUTTON_POS_Y + 1 + 1 + 1 + 1)

// Images

#define LOADORSAVE_IMAGE_X           (INTERNAL_RESOLUTION_X - CurrentSkin->bmLoad->surface->w - 16)                         // 275
#define LOADORSAVE_IMAGE_Y           (INTERNAL_RESOLUTION_Y - CurrentSkin->bmLoad->surface->h - LOW_PANEL_SIZE_Y_PIXELS - 16) // 267





#endif
