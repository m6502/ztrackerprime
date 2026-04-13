#include "zt.h"

CUI_Help::CUI_Help(void) {

//  Help *oe;

    UI = new UserInterface;

    tb = new TextBox;
    UI->add_element(tb, 0);
    tb->x = 1;
    tb->y = 10;
    tb->xsize = 78 + ((INTERNAL_RESOLUTION_X-640)/8);
    // Bottom-anchor: fill from tb->y down to ~5 rows above screen bottom.
    tb->ysize = (INTERNAL_RESOLUTION_Y/8) - tb->y - 8;
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
#ifdef __APPLE__
        // On macOS every primary modifier (CTRL, ALT, Cmd) triggers the same
        // action thanks to KS_HAS_PRIMARY. Rewrite every shortcut row in the
        // help file to the uniform "CMD-<key>/ALT-<key>" format so users see
        // both macOS-natural modifiers listed identically on every line.
        //
        // Line-by-line, one rewrite per line, only at word boundaries so we
        // don't touch prose references. Compound "CTRL-ALT-X" is copied
        // verbatim (New Song, Quit — those truly need both modifiers
        // held).
        //
        // The replacement is wider than the source (e.g. "CTRL-B" (6 chars)
        // becomes "CMD-B/ALT-B" (11 chars); "CTRL-SHIFT-G" (12 chars) becomes
        // "CMD-SHIFT-G/ALT-SHIFT-G" (23 chars)). We consume trailing spaces
        // to compensate so the colon doesn't drift for short keys; long
        // SHIFT combos naturally run wider and push the colon right.
        {
            int src_len = fs;
            char *src = tb->text;
            char *dst = new char[src_len * 3 + 32];
            int di = 0;
            int i = 0;
            while (i < src_len) {
                int line_end = i;
                while (line_end < src_len && src[line_end] != '\n') line_end++;
                bool rewritten = false;
                while (i <= line_end) {
                    if (i == line_end) { if (i < src_len) dst[di++] = src[i++]; break; }
                    bool boundary = (i == 0)
                        || src[i-1] == ' '  || src[i-1] == '\n'
                        || src[i-1] == '\t' || src[i-1] == '(';
                    // Compound modifier: copy "CTRL-ALT-" verbatim.
                    if (!rewritten && boundary
                        && i + 9 <= line_end
                        && strncmp(src + i, "CTRL-ALT-", 9) == 0) {
                        memcpy(dst + di, src + i, 9); di += 9; i += 9;
                        continue;
                    }
                    int kw_len = 0;
                    if (!rewritten && boundary && i + 5 <= line_end
                        && strncmp(src + i, "CTRL-", 5) == 0) kw_len = 5;
                    else if (!rewritten && boundary && i + 4 <= line_end
                        && strncmp(src + i, "ALT-", 4) == 0)  kw_len = 4;
                    if (kw_len) {
                        // Capture the key name: everything after the modifier
                        // until whitespace, colon, or end of line. Keep the
                        // full suffix so SHIFT combinations (e.g. "SHIFT-G")
                        // get duplicated as "CMD-SHIFT-G/ALT-SHIFT-G".
                        int key_start = i + kw_len;
                        int key_end = key_start;
                        while (key_end < line_end
                               && src[key_end] != ' '
                               && src[key_end] != '\t'
                               && src[key_end] != ':') key_end++;
                        int key_len = key_end - key_start;
                        // Emit "CMD-<key>/ALT-<key>".
                        memcpy(dst + di, "CMD-", 4); di += 4;
                        memcpy(dst + di, src + key_start, key_len); di += key_len;
                        memcpy(dst + di, "/ALT-", 5); di += 5;
                        memcpy(dst + di, src + key_start, key_len); di += key_len;
                        i = key_end;
                        // Width expansion: source "CTRL-<key>" is 5+key_len
                        // chars; output is 4+key_len+5+4+key_len = 13+2*key_len.
                        // Consume trailing spaces up to the source surplus so
                        // the colon stays as aligned as possible.
                        int grown = (13 + 2 * key_len) - (kw_len + key_len);
                        while (grown > 0 && i < line_end && src[i] == ' ') {
                            i++; grown--;
                        }
                        rewritten = true;
                        continue;
                    }
                    dst[di++] = src[i++];
                }
            }
            dst[di] = '\0';
            delete[] tb->text;
            tb->text = dst;
        }
#endif
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
    // Bottom-anchor: fill from tb->y down to ~5 rows above screen bottom.
    tb->ysize = (INTERNAL_RESOLUTION_Y/8) - tb->y - 8;

    if (S->lock()==0) {
        UI->draw(S);
        draw_status(S);
        printtitle(PAGE_TITLE_ROW_Y,"(F1) Help",COLORS.Text,COLORS.Background,S);
        need_refresh = 0; updated=2;
        ztPlayer->num_orders();
        S->unlock();
    }
}