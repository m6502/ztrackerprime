#ifndef ZT_SLIDERS_INCLUDED___
#define ZT_SLIDERS_INCLUDED___

#include "UserInterface.h"

class ValueSlider : public UserInterfaceElement {
    public:
        int min;
        int max;
        int value;
        int force_f;
        int force_v;
        int changed;
        int clear;
        int newclick;
        int focus;  // new value here
        ValueSlider();
        ValueSlider(int fset); // new constructor
        ~ValueSlider() = default ;
        int mouseupdate(int cur_element);
        virtual int update();
        void draw(Drawable *S, int active);
        // A zero-sized ValueSlider (xsize==0 or ysize==0) draws nothing and
        // its update() early-returns 0 for every key, so landing focus on it
        // produces a visibly-unfocused "dead" state the user can't escape
        // with arrow keys. Declare it non-focusable so advance_focus skips
        // past it entirely. See doc/design/ui-focus-nav.md.
        bool is_tab_stop() const override { return xsize > 0 && ysize > 0; }
};
 

class ValueSliderDL : public UserInterfaceElement {
    public:
        int min;
        int max;
        int value;
        int changed;
        int newclick;
        int focus;  // new value here
        ValueSliderDL();
        ValueSliderDL(int fset); // new constructor
        ~ValueSliderDL() = default ;
        int mouseupdate(int cur_element);
        int update();
        void draw(Drawable *S, int active);
};
class ValueSliderOFF : public UserInterfaceElement {
    public:
        int min;
        int max;
        signed int value;
        int changed;
        int newclick;
        int focus;  // new value here
        ValueSliderOFF();
        ValueSliderOFF(int fset); // new constructor
        ~ValueSliderOFF() = default ;
        int mouseupdate(int cur_element);
        int update();
        void draw(Drawable *S, int active);
};









#endif
