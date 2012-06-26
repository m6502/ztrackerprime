#include "zt.h"

CUI_Page::CUI_Page() {
    UI = NULL;
    need_refresh = 0;
}

CUI_Page::~CUI_Page() {
    if (UI)
        delete UI;
}