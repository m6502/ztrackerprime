#ifndef __CUI_FILELISTS_COMMON_
#define __CUI_FILELISTS_COMMON_


// Base position (In characters)

#define FILE_LISTS_BASE_X            2
#define FILE_LISTS_BASE_Y            13


// Computed max list sizes (In characters)

#define FONT_SIZE_Y                  8
#define LOW_PANEL_SIZE_Y             43

#define LISTS_MAX_NUMBER_OF_ELEMENTS ((((RESOLUTION_Y -  LOW_PANEL_SIZE_Y) / FONT_SIZE_Y ) - FILE_LISTS_BASE_Y ) - 2)


// Interspace configuration (In characters)

#define FILE_LISTS_SEPARATION_X      2
#define FILE_LISTS_SEPARATION_Y      2


// File list configuration (In characters)

#define FILE_LIST_POS_X              (FILE_LISTS_BASE_X + 0)
#define FILE_LIST_POS_Y              13

#define FILE_LIST_SIZE_X             40
#define LOAD_FILE_LIST_SIZE_Y        (LISTS_MAX_NUMBER_OF_ELEMENTS)
#define SAVE_FILE_LIST_SIZE_Y        (LISTS_MAX_NUMBER_OF_ELEMENTS - 6) // We need extra space for the text input and the two buttons


// Directory list configuration (In characters)

#define DIRECTORY_LIST_POS_X         (FILE_LIST_POS_X + FILE_LIST_SIZE_X + FILE_LISTS_SEPARATION_X)
#define DIRECTORY_LIST_POS_Y         13

#define DIRECTORY_LIST_SIZE_X        30
#define DIRECTORY_LIST_SIZE_Y        LISTS_MAX_NUMBER_OF_ELEMENTS


// Drive list configuration (In characters)

#define DRIVE_LIST_POS_X             (DIRECTORY_LIST_POS_X + DIRECTORY_LIST_SIZE_X + FILE_LISTS_SEPARATION_X)
#define DRIVE_LIST_POS_Y             13

#define DRIVE_LIST_SIZE_X            4
#define DRIVE_LIST_SIZE_Y            LISTS_MAX_NUMBER_OF_ELEMENTS


// Buttons

#define SAVE_BUTTONS_BASE_Y          (FILE_LISTS_BASE_Y + SAVE_FILE_LIST_SIZE_Y + FILE_LISTS_SEPARATION_Y)


#define SAVE_TEXTINPUT_SIZE_X        FILE_LIST_SIZE_X

#define SAVE_TEXTINPUT_POS_X         FILE_LIST_POS_X
#define SAVE_TEXTINPUT_POS_Y         (SAVE_BUTTONS_BASE_Y + 0)


#define SAVE_ZT_BUTTON_POS_X         FILE_LIST_POS_X
#define SAVE_ZT_BUTTON_POS_Y         (SAVE_TEXTINPUT_POS_Y + 1 + 1)


#define SAVE_MID_BUTTON_POS_X        FILE_LIST_POS_X
#define SAVE_MID_BUTTON_POS_Y        (SAVE_ZT_BUTTON_POS_Y + 1 + 1)


// Images

#define LOADORSAVE_IMAGE_X           (RESOLUTION_X - CurrentSkin->bmLoad->surface->w - 16)                         // 275
#define LOADORSAVE_IMAGE_Y           (RESOLUTION_Y - CurrentSkin->bmLoad->surface->h - LOW_PANEL_SIZE_Y - 16) // 267





#endif
