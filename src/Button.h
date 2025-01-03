#ifndef ZT_BUTTON_INCLUDED____
#define ZT_BUTTON_INCLUDED____

#include "zt.h"

class Button : public UserInterfaceElement {
    public:
        const char *caption;
        int state;
        int updown;
        ActFunc OnClick;

        Button();
        ~Button();

        virtual void auto_anchor_at_current_pos(int how) ;
        virtual void auto_update_anchor() ;

        int update();
        void draw(Drawable *S, int active);
        void setOnClick(ActFunc func);
        int mouseupdate(int cur_element);

};


#endif
