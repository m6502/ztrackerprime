#ifndef ZT_BUTTON_INCLUDED____
#define ZT_BUTTON_INCLUDED____

#include "zt.h"

class Button : public UserInterfaceElement {
    public:
        char *caption;
        int state;
        int updown;
        ActFunc OnClick;

        Button();
        ~Button();

        int update();
        void draw(Drawable *S, int active);
        void setOnClick(ActFunc func);
        int mouseupdate(int cur_element);

};


#endif
