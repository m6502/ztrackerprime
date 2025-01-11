#ifndef _UserInterfaceOrdEd_H
#define _UserInterfaceOrdEd_H

#include "UserInterface.h"

class Drawable ;


class OrderEditor : public UserInterfaceElement
{
    public:
        Frame *frm;
        int cursor_y,cursor_x;
        int list_start;
        int old_playing_ord;

        OrderEditor();
        ~OrderEditor();
        int mouseupdate(int cur_element);
        int update();
        void draw(Drawable *S, int active);
};

#endif
