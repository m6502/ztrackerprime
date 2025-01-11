#ifndef ZT_PATTERNDISPLAY_INCLUDED_
#define ZT_PATTERNDISPLAY_INCLUDED_

#include "lc_sdl_wrapper.h"
#include "UserInterface.h"

class Frame;
class Drawable ;

class PatternDisplay : public UserInterfaceElement
{
    public:

        Frame *frame;
        int disp_row;
        int cur_pat_view;
        int starttrack;
        int tracks,tracksize;
        int clear;

        int cur_track;


        PatternDisplay();
        ~PatternDisplay();

        int update();
        void draw(Drawable *S, int active);

        int next_order(void);
        void disp_playing_row(int x,int y, int pattern, int row, Drawable *S, TColor bg);
        void update_frame(void);
        void disp_playing_pattern(Drawable *S);
        char* printNote(char *str, event *r);
        void disp_track_headers(Drawable *S);
};

#endif
