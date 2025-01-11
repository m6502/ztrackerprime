#ifndef _UserInterfaceInstEd_H
#define _UserInterfaceInstEd_H

#include "UserInterface.h"

class Drawable ;

class InstEditor : public UserInterfaceElement
{
    public:
        Frame *frm;
        int cursor,text_cursor;
        int list_start;

        int last_cur_row ;

        InstEditor();
        ~InstEditor();

        void strc(char *dst, char *src);
        int update();
        void draw(Drawable *S, int active);
        int mouseupdate(int cur_element);
};

#endif
