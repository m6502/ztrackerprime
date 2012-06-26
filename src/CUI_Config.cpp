#include "zt.h"

void BTNCLK_SaveSettings(UserInterfaceElement *b) {

    //Config.save("zt.conf");
    zt_config_globals.save();
    need_refresh++; 
}

CUI_Config::CUI_Config(void) {
    UI = new UserInterface;
//    CheckBox *cb_full_screen;
//    CheckBox *cb_fade_in_out;
//    CheckBox *cb_Auto_open_MIDI;
//    TextInput *ti_skin_file;
//    TextInput *ti_color_file;
//    TextInput *ti_work_directory;
//    ValueSlider *vs_key_repeat;
//    ValueSlider *vs_key_wait;

    CheckBox *cb;
//    TextInput *ti;
    ValueSlider *vs;
//    Button *b;
    //TextBox *tb;
    SkinSelector *sk;

    cb = new CheckBox;
    UI->add_element(cb,0);
    cb->frame = 0;
    cb->x = 17;
    cb->y = 14;
    cb->xsize = 5;
    cb->value = &zt_config_globals.full_screen;
    cb->frame = 1;

    vs = new ValueSlider;
    UI->add_element(vs,1);
    vs->x = 17;
    vs->y = 16;
    vs->xsize = 15;
    vs->ysize = 1;
    vs->value = zt_config_globals.key_repeat_time;
    vs->min = 1;
    vs->max = 32;

    vs = new ValueSlider;
    UI->add_element(vs,2);
    vs->x = 17;
    vs->y = 18;
    vs->xsize = 15;
    vs->ysize = 1;
    vs->value = zt_config_globals.key_wait_time;
    vs->min = 1;
    vs->max = 1000;

    sk = new SkinSelector;
    UI->add_element(sk,3);
    sk->x = 17;
    sk->y = 20;
    sk->xsize = 19;
    sk->ysize = 8;

    /*
    cb = new CheckBox;
    UI->add_element(cb,1);
    cb->frame = 0;
    cb->x = 17;
    cb->y = 16;
    cb->xsize = 5;
    cb->value = &zt_config_globals.do_fade;
*/

/*

    ti = new TextInput;
    UI->add_element(ti,3);
    ti->frame = 1;
    ti->x = 17; 
    ti->y = 20;
    ti->xsize=50;
    ti->length=50;
    ti->str = (unsigned char*)zt_config_globals.skin;
    ti = new TextInput;
    UI->add_element(ti,4);
    ti->frame = 1;
    ti->x = 17;
    ti->y = 22;
    ti->xsize=50;
    ti->length=50;
    ti->str = (unsigned char*)COLORFILE;
*/
/*
    ti = new TextInput;
    UI->add_element(ti,6);
    ti->frame = 1;
    ti->x = 17; 
    ti->y = 26;
    ti->xsize=50;
    ti->length=50;
    ti->str = (unsigned char*)zt_config_globals.work_directory;
    b = new Button;
    UI->add_element(b,7);
    b->caption = " Save instance";
    b->x = 17;
    b->y = 29;
    b->xsize = 15;
    b->ysize = 1;
    b->OnClick = (ActFunc)BTNCLK_SaveSettings; 
*/

    tb = new TextBox;
    UI->add_element(tb, 4);
    tb->x = 1;
    tb->y = 34;
    tb->xsize = 78;
    tb->ysize = 16;
    tb->text=NULL;


}

CUI_Config::~CUI_Config(void) {
    if (UI) delete UI; UI = NULL;
}

void CUI_Config::enter(void) {
    need_refresh = 1;
    cur_state = STATE_SONG_CONFIG;
    Keys.flush();
}

void CUI_Config::leave(void) {

}

void CUI_Config::update() {
    int key=0;

    UI->update();
    if (Keys.size()) {
        key = Keys.getkey();    
    }
}

void CUI_Config::draw(Drawable *S) {
    char buf[1024];
    if (S->lock()==0) {
        sprintf(buf,"\n|U|Current Settings in memory:\n");
        if(tb->text != NULL)
        {
            free(tb->text);
            tb->text = NULL;
        }
        sprintf(buf+strlen(buf),"\n|U|    Full Screen |L|[|H|%s|L|]",zt_config_globals.full_screen?"On":"Off");
//        sprintf(buf+strlen(buf),"\n|U|    Fade In/Out |L|[|H|%s|L|]",zt_config_globals.do_fade?"On":"Off");
        sprintf(buf+strlen(buf),"\n|U| Auto-Open MIDI |L|[|H|%s|L|]",zt_config_globals.auto_open_midi?"On":"Off");

        tb->text = _strdup(buf);
        UI->draw(S);
        draw_status(S);
        status(S);
        printtitle(11,"Global Configuration",COLORS.Text,COLORS.Background,S);
        print(row(2),col(14),"   Full Screen",COLORS.Text,S);
        print(row(2),col(16),"    Key Repeat",COLORS.Text,S);
        print(row(2),col(18),"      Key Wait",COLORS.Text,S);
        print(row(2),col(20),"          Skin",COLORS.Text,S);
//        print(row(2),col(26)," .ZT directory",COLORS.Text,S);

        printtitle(32,"Current Global Settings",COLORS.Text,COLORS.Background,S);

        need_refresh = 0; updated=2;
        S->unlock();
    }
}

