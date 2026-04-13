#include "zt.h"

CUI_Help::CUI_Help(void) {

//  Help *oe;

    UI = new UserInterface;

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
    help_file = (char*)malloc(1024);
    // Try multiple paths: zt_directory/doc, ./doc, then plain ./help.txt.
    fp = NULL;
    if (zt_directory[0]) {
        sprintf(help_file,"%s/doc/help.txt",zt_directory);
        fp = fopen(help_file,"r");
    }
    if (!fp) {
        sprintf(help_file,"doc/help.txt");
        fp = fopen(help_file,"r");
    }
    if (!fp) {
        sprintf(help_file,"help.txt");
        fp = fopen(help_file,"r");
    }
    if (fp) {
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
        tb->text = "\n\n  I couldn't find doc/help.txt.  no help for you :[";
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
        if (key == SDLK_F1 || key == SDLK_ESCAPE) {
            switch_page(UIP_Patterneditor);
        }
    }
}

void CUI_Help::draw(Drawable *S) {

    tb->xsize = 78 + ((INTERNAL_RESOLUTION_X-640)/8);
    tb->ysize = 36+ ((INTERNAL_RESOLUTION_Y-480)/8);

    if (S->lock()==0) {
        UI->draw(S);
        draw_status(S);
        printtitle(PAGE_TITLE_ROW_Y,"Help (F1)",COLORS.Text,COLORS.Background,S);
        need_refresh = 0; updated=2;
        ztPlayer->num_orders();
        S->unlock();
    }
}