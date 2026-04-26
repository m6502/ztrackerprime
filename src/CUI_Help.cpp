#include "zt.h"

CUI_Help::CUI_Help(void) {

//  Help *oe;

    section_lines = NULL;
    section_count = 0;

    UI = new UserInterface;

    tb = new TextBox;
    UI->add_element(tb, 0);
    tb->x = 1;
    tb->y = 12;
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
        char *text = new char[fs + 10];
        memset(text,0,fs+10);
        fread(text, fs, 1, fp);
        fclose(fp);
        tb->text = text;
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
            const char *src = tb->text;
            char *dst = new char[src_len * 3 + 32];
            int di = 0;
            int i = 0;
            // Carries over from the previous shortcut line. If a rewrite
            // pushed the colon `pending_pad` columns to the right (e.g.
            // when there weren't enough trailing spaces in the source to
            // absorb a long key name like F10/F11/F12), the next
            // continuation line(s) (lines starting with leading whitespace
            // and no early colon) get the same number of extra leading
            // spaces so the wrapped description text re-aligns under the
            // rewritten line's description column.
            int pending_pad = 0;
            while (i < src_len) {
                int line_end = i;
                while (line_end < src_len && src[line_end] != '\n') line_end++;
                bool rewritten = false;
                int line_growth = 0;

                bool is_blank_line = (i >= line_end);
                bool starts_with_space = (!is_blank_line && src[i] == ' ');
                bool has_early_colon = false;
                {
                    int scan_end = line_end;
                    if (scan_end - i > 24) scan_end = i + 24;
                    for (int k = i; k < scan_end; k++) {
                        if (src[k] == ':') { has_early_colon = true; break; }
                    }
                }
                bool is_continuation = !is_blank_line && starts_with_space && !has_early_colon;

                if (is_blank_line) pending_pad = 0;
                if (pending_pad > 0 && is_continuation) {
                    for (int p = 0; p < pending_pad && di < src_len * 3 + 30; p++) {
                        dst[di++] = ' ';
                    }
                }

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
                        // Don't rewrite chords whose Cmd-form is OS-reserved
                        // on macOS (Cmd-TAB = app switcher, Cmd-Q = quit
                        // — handled separately, Cmd-W = close window,
                        // Cmd-H = hide, Cmd-` = window-cycle).
                        // Copy the modifier verbatim so users see e.g.
                        // "CTRL-TAB" not "CMD-TAB/ALT-TAB".
                        auto is_reserved = [](const char *p, int n) {
                            if (n == 3 && strncmp(p, "TAB", 3) == 0) return true;
                            if (n == 1 && (*p == 'W' || *p == 'H')) return true;
                            return false;
                        };
                        // Compound combos like "SHIFT-X" expand to
                        // "CMD-SHIFT-X/ALT-SHIFT-X" which doubles the
                        // length and pushes the colon way past the
                        // source's column. That destroys row-to-row
                        // colon alignment and looks bad. Keep the
                        // source modifier verbatim for these.
                        bool key_starts_with_shift =
                            key_len >= 6 && strncmp(src + key_start, "SHIFT-", 6) == 0;
                        if (is_reserved(src + key_start, key_len)
                            || key_starts_with_shift) {
                            memcpy(dst + di, src + i, kw_len + key_len);
                            di += kw_len + key_len;
                            i = key_end;
                            rewritten = true;
                            continue;
                        }
                        // Emit "CMD-<key>/ALT-<key>".
                        memcpy(dst + di, "CMD-", 4); di += 4;
                        memcpy(dst + di, src + key_start, key_len); di += key_len;
                        memcpy(dst + di, "/ALT-", 5); di += 5;
                        memcpy(dst + di, src + key_start, key_len); di += key_len;
                        i = key_end;
                        // Width expansion: source "CTRL-<key>" is kw_len +
                        // key_len chars; output is "CMD-" (4) + key_len +
                        // "/ALT-" (5) + key_len = 9 + 2*key_len. Consume
                        // exactly that many trailing spaces from the source
                        // so the colon ends up at the same column it had
                        // pre-rewrite. (Previous formula used 13 instead of
                        // 9, eating four spaces too many — rewritten lines
                        // ended up with colons four columns left of
                        // unrewritten neighbours.)
                        int grown = (9 + 2 * key_len) - (kw_len + key_len);
                        int absorbed = 0;
                        while (absorbed < grown && i < line_end && src[i] == ' ') {
                            i++; absorbed++;
                        }
                        line_growth += (grown - absorbed);
                        rewritten = true;
                        continue;
                    }
                    dst[di++] = src[i++];
                }
                if (rewritten) {
                    pending_pad = line_growth;
                } else if (!is_continuation && !is_blank_line) {
                    pending_pad = 0;
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

    // Build section anchor list. Section headers look like
    //   |L|%==|H|Section Title|L|==%|U|
    // and always sit at the start of a line. Walk tb->text once,
    // recording the line index (number of \n encountered before the
    // line) for each match. Two-pass: count, then fill.
    {
        const char *t = tb->text;
        int line_idx = 0;
        int count = 0;
        for (int i = 0; t[i]; ) {
            int line_start = i;
            int line_end = line_start;
            while (t[line_end] && t[line_end] != '\n') line_end++;
            // Match the marker at the very start of the line, allowing
            // for one optional leading space (defensive — the file's
            // headers all start at column 0 today).
            int p = line_start;
            if (t[p] == ' ') p++;
            if (line_end - p >= 8 && strncmp(t + p, "|L|%==|H|", 9) == 0) {
                count++;
            }
            line_idx++;
            i = (t[line_end] == '\n') ? line_end + 1 : line_end;
        }
        if (count > 0) {
            section_lines = new int[count];
            section_count = 0;
            line_idx = 0;
            for (int i = 0; t[i]; ) {
                int line_start = i;
                int line_end = line_start;
                while (t[line_end] && t[line_end] != '\n') line_end++;
                int p = line_start;
                if (t[p] == ' ') p++;
                if (line_end - p >= 8 && strncmp(t + p, "|L|%==|H|", 9) == 0) {
                    section_lines[section_count++] = line_idx;
                }
                line_idx++;
                i = (t[line_end] == '\n') ? line_end + 1 : line_end;
            }
        }
    }
}

CUI_Help::~CUI_Help() {
    TextBox *tb;
    if (needfree) {
        tb = (TextBox *)UI->get_element(0);
        delete[] tb->text;
    }
    if (section_lines) {
        delete[] section_lines;
        section_lines = NULL;
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
    KBMod kstate = 0;
    // Peek the buffered key BEFORE TextBox::update() consumes it, so
    // Tab/Shift-Tab can be intercepted for section navigation rather
    // than scrolling line-by-line.
    if (Keys.size()) {
        key = Keys.checkkey();
        kstate = Keys.cur_state;
        if (key == SDLK_TAB && section_count > 0) {
            Keys.getkey(); // consume
            if (kstate & KS_SHIFT) {
                // Previous section: largest section_lines[i] strictly
                // less than current startline. If we're already at or
                // before the first section, wrap to the last.
                int target = section_lines[section_count - 1];
                for (int i = section_count - 1; i >= 0; i--) {
                    if (section_lines[i] < tb->startline) {
                        target = section_lines[i];
                        break;
                    }
                }
                tb->startline = target;
            } else {
                // Next section: smallest section_lines[i] strictly
                // greater than current startline. If already at/past
                // the last, wrap to the first.
                int target = section_lines[0];
                int found = 0;
                for (int i = 0; i < section_count; i++) {
                    if (section_lines[i] > tb->startline) {
                        target = section_lines[i];
                        found = 1;
                        break;
                    }
                }
                if (!found) target = section_lines[0];
                tb->startline = target;
            }
            tb->bEof = false;
            // The TextBox only repaints when its element-level need_redraw
            // flag is set; bumping the global need_refresh alone leaves the
            // visible scroll position stale until the page is left+re-entered.
            UI->full_refresh();
            need_refresh++;
            return;
        }
    }
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
        printtitle(PAGE_TITLE_ROW_Y,"Help (F1)",COLORS.Text,COLORS.Background,S);
        need_refresh = 0; updated=2;
        ztPlayer->num_orders();
        S->unlock();
    }
}