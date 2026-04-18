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
        int update() override;
        void draw(Drawable *S, int active) override;
        int mouseupdate(int cur_element) override;
        bool is_text_input() const override { return text_cursor < 24; }
        void on_focus() override { text_cursor = (g_ui_tab_dir < 0) ? 24 : 0; }
};

#endif
