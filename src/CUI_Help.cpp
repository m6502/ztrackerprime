#include "zt.h"

CUI_Help::CUI_Help(void) {

//  Help *oe;

    UI = new UserInterface;

    TextBox *tb;
    tb = new TextBox;
    UI->add_element(tb, 0);
    tb->x = 1;
    tb->y = 14;
    tb->xsize = 78 + ((INTERNAL_RESOLUTION_X-640)/8);
    tb->ysize = 36+ ((INTERNAL_RESOLUTION_Y-480)/8);
    //tb->text = "This is a test of the textbox reader\n\nit is supposed to work";

    FILE *fp;
    int fs=0;
    char *help_file;
    help_file = (char*)malloc(strlen(zt_directory) + strlen("/doc/help.txt") + 1);
    sprintf(help_file,"%s/doc/help.txt",zt_directory);
    if (fp = fopen(help_file,"rt")) {
        while(!feof(fp)) {
            fgetc(fp);
            fs++;
        }
        fseek(fp,0,SEEK_SET);
        tb->text = new char[fs+10];
        memset(tb->text,0,fs+10);
        fread(tb->text, fs, 1, fp);
        fclose(fp);
        needfree = 1;
    } else {
        needfree = 0;
        tb->text = "\n\n  i couldnt find doc/help.txt.  no help for you :[";
    }
    free(help_file);
}

CUI_Help::~CUI_Help(void) {
    TextBox *tb;
    if (needfree) {
        tb = (TextBox *)UI->get_element(0);
        delete[] tb->text;
    }
}

void CUI_Help::enter(void) {
    need_refresh++;
    cur_state = STATE_HELP;
}

void CUI_Help::leave(void) {

}

void CUI_Help::update() {
    int key=0;
    UI->update();
    if (Keys.size()) {
        key = Keys.getkey();
    }
}

void CUI_Help::draw(Drawable *S) {
    if (S->lock()==0) {
        UI->draw(S);
        draw_status(S);
        printtitle(11,"Help",COLORS.Text,COLORS.Background,S);
        need_refresh = 0; updated=2;
        ztPlayer->num_orders();
        S->unlock();
    }
}