#ifndef ZT_SLIDERS_INCLUDED___
#define ZT_SLIDERS_INCLUDED___

#include "zt.h"
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
        ~ValueSlider();
        int mouseupdate(int cur_element);
        virtual int update();
        void draw(Drawable *S, int active);
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
        ~ValueSliderDL();
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
        ~ValueSliderOFF();
        int mouseupdate(int cur_element);
        int update();
        void draw(Drawable *S, int active);
};









#endif
