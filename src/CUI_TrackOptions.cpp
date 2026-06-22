// --------------------------------------------------------------------------------------
//  Track Options popup
// --------------------------------------------------------------------------------------

#include "zt.h"

#define TO_WIN_W_CHARS   54
#define TO_WIN_H_CHARS   17


// ------------------------------------------------------------------------------------------------
//
//
static void to_window_origin(int *start_x, int *start_y, int *w, int *h)
{
    *w = TO_WIN_W_CHARS * col(1);
    *h = TO_WIN_H_CHARS * row(1);
    int sx = (INTERNAL_RESOLUTION_X / 2) - (*w / 2);
    for (; sx % 8; sx--) ;  // <Manu> &= ~7; ?
    int sy = (INTERNAL_RESOLUTION_Y / 2) - (*h / 2);
    for (; sy % 8; sy--) ;
    *start_x = sx;
    *start_y = sy;
}


// ------------------------------------------------------------------------------------------------
//
//
CUI_TrackOptions::CUI_TrackOptions(void)
{
    UI = new UserInterface;

    int start_x, start_y, window_width, window_height;
    to_window_origin(&start_x, &start_y, &window_width, &window_height);

    use_color = 0;

    ti_name = new TextInput;
    UI->add_element(ti_name, 0);
    ti_name->frame = 1;
    ti_name->x = (start_x / 8) + 17;
    ti_name->y = (start_y / 8) + 4;
    ti_name->xsize = ZTM_TRACKNAME_MAXLEN - 1;
    ti_name->length = ZTM_TRACKNAME_MAXLEN - 1;
    ti_name->str = (unsigned char *)&song->track_name[cur_edit_track][0];

    cb_color = new CheckBox;
    UI->add_element(cb_color, 1);
    cb_color->frame = 1;
    cb_color->x = (start_x / 8) + 17;
    cb_color->y = (start_y / 8) + 6;
    cb_color->xsize = 3;
    cb_color->value = &use_color;

    vs_r = new ValueSlider;
    UI->add_element(vs_r, 2);
    vs_r->frame = 1;
    vs_r->x = (start_x / 8) + 17;
    vs_r->y = (start_y / 8) + 10;
    vs_r->xsize = window_width / 8 - 23;
    vs_r->min = 0; vs_r->max = 255; vs_r->value = 208;

    vs_g = new ValueSlider;
    UI->add_element(vs_g, 3);
    vs_g->frame = 1;
    vs_g->x = (start_x / 8) + 17;
    vs_g->y = (start_y / 8) + 12;
    vs_g->xsize = window_width / 8 - 23;
    vs_g->min = 0; vs_g->max = 255; vs_g->value = 48;

    vs_b = new ValueSlider;
    UI->add_element(vs_b, 4);
    vs_b->frame = 1;
    vs_b->x = (start_x / 8) + 17;
    vs_b->y = (start_y / 8) + 14;
    vs_b->xsize = window_width / 8 - 23;
    vs_b->min = 0; vs_b->max = 255; vs_b->value = 48;
}



// ------------------------------------------------------------------------------------------------
//
//
void CUI_TrackOptions::apply_color(void)
{
    if (use_color) {
        unsigned int r = (unsigned int)((ValueSlider *)UI->get_element(2))->value;
        unsigned int g = (unsigned int)((ValueSlider *)UI->get_element(3))->value;
        unsigned int b = (unsigned int)((ValueSlider *)UI->get_element(4))->value;
        song->track_color[cur_edit_track] =
            0xFF000000u | (r << 16) | (g << 8) | b;
    } else {
        song->track_color[cur_edit_track] = 0;
    }
}

// ------------------------------------------------------------------------------------------------
//
//
void CUI_TrackOptions::enter(void)
{
    cur_state = STATE_TRACKOPTS_WIN;

    ti_name = (TextInput *)UI->get_element(0);
    if (!song->track_name[cur_edit_track][0]) {
        snprintf(song->track_name[cur_edit_track], ZTM_TRACKNAME_MAXLEN, "Track %.2d", cur_edit_track + 1);
    }
    
    ti_name->str = (unsigned char *)&song->track_name[cur_edit_track][0];
    ti_name->cursor = (int)strnlen(song->track_name[cur_edit_track], ZTM_TRACKNAME_MAXLEN - 1);

    unsigned long tc = song->track_color[cur_edit_track];
    use_color = tc ? 1 : 0;
    if (tc) {
        ((ValueSlider *)UI->get_element(2))->value = (tc >> 16) & 0xFF;
        ((ValueSlider *)UI->get_element(3))->value = (tc >>  8) & 0xFF;
        ((ValueSlider *)UI->get_element(4))->value = (tc      ) & 0xFF;
    }
    cb_color = (CheckBox *)UI->get_element(1);
    cb_color->value = &use_color;

    need_refresh++;
}



// ------------------------------------------------------------------------------------------------
//
//
void CUI_TrackOptions::leave(void)
{
    cur_state = STATE_PEDIT;
}



// ------------------------------------------------------------------------------------------------
//
//
void CUI_TrackOptions::update(void)
{
    int key = 0;

    UI->update();

    CheckBox    *cb = (CheckBox *)UI->get_element(1);
    ValueSlider *r  = (ValueSlider *)UI->get_element(2);
    ValueSlider *g  = (ValueSlider *)UI->get_element(3);
    ValueSlider *b  = (ValueSlider *)UI->get_element(4);

    if (cb->changed || r->changed || g->changed || b->changed) {
        apply_color();
        need_refresh++;
        need_popup_refresh++;
    }

    if (Keys.size()) {
        key = Keys.getkey();
        // F2 -> pattern options
        if (key == SDLK_F2) {
            apply_color();
            close_popup_window();
            popup_window(UIP_PEParms);
            fixmouse++;
            need_refresh++;
            return;
        }
        if (key == SDLK_ESCAPE || key == SDLK_RETURN ||
            key == ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_RIGHT))) {
            apply_color();
            close_popup_window();
            fixmouse++;
            need_refresh++;
            return;
        }
    }
}


// ------------------------------------------------------------------------------------------------
//
//
void CUI_TrackOptions::draw(Drawable *S)
{
    int start_x, start_y, window_width, window_height;
    to_window_origin(&start_x, &start_y, &window_width, &window_height);

    ti_name->x = (start_x / 8) + 17;
    ti_name->y = (start_y / 8) + 3;
    
    cb_color->x = (start_x / 8) + 17;
    cb_color->y = (start_y / 8) + 5;
    
    vs_r->x = (start_x / 8) + 17;
    vs_r->y = (start_y / 8) + 10;
    
    vs_g->x = (start_x / 8) + 17;
    vs_g->y = (start_y / 8) + 12;
    
    vs_b->x = (start_x / 8) + 17;
    vs_b->y = (start_y / 8) + 14;

    if (S->lock() == 0) {
        
        S->fillRect(start_x, start_y, start_x + window_width, start_y + window_height, COLORS.Background);
        printline(start_x, start_y + window_height - row(1) + 1, 148, window_width / 8, COLORS.Lowlight, S);
        
        for (int i = start_y / row(1); i < (start_y + window_height) / row(1); i++) {
            printchar(start_x, row(i), 145, COLORS.Highlight, S);
            printchar(start_x + window_width - row(1) + 1, row(i), 146, COLORS.Lowlight, S);
        }
        
        printline(start_x, start_y, 143, window_width / 8, COLORS.Highlight, S);
        print(col(textcenter("Track Options")), start_y + row(1), "Track Options", COLORS.Text, S);

        print(start_x + col(2), start_y + row(3),  "   Track Name:", COLORS.Text, S);
        print(start_x + col(2), start_y + row(5),  " Custom Color:", COLORS.Text, S);
        print(start_x + col(2), start_y + row(10), "          Red:", COLORS.Text, S);
        print(start_x + col(2), start_y + row(12), "        Green:", COLORS.Text, S);
        print(start_x + col(2), start_y + row(14), "         Blue:", COLORS.Text, S);

        // <Manu> Vale la pena tener una preview si se ve el color detrás?
        {
            unsigned long sw = use_color
                ? (0xFF000000u | (((unsigned long)vs_r->value) << 16)
                              | (((unsigned long)vs_g->value) << 8)
                              |  ((unsigned long)vs_b->value))
                : COLORS.EditBG;
            int sx = start_x + col(17);
            int sy = start_y + row(7);
            int ex = sx + col(window_width / 8 - 23);
            int ey = sy + row(2) - 2;
            S->fillRect(sx, sy, ex, ey, sw);
            print(start_x + col(2), start_y + row(7), "Color Preview:", COLORS.Text, S);
        }

        // <Manu> Deberíamos imprimir algún texto avisando de que se puede ir a la otra pantalla?
        // print(start_x + col(2), start_y + row(18), "F2: Go to Pattern Options", COLORS.Lowlight, S);

        UI->full_refresh();
        UI->draw(S);
        S->unlock();
        need_refresh = need_popup_refresh = 0;
        updated++;
    }
}
