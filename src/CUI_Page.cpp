#include "zt.h"

CUI_Page::CUI_Page()
    : UI(nullptr) {
    need_refresh = 0;
}

CUI_Page::~CUI_Page() {
    delete UI;
}
