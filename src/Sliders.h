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
        int from_input;  // set when value was just set via numeric SliderInput popup;
                         // callers can use this to disambiguate typed-value vs arrow-step
                         // for sliders backed by an index table (e.g. TPB).
        int input_value; // raw typed number from the popup, preserved BEFORE clamping
                         // so callers can see the user's literal input even if it falls
                         // outside [min,max]. Only valid when from_input == 1.
        // Double-click-to-reset. If dblclick_default is in [min,max], a second
        // left-click within DBLCLICK_MS on the same widget snaps value to
        // dblclick_default and bumps `changed`. Sentinel -1 disables. Used by
        // CC Console to center pitchbend (8192) / zero CCs on a quick re-click.
        int dblclick_default;
        unsigned int last_click_ms;
        explicit ValueSlider(int fset = 0);
        virtual ~ValueSlider() = default ;
        int mouseupdate(int cur_element);
        virtual int update();
        virtual void draw(Drawable *S, int active);
};
 
class ValueSliderDL : public ValueSlider {
    public:
        explicit ValueSliderDL(int fset = 0);
        virtual ~ValueSliderDL() = default ;
        int update() override;
        virtual void draw(Drawable *S, int active) override;
};

class ValueSliderOFF : public ValueSliderDL {
    public:
        explicit ValueSliderOFF(int fset);
        void draw(Drawable *S, int active) override;
};

#endif
