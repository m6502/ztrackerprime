#include "zt.h"
#include "CUI_PaletteEditor.h"
#include "Skins.h"
#include <stdio.h>
#include <string.h>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <string>

extern void make_toolbar(void);
extern void redrawscreen(Drawable *S);
extern Screen *screen_buffer;

static void palette_changed(void);

// ---------------------------------------------------------------------------
// Palette Editor (Shift+Ctrl+F12)
//
// Inspired by Impulse Tracker v2.14's Palette Configuration screen.
// Displays every editable COLORS.* slot as a swatch + click-and-drag RGB
// sliders, plus a preset panel listing every shipped palette/*.conf and
// every skin's colors.conf so the user can audition any look without
// leaving the editor. Global brightness / contrast / tint sliders sit at
// the bottom and operate on the snapshot taken on entry, so adjustments
// are composable and reversible (ESC restores the snapshot exactly).
//
// Layout (computed from the canvas size every frame, see pe_layout()):
//
//   Colours            selected colour                Palettes & Skins
//   ## Background      Name  #RRGGBB                  Acid Gas
//   ## Highlight       [swatch]   R [=========] 164   ...
//   ...  (all 18)                 G / B               [skin] default
//                      pattern-editor preview         Bright / Contrast /
//                      (rows, beats, cursor row,      Tint bars, Hue,
//                       selection, caret, LCD bars)   [ ] Reset skin
//
// Navigation:
//   Tab / Shift+Tab      - cycle colour list -> R/G/B sliders -> presets
//   Colour list          - Up/Down pick a colour, Right/Enter edit it
//   Sliders              - Up/Down pick R/G/B, Left/Right -/+1 (Shift x16),
//                          0..9 type a value, +/- also step
//   Presets              - Up/Down/PgUp/PgDn/Home/End move, Enter loads,
//                          Left goes back to the colour list
//   R / G / B            - jump to that channel of the selected colour
//   Mouse                - click a colour, preset or bar; drag bars to scrub
//   [ / ]                - global brightness -/+ 8
//   ; / '                - global contrast   -/+ 4
//   T                    - cycle global tint hue (none, purple, red, blue,
//                          green, amber, cyan, magenta)
//   Y / U                - tint amount -/+ 8
//   Ctrl+S               - save current palette to palettes/custom.conf
//   ESC                  - revert to snapshot taken on entry and return
//                          to pattern editor
// ---------------------------------------------------------------------------

#define PE_LIST_W           17   // colour list: 2-col swatch, gap, 14-col name
#define PE_PRESET_W         22   // preset panel content width
#define PE_SWATCH_W         6    // big swatch of the selected colour
#define PE_SWATCH_H         3
#define PE_CHECKBOX_LABEL   "Reset skin on load"

struct PaletteSlot {
    const char *name;
    size_t      offset;
};

static const PaletteSlot g_slots[NUM_PALETTE_SLOTS] = {
    { "Background",     offsetof(colorset, Background) },
    { "Highlight",      offsetof(colorset, Highlight) },
    { "Lowlight",       offsetof(colorset, Lowlight) },
    { "Text",           offsetof(colorset, Text) },
    { "Brighttext",     offsetof(colorset, Brighttext) },
    { "Data",           offsetof(colorset, Data) },
    { "Black",          offsetof(colorset, Black) },
    { "EditText",       offsetof(colorset, EditText) },
    { "EditBG",         offsetof(colorset, EditBG) },
    { "EditBGlow",      offsetof(colorset, EditBGlow) },
    { "EditBGhigh",     offsetof(colorset, EditBGhigh) },
    { "SelectedBGLow",  offsetof(colorset, SelectedBGLow) },
    { "SelectedBGHigh", offsetof(colorset, SelectedBGHigh) },
    { "CursorRowLow",   offsetof(colorset, CursorRowLow) },
    { "CursorRowHigh",  offsetof(colorset, CursorRowHigh) },
    { "LCDLow",         offsetof(colorset, LCDLow) },
    { "LCDMid",         offsetof(colorset, LCDMid) },
    { "LCDHigh",        offsetof(colorset, LCDHigh) },
};

struct StaticPreset {
    const char *label;
    const char *file;
};

static const StaticPreset g_palette_presets[] = {
    // Hand-tuned palettes that have shipped since the early forks.
    { "Light Blue",        "light_blue.conf" },
    { "Gold",              "gold.conf" },
    { "Camouflage",        "camouflage.conf" },
    { "Midnight Tracking", "midnight_tracking.conf" },
    { "Pine Colours",      "pine_colours.conf" },
    { "Soundtracker",      "soundtracker.conf" },
    { "Volcanic",          "volcanic.conf" },
    // Imported from Schism Tracker via scripts/import-schism-palettes.py.
    // The first 7 above are already-shipping zT' versions; these 9 are
    // the additional Schism palettes that didn't have a zT counterpart.
    { "Industrial",        "industrial.conf" },
    { "Purple Motion",     "purple_motion.conf" },
    { "Why Colors?",       "why_colors.conf" },
    { "Kawaii",            "kawaii.conf" },
    { "Gold (Vintage)",    "gold_vintage.conf" },
    { "FX 2.0",            "fx_2_0.conf" },
    { "Atlantic",          "atlantic.conf" },
    { "Acid Gas",          "acid_gas.conf" },
    { "Hotdog Stand",      "hotdog_stand.conf" },
};
static const int NUM_STATIC_PRESETS =
    (int)(sizeof(g_palette_presets) / sizeof(g_palette_presets[0]));

struct TintEntry { const char *label; TColor color; };
static const TintEntry g_tints[] = {
    { "None",    0xFF000000 },     // marker; tint_amount ignored when index==0
    { "Purple",  0xFF8000C0 },
    { "Red",     0xFFD03030 },
    { "Blue",    0xFF3060D0 },
    { "Green",   0xFF30A050 },
    { "Amber",   0xFFE08020 },
    { "Cyan",    0xFF20B0C0 },
    { "Magenta", 0xFFC020A0 },
};
static const int NUM_TINTS = (int)(sizeof(g_tints) / sizeof(g_tints[0]));

static TColor *slot_color_ptr(int slot) {
    if (slot < 0 || slot >= NUM_PALETTE_SLOTS) return NULL;
    char *base = (char *)&COLORS;
    return (TColor *)(base + g_slots[slot].offset);
}

static void unpack_rgb(TColor c, unsigned char *r, unsigned char *g, unsigned char *b) {
    *r = (c >> 16) & 0xFF;
    *g = (c >>  8) & 0xFF;
    *b = (c      ) & 0xFF;
}

static TColor pack_rgb(unsigned char r, unsigned char g, unsigned char b) {
    return (TColor)(0xFF000000u | ((TColor)b) | (((TColor)g) << 8) | (((TColor)r) << 16));
}

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Apply brightness (additive), contrast (multiplicative around 128) and
// tint (linear blend toward tint_color by amount/255) to a single color.
static TColor apply_xform(TColor src, int brightness, int contrast,
                          int tint_idx, int tint_amount) {
    unsigned char r, g, b;
    unpack_rgb(src, &r, &g, &b);

    // Contrast factor: contrast in [-100..+100] -> factor in [0..2.0].
    // Use integer math: factor_q8 = 256 + contrast * 256 / 100, clamped.
    int factor_q8 = 256 + (contrast * 256) / 100;
    if (factor_q8 < 0) factor_q8 = 0;
    if (factor_q8 > 1024) factor_q8 = 1024;

    int rr = ((((int)r - 128) * factor_q8) >> 8) + 128 + brightness;
    int gg = ((((int)g - 128) * factor_q8) >> 8) + 128 + brightness;
    int bb = ((((int)b - 128) * factor_q8) >> 8) + 128 + brightness;
    rr = clampi(rr, 0, 255);
    gg = clampi(gg, 0, 255);
    bb = clampi(bb, 0, 255);

    if (tint_idx > 0 && tint_idx < NUM_TINTS && tint_amount > 0) {
        unsigned char tr, tg, tb;
        unpack_rgb(g_tints[tint_idx].color, &tr, &tg, &tb);
        int a = clampi(tint_amount, 0, 255);
        int inv = 255 - a;
        rr = (rr * inv + tr * a) / 255;
        gg = (gg * inv + tg * a) / 255;
        bb = (bb * inv + tb * a) / 255;
    }

    return pack_rgb((unsigned char)rr, (unsigned char)gg, (unsigned char)bb);
}

CUI_PaletteEditor::CUI_PaletteEditor(void) {
    UI = new UserInterface;
    selected_slot = 0;
    focus_panel = 0;
    channel_edit = 0;
    preset_sel = 0;
    preset_top = 0;
    dirty = 0;
    status_line[0] = '\0';
    num_presets = 0;
    brightness = 0;
    contrast = 0;
    tint_index = 0;
    tint_amount = 0;
    reset_skin_on_palette = 1;   // canonical look by default
}

CUI_PaletteEditor::~CUI_PaletteEditor(void) {
    if (UI) delete UI;
    UI = NULL;
}

void CUI_PaletteEditor::rebuild_preset_list(void) {
    num_presets = 0;

    // 1. Static palette presets (palettes/*.conf next to the binary),
    //    listed alphabetically by label. (Labels are Title Case, so a
    //    plain strcmp is already alphabetical; selection is by path, not
    //    index, so reordering the display is safe.)
    int order[NUM_STATIC_PRESETS];
    for (int i = 0; i < NUM_STATIC_PRESETS; ++i) order[i] = i;
    std::sort(order, order + NUM_STATIC_PRESETS, [](int a, int b) {
        return strcmp(g_palette_presets[a].label, g_palette_presets[b].label) < 0;
    });
    for (int k = 0; k < NUM_STATIC_PRESETS && num_presets < 64; ++k) {
        int i = order[k];
        snprintf(preset_label[num_presets], sizeof(preset_label[0]),
                 "%s", g_palette_presets[i].label);
#if defined(_WIN32)
        snprintf(preset_path[num_presets], sizeof(preset_path[0]),
                 "%s\\palettes\\%s",
                 cur_dir ? cur_dir : ".", g_palette_presets[i].file);
#else
        snprintf(preset_path[num_presets], sizeof(preset_path[0]),
                 "%s/palettes/%s",
                 cur_dir ? cur_dir : ".", g_palette_presets[i].file);
#endif
        preset_is_skin[num_presets] = 0;
        num_presets++;
    }

    // 2. Skin colors.conf — every skins/<name>/colors.conf becomes a preset.
    //    Sorted alphabetically; "old_0.85_skins" container is skipped because
    //    it has no colors.conf of its own, only legacy subfolders.
    char skinroot[PE_PRESET_PATH_LEN];
#if defined(_WIN32)
    snprintf(skinroot, sizeof(skinroot), "%s\\skins", cur_dir ? cur_dir : ".");
#else
    snprintf(skinroot, sizeof(skinroot), "%s/skins", cur_dir ? cur_dir : ".");
#endif
    std::error_code ec;
    if (std::filesystem::is_directory(skinroot, ec)) {
        std::vector<std::filesystem::path> skin_dirs;
        for (auto &entry : std::filesystem::directory_iterator(skinroot, ec)) {
            if (ec) break;
            if (!entry.is_directory()) continue;
            auto colors = entry.path() / "colors.conf";
            if (std::filesystem::exists(colors, ec)) {
                skin_dirs.push_back(entry.path());
            }
        }
        std::sort(skin_dirs.begin(), skin_dirs.end());
        for (auto &p : skin_dirs) {
            if (num_presets >= 64) break;
            std::string name = p.filename().string();
            snprintf(preset_label[num_presets], sizeof(preset_label[0]),
                     "[skin] %s", name.c_str());
            auto colors = p / "colors.conf";
            snprintf(preset_path[num_presets], sizeof(preset_path[0]),
                     "%s", colors.string().c_str());
            preset_is_skin[num_presets] = 1;
            num_presets++;
        }
    }
}

void CUI_PaletteEditor::enter(void) {
    need_refresh = 1;
    cur_state = STATE_PALETTE_EDITOR;
    for (int i = 0; i < NUM_PALETTE_SLOTS; ++i) {
        snapshot[i] = *slot_color_ptr(i);
        entry_colors[i] = snapshot[i];
    }
    snprintf(entry_skin, sizeof(entry_skin), "%s",
             CurrentSkin ? CurrentSkin->strSkinName : "");
    dirty = 0;
    channel_edit = 0;
    brightness = 0;
    contrast = 0;
    tint_index = 0;
    tint_amount = 0;
    if (selected_slot < 0) selected_slot = 0;
    if (selected_slot >= NUM_PALETTE_SLOTS) selected_slot = 0;
    status_line[0] = '\0';
    rebuild_preset_list();
    Keys.flush();
    UI->enter();
}

void CUI_PaletteEditor::leave(void) {
    channel_edit = 0;
}

void CUI_PaletteEditor::load_palette_file(const char *path_or_fname) {
    // Accepts either a bare filename (resolved against palettes/) or a
    // full path (used by skin presets).
    char path[PE_PRESET_PATH_LEN];
    if (strchr(path_or_fname, '/') || strchr(path_or_fname, '\\')) {
        snprintf(path, sizeof(path), "%s", path_or_fname);
    } else {
#if defined(_WIN32)
        snprintf(path, sizeof(path), "%s\\palettes\\%s",
                 cur_dir ? cur_dir : ".", path_or_fname);
#else
        snprintf(path, sizeof(path), "%s/palettes/%s",
                 cur_dir ? cur_dir : ".", path_or_fname);
#endif
    }

    // Palette presets render against the default skin's PNG templates by
    // default — that's the canonical look they were designed for. The user
    // can disable this via the "Reset skin to default" checkbox to instead
    // overlay the palette on whatever skin is currently loaded.
    if (reset_skin_on_palette
        && CurrentSkin
        && strcmp(CurrentSkin->strSkinName, "default") != 0) {
        Skin *old = CurrentSkin;
        char def[] = "default";
        Skin *todel = CurrentSkin->switchskin(def);
        if (old == todel) {
            strcpy(zt_config_globals.skin, CurrentSkin->strSkinName);
        }
        delete todel;
    }

    if (COLORS.load(path)) {
        for (int i = 0; i < NUM_PALETTE_SLOTS; ++i) {
            snapshot[i] = *slot_color_ptr(i);
        }
        brightness = 0;
        contrast = 0;
        tint_index = 0;
        tint_amount = 0;
        dirty = 1;
        snprintf(status_line, sizeof(status_line), "Loaded: %s", path);
        palette_changed();
    } else {
        snprintf(status_line, sizeof(status_line), "Failed to load %s", path);
    }
}

// Pull the skin name out of a path like "<cur_dir>/skins/<name>/colors.conf".
static void extract_skin_name(const char *path, char *out, size_t outsz) {
    out[0] = '\0';
    const char *colors = strstr(path, "colors.conf");
    if (!colors || colors == path) return;
    const char *sep_after = colors - 1;
    if (*sep_after != '/' && *sep_after != '\\') return;
    const char *sep_before = sep_after - 1;
    while (sep_before > path && *sep_before != '/' && *sep_before != '\\') sep_before--;
    if (sep_before <= path) return;
    size_t len = (size_t)(sep_after - sep_before - 1);
    if (len >= outsz) len = outsz - 1;
    memcpy(out, sep_before + 1, len);
    out[len] = '\0';
}

// Full skin switch — same code path as the F12 Sysconfig skin selector. Loads
// new toolbar.png/buttons.png/font.fnt etc., updates CurrentSkin, frees the
// old skin. Must be used (not COLORS.load alone) when the user picks a
// "[skin] xxx" preset, otherwise the new skin's authoring colors and PNG
// templates never get adopted.
void CUI_PaletteEditor::load_skin_full(const char *colors_conf_path) {
    char skin_name[64];
    extract_skin_name(colors_conf_path, skin_name, sizeof(skin_name));
    if (!skin_name[0] || !CurrentSkin) {
        // Fall back to plain palette load if we can't parse the path.
        load_palette_file(colors_conf_path);
        return;
    }
    Skin *old = CurrentSkin;
    Skin *todel = CurrentSkin->switchskin(skin_name);
    if (old == todel) {
        // Success — switchskin set CurrentSkin to the new instance and
        // returned the old `this` for us to delete.
        strcpy(zt_config_globals.skin, CurrentSkin->strSkinName);
    }
    delete todel;

    // Bring the snapshot in sync with the new skin's authoring colors and
    // reset global adjustments so the user starts fresh on this skin.
    for (int i = 0; i < NUM_PALETTE_SLOTS; ++i) {
        snapshot[i] = *slot_color_ptr(i);
    }
    brightness = 0;
    contrast = 0;
    tint_index = 0;
    tint_amount = 0;
    dirty = 0;
    snprintf(status_line, sizeof(status_line), "Switched to skin: %s", skin_name);
    // No recolor needed — switchskin already loaded the new PNGs at their
    // authored colors. Force a redraw on the next frame.
    doredraw++;
    need_refresh++;
    screenmanager.UpdateAll();
}

void CUI_PaletteEditor::save_palette_file(const char *fname) {
    char path[PE_PRESET_PATH_LEN];
#if defined(_WIN32)
    snprintf(path, sizeof(path), "%s\\palettes\\%s", cur_dir ? cur_dir : ".", fname);
#else
    snprintf(path, sizeof(path), "%s/palettes/%s", cur_dir ? cur_dir : ".", fname);
#endif
    FILE *f = fopen(path, "w");
    if (!f) {
        snprintf(status_line, sizeof(status_line), "Cannot open %s for writing", fname);
        return;
    }
    for (int i = 0; i < NUM_PALETTE_SLOTS; ++i) {
        TColor c = *slot_color_ptr(i);
        unsigned char r, g, b;
        unpack_rgb(c, &r, &g, &b);
        char name_with_colon[64];
        snprintf(name_with_colon, sizeof(name_with_colon), "%s:", g_slots[i].name);
        fprintf(f, "%-16s #%02X%02X%02X\n", name_with_colon, r, g, b);
    }
    fclose(f);
    snprintf(status_line, sizeof(status_line), "Saved palette to palettes/%s", fname);
}

// Single funnel for "the palette just changed" so the toolbar/buttons PNG
// stays in sync with COLORS.* no matter which path edited it.
//
// Earlier versions called redrawscreen() inline here to make the toolbar
// and page-background update land in the same frame as the click, but that
// re-enters the screen-draw pipeline while the page's own update() is still
// running — the symptom was the Pattern Editor coming back from ESC with
// its track frame pushed down the screen because layout state got
// corrupted. Drop the inline redraw; the 1-frame delay is acceptable, the
// crashed layout was not.
static void palette_changed(void) {
    if (CurrentSkin) {
        CurrentSkin->recolor_to_palette(COLORS.Lowlight,
                                        COLORS.Background,
                                        COLORS.Highlight);
        make_toolbar();
    }
    doredraw++;
    need_refresh++;
    screenmanager.UpdateAll();
}

void CUI_PaletteEditor::recompute_globals(void) {
    for (int i = 0; i < NUM_PALETTE_SLOTS; ++i) {
        TColor *c = slot_color_ptr(i);
        if (!c) continue;
        *c = apply_xform(snapshot[i], brightness, contrast, tint_index, tint_amount);
    }
    dirty = 1;
    palette_changed();
}

void CUI_PaletteEditor::apply_channel_delta(int delta) {
    if (channel_edit < 1 || channel_edit > 3) return;
    TColor *c = slot_color_ptr(selected_slot);
    if (!c) return;
    unsigned char r, g, b;
    unpack_rgb(*c, &r, &g, &b);
    int v = 0;
    if (channel_edit == 1) v = r;
    else if (channel_edit == 2) v = g;
    else v = b;
    v = clampi(v + delta, 0, 255);
    if (channel_edit == 1) r = (unsigned char)v;
    else if (channel_edit == 2) g = (unsigned char)v;
    else b = (unsigned char)v;
    *c = pack_rgb(r, g, b);
    // Direct edits become the new anchor for that slot so subsequent global
    // tweaks don't undo manual work.
    snapshot[selected_slot] = *c;
    dirty = 1;
    palette_changed();
}

void CUI_PaletteEditor::apply_channel_set(int value) {
    if (channel_edit < 1 || channel_edit > 3) return;
    value = clampi(value, 0, 255);
    TColor *c = slot_color_ptr(selected_slot);
    if (!c) return;
    unsigned char r, g, b;
    unpack_rgb(*c, &r, &g, &b);
    if (channel_edit == 1) r = (unsigned char)value;
    else if (channel_edit == 2) g = (unsigned char)value;
    else b = (unsigned char)value;
    *c = pack_rgb(r, g, b);
    snapshot[selected_slot] = *c;
    dirty = 1;
    palette_changed();
}

// ---------------------------------------------------------------------------
// Layout. Shared by draw() and update() so click hit-tests always match
// what is on screen. Everything is in character cells unless noted.
// ---------------------------------------------------------------------------

struct PeLayout {
    int cols, rows;
    int top, bottom;        // content rows, inclusive
    int list_x;             // colour list: swatch list_x..+1, name from list_x+3
    int mid_x, mid_w;       // middle column (selected colour + preview)
    int pre_x, pre_w;       // preset panel content (frame drawn around it)
    int pre_y, pre_rows;    // first preset row, visible preset rows
    int glob_y;             // Bright; Contrast +1; Tint +2; Hue +3; Reset +4
    int prev_y;             // pattern preview: track header row
    int hint_y;             // hint, global keys, status: hint_y .. hint_y+2
};

static PeLayout pe_layout(void) {
    PeLayout L;
    L.cols = INTERNAL_RESOLUTION_X / 8;
    L.rows = INTERNAL_RESOLUTION_Y / 8;
    L.top = PAGE_TITLE_ROW_Y + 2;
    L.hint_y = L.rows - 9;          // three lines, above the toolbar
    L.bottom = L.hint_y - 2;
    L.list_x = 1;
    L.pre_w = PE_PRESET_W;
    L.pre_x = L.cols - 2 - L.pre_w;
    L.mid_x = L.list_x + PE_LIST_W + 2;
    L.mid_w = L.pre_x - 2 - L.mid_x;
    L.glob_y = L.bottom - 4;
    L.pre_y = L.top + 2;            // row L.top is the panel header, then a gap
    L.pre_rows = (L.glob_y - 2) - L.pre_y + 1;
    if (L.pre_rows < 3) L.pre_rows = 3;
    L.prev_y = L.top + 6;
    return L;
}

static int pe_hit(int mx, int my, int x0, int y0, int x1, int y1) {
    return mx >= x0 && mx < x1 && my >= y0 && my < y1;
}

// Pixel rect of colour-list entry i.
static void list_item_rect(const PeLayout &L, int i, int *x0, int *y0, int *x1, int *y1) {
    *x0 = col(L.list_x);
    *x1 = col(L.list_x + PE_LIST_W);
    *y0 = row(L.top + 1 + i);
    *y1 = *y0 + row(1);
}

// Pixel rect of the R (1) / G (2) / B (3) bar of the selected colour.
static void rgb_slider_rect(const PeLayout &L, int channel, int *x0, int *y0, int *x1, int *y1) {
    *x0 = col(L.mid_x + PE_SWATCH_W + 4);
    *x1 = col(L.mid_x + L.mid_w - 4);
    *y0 = row(L.top + 1 + channel);
    *y1 = *y0 + row(1) - 2;
}

// Pixel rect of global bar 0 = brightness, 1 = contrast, 2 = tint amount.
static void global_slider_rect(const PeLayout &L, int which, int *x0, int *y0, int *x1, int *y1) {
    *x0 = col(L.pre_x + 9);
    *x1 = col(L.pre_x + L.pre_w - 5);
    *y0 = row(L.glob_y + which);
    *y1 = *y0 + row(1) - 2;
}

static int slider_value_at(int mx, int x0, int x1, int range) {
    return clampi((mx - x0) * range / (x1 - x0 - 1), 0, range);
}

// Keep the selected preset inside the visible part of the list.
void CUI_PaletteEditor::clamp_preset_view(void) {
    const PeLayout L = pe_layout();
    if (num_presets <= 0) { preset_sel = 0; preset_top = 0; return; }
    preset_sel = clampi(preset_sel, 0, num_presets - 1);
    if (preset_sel < preset_top) preset_top = preset_sel;
    if (preset_sel >= preset_top + L.pre_rows) preset_top = preset_sel - L.pre_rows + 1;
    preset_top = clampi(preset_top, 0, num_presets > L.pre_rows ? num_presets - L.pre_rows : 0);
}

void CUI_PaletteEditor::update(void) {
    static int digit_accum = -1;
    static int digit_channel = 0;
    // Mouse-drag tracking for slider scrubbing while button is held.
    // 0 = no drag, 1..3 = R/G/B bar, 10/11/12 = global brightness/
    // contrast/tint amount.
    static int drag_kind = 0;

    const PeLayout L = pe_layout();

    auto set_global = [&](int which, int x) {
        int gx0, gy0, gx1, gy1;
        global_slider_rect(L, which, &gx0, &gy0, &gx1, &gy1);
        int frac = slider_value_at(x, gx0, gx1, 1000);
        if (which == 0)      brightness = -128 + (frac * 255) / 1000;
        else if (which == 1) contrast   = -100 + (frac * 200) / 1000;
        else {
            tint_amount = (frac * 255) / 1000;
            // Scrubbing the amount with no hue selected would do nothing;
            // pick the first real hue so the drag is visible.
            if (tint_amount > 0 && tint_index == 0) tint_index = 1;
        }
        recompute_globals();
    };

    UI->update();
    if (!Keys.size()) {
        // Continue scrubbing while the button is held.
        if (drag_kind && bMouseIsDown) {
            int x = LastX;
            if (drag_kind >= 1 && drag_kind <= 3) {
                int rx0, ry0, rx1, ry1;
                rgb_slider_rect(L, drag_kind, &rx0, &ry0, &rx1, &ry1);
                channel_edit = drag_kind;
                apply_channel_set(slider_value_at(x, rx0, rx1, 255));
                need_refresh++;
            } else if (drag_kind >= 10 && drag_kind <= 12) {
                set_global(drag_kind - 10, x);
                need_refresh++;
            }
        } else if (drag_kind && !bMouseIsDown) {
            drag_kind = 0;
        }
        return;
    }

    KBMod kstate = Keys.getstate();
    KBKey key = Keys.getkey();
    int handled = 1;

    auto reset_accum = [&]() {
        digit_accum = -1;
        digit_channel = 0;
    };
    auto pick_preset = [&](int i) {
        if (i < 0 || i >= num_presets) return;
        if (preset_is_skin[i]) load_skin_full(preset_path[i]);
        else load_palette_file(preset_path[i]);
    };

    if (key == ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_UP << 8) | SDL_BUTTON_LEFT))) {
        drag_kind = 0;
        return;
    }

    if (key == ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_LEFT))) {
        int mx = MousePressX;
        int my = MousePressY;
        reset_accum();
        need_refresh++;

        // Colour list.
        for (int i = 0; i < NUM_PALETTE_SLOTS; ++i) {
            int x0, y0, x1, y1;
            list_item_rect(L, i, &x0, &y0, &x1, &y1);
            if (pe_hit(mx, my, x0, y0, x1, y1)) {
                focus_panel = 0;
                selected_slot = i;
                channel_edit = 0;
                return;
            }
        }

        // R/G/B bars of the selected colour.
        for (int ch = 1; ch <= 3; ++ch) {
            int x0, y0, x1, y1;
            rgb_slider_rect(L, ch, &x0, &y0, &x1, &y1);
            if (pe_hit(mx, my, x0, y0, x1, y1 + 2)) {
                focus_panel = 0;
                channel_edit = ch;
                apply_channel_set(slider_value_at(mx, x0, x1, 255));
                drag_kind = ch;
                return;
            }
        }

        // Presets.
        for (int i = 0; i < L.pre_rows && preset_top + i < num_presets; ++i) {
            int y0 = row(L.pre_y + i);
            if (pe_hit(mx, my, col(L.pre_x), y0, col(L.pre_x + L.pre_w), y0 + row(1))) {
                focus_panel = 1;
                channel_edit = 0;
                preset_sel = preset_top + i;
                pick_preset(preset_sel);
                return;
            }
        }

        // Global bars.
        for (int which = 0; which < 3; ++which) {
            int x0, y0, x1, y1;
            global_slider_rect(L, which, &x0, &y0, &x1, &y1);
            if (pe_hit(mx, my, x0, y0, x1, y1 + 2)) {
                set_global(which, mx);
                drag_kind = 10 + which;
                return;
            }
        }

        // Hue (cycles) and Reset skin rows.
        int rx0 = col(L.pre_x), rx1 = col(L.pre_x + L.pre_w);
        if (pe_hit(mx, my, rx0, row(L.glob_y + 3), rx1, row(L.glob_y + 4))) {
            tint_index = (tint_index + 1) % NUM_TINTS;
            if (tint_index != 0 && tint_amount == 0) tint_amount = 64;
            recompute_globals();
            snprintf(status_line, sizeof(status_line),
                     "Tint: %s (amount %d)", g_tints[tint_index].label, tint_amount);
            return;
        }
        if (pe_hit(mx, my, rx0, row(L.glob_y + 4), rx1, row(L.glob_y + 5))) {
            reset_skin_on_palette = !reset_skin_on_palette;
            snprintf(status_line, sizeof(status_line),
                     "Reset skin to default on palette load: %s",
                     reset_skin_on_palette ? "ON" : "OFF");
            doredraw++;
            screenmanager.UpdateAll();
            return;
        }
        return;
    }

    need_refresh++;

    switch (key) {
        case SDLK_ESCAPE:
            // Back to the skin and colours this page was entered with
            // (`snapshot` is only the anchor for the global adjustments;
            // direct edits and preset loads overwrite it).
            if (CurrentSkin && entry_skin[0] && strcmp(CurrentSkin->strSkinName, entry_skin) != 0) {
                Skin *old = CurrentSkin;
                Skin *todel = CurrentSkin->switchskin(entry_skin);
                if (old == todel) strcpy(zt_config_globals.skin, CurrentSkin->strSkinName);
                delete todel;
            }
            for (int i = 0; i < NUM_PALETTE_SLOTS; ++i) {
                TColor *c = slot_color_ptr(i);
                if (c) *c = entry_colors[i];
                snapshot[i] = entry_colors[i];
            }
            dirty = 0;
            channel_edit = 0;
            // Re-tint the toolbar/buttons back to the snapshot colors before
            // we leave the page, so the pattern editor doesn't briefly show
            // mid-edit colors on the bottom frame.
            palette_changed();
            switch_page(UIP_Patterneditor);
            reset_accum();
            return;

        case SDLK_TAB:
            // colours -> sliders -> presets -> colours (Shift: reverse)
            reset_accum();
            if (kstate & KS_SHIFT) {
                if (focus_panel == 1)       { focus_panel = 0; channel_edit = 1; }
                else if (channel_edit)      { channel_edit = 0; }
                else                        { focus_panel = 1; clamp_preset_view(); }
            } else {
                if (focus_panel == 1)       { focus_panel = 0; channel_edit = 0; }
                else if (channel_edit == 0) { channel_edit = 1; }
                else                        { channel_edit = 0; focus_panel = 1; clamp_preset_view(); }
            }
            return;

        case SDLK_UP:
        case SDLK_DOWN: {
            reset_accum();
            int dir = (key == SDLK_UP) ? -1 : 1;
            if (focus_panel == 1) {
                preset_sel += dir;
                clamp_preset_view();
            } else if (channel_edit) {
                channel_edit = clampi(channel_edit + dir, 1, 3);
            } else {
                selected_slot = clampi(selected_slot + dir, 0, NUM_PALETTE_SLOTS - 1);
            }
            return;
        }

        case SDLK_PAGEUP:
        case SDLK_PAGEDOWN:
        case SDLK_HOME:
        case SDLK_END:
            reset_accum();
            if (focus_panel == 1) {
                if (key == SDLK_PAGEUP)        preset_sel -= L.pre_rows - 1;
                else if (key == SDLK_PAGEDOWN) preset_sel += L.pre_rows - 1;
                else if (key == SDLK_HOME)     preset_sel = 0;
                else                           preset_sel = num_presets - 1;
                clamp_preset_view();
            } else if (!channel_edit) {
                if (key == SDLK_HOME || key == SDLK_PAGEUP) selected_slot = 0;
                else selected_slot = NUM_PALETTE_SLOTS - 1;
            }
            return;

        case SDLK_LEFT:
        case SDLK_RIGHT: {
            reset_accum();
            int dir = (key == SDLK_LEFT) ? -1 : 1;
            if (focus_panel == 1) {
                if (dir < 0) { focus_panel = 0; channel_edit = 0; }
            } else if (channel_edit) {
                apply_channel_delta(dir * ((kstate & KS_SHIFT) ? 16 : 1));
            } else if (dir > 0) {
                channel_edit = 1;
            }
            return;
        }

        case SDLK_RETURN:
            reset_accum();
            if (focus_panel == 1) pick_preset(preset_sel);
            else if (!channel_edit) channel_edit = 1;
            return;

        case SDLK_R:
        case SDLK_G:
        case SDLK_B:
            reset_accum();
            focus_panel = 0;
            channel_edit = (key == SDLK_R) ? 1 : (key == SDLK_G) ? 2 : 3;
            return;

        case SDLK_PLUS:
        case SDLK_EQUALS:
        case SDLK_KP_PLUS:
            reset_accum();
            apply_channel_delta((kstate & KS_SHIFT) ? 16 : 1);
            return;

        case SDLK_MINUS:
        case SDLK_KP_MINUS:
            reset_accum();
            apply_channel_delta((kstate & KS_SHIFT) ? -16 : -1);
            return;

        case SDLK_LEFTBRACKET:
            brightness = clampi(brightness - 8, -128, 127);
            recompute_globals();
            snprintf(status_line, sizeof(status_line), "Brightness %+d", brightness);
            return;

        case SDLK_RIGHTBRACKET:
            brightness = clampi(brightness + 8, -128, 127);
            recompute_globals();
            snprintf(status_line, sizeof(status_line), "Brightness %+d", brightness);
            return;

        case SDLK_SEMICOLON:
            contrast = clampi(contrast - 4, -100, 100);
            recompute_globals();
            snprintf(status_line, sizeof(status_line), "Contrast %+d", contrast);
            return;

        case SDLK_APOSTROPHE:
            contrast = clampi(contrast + 4, -100, 100);
            recompute_globals();
            snprintf(status_line, sizeof(status_line), "Contrast %+d", contrast);
            return;

        case SDLK_T:
            tint_index = (tint_index + 1) % NUM_TINTS;
            if (tint_index != 0 && tint_amount == 0) tint_amount = 64;
            recompute_globals();
            snprintf(status_line, sizeof(status_line),
                     "Tint: %s (amount %d)",
                     g_tints[tint_index].label, tint_amount);
            return;

        case SDLK_Y:
            tint_amount = clampi(tint_amount - 8, 0, 255);
            recompute_globals();
            snprintf(status_line, sizeof(status_line),
                     "Tint: %s (amount %d)",
                     g_tints[tint_index].label, tint_amount);
            return;

        case SDLK_U:
            tint_amount = clampi(tint_amount + 8, 0, 255);
            recompute_globals();
            snprintf(status_line, sizeof(status_line),
                     "Tint: %s (amount %d)",
                     g_tints[tint_index].label, tint_amount);
            return;

        case SDLK_S:
            if (kstate & KS_CTRL) {
                save_palette_file("custom.conf");
                return;
            }
            handled = 0;
            break;

        default:
            handled = 0;
            break;
    }

    if (channel_edit >= 1 && channel_edit <= 3 && key >= SDLK_0 && key <= SDLK_9) {
        int digit = (int)(key - SDLK_0);
        if (digit_channel != channel_edit) {
            digit_accum = -1;
            digit_channel = channel_edit;
        }
        if (digit_accum < 0) digit_accum = 0;
        digit_accum = digit_accum * 10 + digit;
        if (digit_accum > 255) digit_accum = digit;
        apply_channel_set(digit_accum);
        return;
    }

    if (!handled) reset_accum();
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

extern char *printNote(char *str, event *r, int cur_edit_mode);
extern void draw_bar(int x, int y, int xsize, int ysize, int value, int maxval, TColor bg, Drawable *S);

// 1-pixel rectangle outline drawn with fillRect (Drawable::drawVLine is a
// no-op).
static void draw_outline(Drawable *S, int x0, int y0, int x1, int y1, TColor c) {
    S->fillRect(x0, y0, x1 - 1, y0, c);
    S->fillRect(x0, y1 - 1, x1 - 1, y1 - 1, c);
    S->fillRect(x0, y0, x0, y1 - 1, c);
    S->fillRect(x1 - 1, y0, x1 - 1, y1 - 1, c);
}

// Print at most `width` characters (text that does not fit is cut, never
// drawn past the column it was given).
static void print_cut(int cx, int cy, const char *s, int width, TColor fg, Drawable *S) {
    if (width <= 0) return;
    char buf[256];
    snprintf(buf, sizeof(buf), "%.*s", width < 255 ? width : 255, s);
    print(col(cx), row(cy), buf, fg, S);
}

static void draw_slider_bar(Drawable *S, int x0, int y0, int x1, int y1,
                            int value, int max_value, TColor fill,
                            int highlight) {
    S->fillRect(x0, y0, x1 - 1, y1 - 1, COLORS.EditBG);
    int span = x1 - x0 - 2;
    if (span < 1) span = 1;
    int filled = (value * span) / (max_value > 0 ? max_value : 1);
    if (filled < 0) filled = 0;
    if (filled > span) filled = span;
    if (filled > 0) {
        S->fillRect(x0 + 1, y0 + 1, x0 + filled, y1 - 2, fill);
    }
    draw_outline(S, x0, y0, x1, y1, highlight ? COLORS.Highlight : COLORS.Lowlight);
}

// Fake pattern editor showing every pattern colour in its real role:
// track headers, LCD bars, EditBG / EditBGlow / EditBGhigh beat rows, the
// cursor row (CursorRowLow / CursorRowHigh), a selection (SelectedBGLow /
// SelectedBGHigh, Highlight text) and the caret.
static void draw_pattern_preview(Drawable *S, const PeLayout &L, int selected_slot) {
    char str[64];
    event ev;
    int fs = (int)strlen(printNote(str, &ev, VIEW_REGULAR));
    int mode = VIEW_REGULAR;
    if (L.mid_w < 4 + fs + 1) {
        mode = VIEW_SQUISH;
        fs = (int)strlen(printNote(str, &ev, VIEW_SQUISH));
    }
    int tracks = (L.mid_w - 4) / (fs + 1);
    if (tracks < 1) return;
    if (tracks > 8) tracks = 8;

    const int lo = zt_config_globals.lowlight_increment > 0 ? zt_config_globals.lowlight_increment : 4;
    const int hi = zt_config_globals.highlight_increment > 0 ? zt_config_globals.highlight_increment : 16;
    auto beat = [&](int r) { return (r % lo) == 0 || (r % hi) == 0; };

    const int x_rows = L.mid_x;          // row numbers
    const int x_trk = L.mid_x + 4;       // first track
    const int y_rows = L.prev_y + 2;     // first pattern row
    const int nrows = L.bottom - y_rows + 1;
    if (nrows < 4) return;

    // Cursor row: an off-beat row, or a beat row while CursorRowHigh is
    // the colour being edited, so both cursor colours can be seen.
    const bool want_beat = (selected_slot == 14);
    int cur_r = 1;
    for (int r = 1; r < nrows; ++r) {
        if (beat(r) == want_beat) { cur_r = r; break; }
    }
    // Selection: last track, three rows around the first beat row after
    // the cursor (one beat row, SelectedBGHigh; off-beat ones, SelectedBGLow).
    int sel_mid = cur_r + 2;
    while (sel_mid < nrows - 1 && !beat(sel_mid)) sel_mid++;
    const int sel_t = tracks - 1;
    const int sel_r0 = sel_mid - 1, sel_r1 = sel_mid + 1;

    // Track headers and LCD bars.
    for (int t = 0; t < tracks; ++t) {
        int x = x_trk + t * (fs + 1);
        char hdr[64];
        snprintf(hdr, sizeof(hdr), " Track %.2d", t + 1);
        for (int k = (int)strlen(hdr); k < fs && k < 63; ++k) hdr[k] = ' ';
        hdr[fs < 63 ? fs : 63] = 0;
        printBG(col(x), row(L.prev_y), hdr, COLORS.Brighttext, COLORS.Lowlight, S);
        static const int levels[8] = { 80, 55, 95, 40, 70, 60, 85, 50 };
        draw_bar(col(x), row(L.prev_y + 1) + 1, col(fs), row(1) - 2,
                 levels[t], 100, COLORS.Black, S);
    }

    for (int r = 0; r < nrows; ++r) {
        int y = row(y_rows + r);
        bool is_cur = (r == cur_r);
        snprintf(str, sizeof(str), "%.3d", r);
        printBG(col(x_rows), y, str, is_cur ? COLORS.Highlight : COLORS.Text, COLORS.Background, S);
        printchar(col(x_rows + 3), y, 146, COLORS.Lowlight, S);

        for (int t = 0; t < tracks; ++t) {
            TColor bg = COLORS.EditBG;
            if (!(r % lo)) bg = COLORS.EditBGlow;
            if (!(r % hi)) bg = COLORS.EditBGhigh;
            if (is_cur) bg = beat(r) ? COLORS.CursorRowHigh : COLORS.CursorRowLow;
            TColor fg = COLORS.EditText;
            if (t == sel_t && r >= sel_r0 && r <= sel_r1) {
                fg = COLORS.Highlight;
                if (bg == COLORS.EditBGlow || bg == COLORS.EditBGhigh) bg = COLORS.SelectedBGHigh;
                else bg = is_cur ? COLORS.SelectedBGHigh : COLORS.SelectedBGLow;
            }

            event e;
            if (((r + t * 2) % 4) == 0) {
                static const unsigned char notes[8] = { 48, 52, 55, 60, 43, 50, 57, 62 };
                e.note = notes[(r / 4 + t) % 8];
                e.inst = (unsigned char)(t + 1);
                e.vol = (unsigned char)(0x40 + ((r * 7 + t * 13) % 0x3F));
            }
            printNote(str, &e, mode);
            str[fs] = 0;
            int x = x_trk + t * (fs + 1);
            printBG(col(x), y, str, fg, bg, S);
            if (is_cur && t == 0) {
                char caret[4] = { str[0], str[1], str[2], 0 };
                printBG(col(x), y, caret, COLORS.EditBG, COLORS.Highlight, S);
            }
            if (t < tracks - 1) printchar(col(x + fs), y, 168, COLORS.Lowlight, S);
            else                printchar(col(x + fs), y, 145, COLORS.Highlight, S);
        }
    }
}

void CUI_PaletteEditor::draw(Drawable *S) {
    if (S->lock() != 0) return;

    const PeLayout L = pe_layout();
    clamp_preset_view();

    UI->draw(S);
    // Clear the page area (title row to just above the toolbar): values
    // and labels are printed without a background.
    S->fillRect(0, row(PAGE_TITLE_ROW_Y + 1), col(L.cols) - 1, row(L.hint_y + 3) - 1,
                COLORS.Background);
    draw_status(S);

    printtitle(PAGE_TITLE_ROW_Y,
               "Palette Editor (Shift+Ctrl+F12)",
               COLORS.Text, COLORS.Background, S);

    // --- Colour list ------------------------------------------------------
    print_cut(L.list_x, L.top, "Colours", PE_LIST_W, COLORS.Text, S);
    for (int i = 0; i < NUM_PALETTE_SLOTS; ++i) {
        int x0, y0, x1, y1;
        list_item_rect(L, i, &x0, &y0, &x1, &y1);
        TColor c = *slot_color_ptr(i);
        S->fillRect(x0, y0 + 1, x0 + col(2) - 1, y1 - 2, c);
        draw_outline(S, x0, y0 + 1, x0 + col(2), y1 - 1, COLORS.Lowlight);

        char name[32];
        snprintf(name, sizeof(name), " %-14.14s", g_slots[i].name);
        bool sel = (i == selected_slot);
        TColor fg = sel ? COLORS.Highlight : COLORS.Text;
        TColor bg = (sel && focus_panel == 0 && !channel_edit) ? COLORS.SelectedBGHigh : COLORS.Background;
        printBG(x0 + col(2), y0, name, fg, bg, S);
    }

    // --- Selected colour ----------------------------------------------------
    {
        TColor c = *slot_color_ptr(selected_slot);
        unsigned char r, g, b;
        unpack_rgb(c, &r, &g, &b);
        char head[64];
        snprintf(head, sizeof(head), "%s  #%02X%02X%02X", g_slots[selected_slot].name, r, g, b);
        print_cut(L.mid_x, L.top, head, L.mid_w, COLORS.Brighttext, S);
        if (dirty && L.mid_w >= (int)strlen(head) + 12)
            print(col(L.mid_x + L.mid_w - 10), row(L.top), "[modified]", COLORS.LCDHigh, S);

        int sx0 = col(L.mid_x), sy0 = row(L.top + 2);
        int sx1 = col(L.mid_x + PE_SWATCH_W), sy1 = row(L.top + 2 + PE_SWATCH_H);
        S->fillRect(sx0, sy0, sx1 - 1, sy1 - 1, c);
        draw_outline(S, sx0, sy0, sx1, sy1, COLORS.Lowlight);

        static const TColor chan_fill[3] = { 0xFFE04040, 0xFF40C040, 0xFF4080E0 };
        static const char *chan_label[3] = { "R", "G", "B" };
        int values[3] = { (int)r, (int)g, (int)b };
        for (int ch = 1; ch <= 3; ++ch) {
            int x0, y0, x1, y1;
            rgb_slider_rect(L, ch, &x0, &y0, &x1, &y1);
            int hl = (focus_panel == 0 && channel_edit == ch);
            TColor lc = hl ? COLORS.Highlight : COLORS.Text;
            print(x0 - col(2), y0, chan_label[ch - 1], lc, S);
            draw_slider_bar(S, x0, y0, x1, y1, values[ch - 1], 255, chan_fill[ch - 1], hl);
            char buf[8];
            snprintf(buf, sizeof(buf), "%3d", values[ch - 1]);
            print(x1 + col(1), y0, buf, lc, S);
        }
    }

    draw_pattern_preview(S, L, selected_slot);

    // --- Presets --------------------------------------------------------------
    {
        int px = col(L.pre_x), pw = col(L.pre_w);
        int py = row(L.top), ph = row(L.pre_y - L.top + L.pre_rows);
        S->fillRect(px - 2, py - 2, px + pw + 1, py + ph + 1, COLORS.Black);
        draw_outline(S, px - 2, py - 2, px + pw + 2, py + ph + 2, COLORS.Lowlight);
        char head[40];
        bool more_up = preset_top > 0;
        bool more_dn = preset_top + L.pre_rows < num_presets;
        snprintf(head, sizeof(head), " %-18s%s ", "Palettes & Skins",
                 more_up && more_dn ? "^v" : more_up ? " ^" : more_dn ? " v" : "  ");
        printBG(px, py, head, COLORS.Brighttext, COLORS.Black, S);
        for (int i = 0; i < L.pre_rows && preset_top + i < num_presets; ++i) {
            int p = preset_top + i;
            bool sel = (p == preset_sel);
            TColor fg = sel ? COLORS.Highlight : COLORS.EditText;
            TColor bg = (sel && focus_panel == 1) ? COLORS.SelectedBGHigh : COLORS.Black;
            char line[64];
            snprintf(line, sizeof(line), " %-20.20s ", preset_label[p]);
            printBG(px, row(L.pre_y + i), line, fg, bg, S);
        }
    }

    // --- Global adjustments -----------------------------------------------------
    {
        struct { const char *label; int value, min_v, max_v; TColor fill; const char *fmt; } g[3] = {
            { "Bright",   brightness,  -128, 127, 0xFFE0E0E0, "%+4d" },
            { "Contrast", contrast,    -100, 100, 0xFFD0D080, "%+4d" },
            { "Tint",     tint_amount,    0, 255,
              tint_index == 0 ? COLORS.Lowlight : g_tints[tint_index].color, "%4d" },
        };
        for (int w = 0; w < 3; ++w) {
            int x0, y0, x1, y1;
            global_slider_rect(L, w, &x0, &y0, &x1, &y1);
            print(col(L.pre_x), y0, g[w].label, COLORS.Text, S);
            draw_slider_bar(S, x0, y0, x1, y1, g[w].value - g[w].min_v,
                            g[w].max_v - g[w].min_v, g[w].fill, 0);
            char buf[16];
            snprintf(buf, sizeof(buf), g[w].fmt, g[w].value);
            print(x1 + 2, y0, buf, COLORS.Text, S);
        }
        char line[40];
        snprintf(line, sizeof(line), "Hue      %s", g_tints[tint_index].label);
        print_cut(L.pre_x, L.glob_y + 3, line, L.pre_w, COLORS.Text, S);
        snprintf(line, sizeof(line), "[%c] %s", reset_skin_on_palette ? 'X' : ' ', PE_CHECKBOX_LABEL);
        print_cut(L.pre_x, L.glob_y + 4, line, L.pre_w,
                  reset_skin_on_palette ? COLORS.Highlight : COLORS.Text, S);
    }

    // --- Hints and status ---------------------------------------------------------
    const char *hint;
    if (focus_panel == 1)
        hint = "Up/Dn PgUp/PgDn: preset  Enter/click: load  Left/Tab: colours";
    else if (channel_edit)
        hint = "Up/Dn: R/G/B  Left/Right or +/-: step (Shift x16)  0-9: value  Tab: presets";
    else
        hint = "Up/Dn: colour  Right/Enter or R/G/B: edit  Tab: next panel  Drag bars";
    const int w = L.cols - 2;
    print_cut(1, L.hint_y, hint, w, COLORS.Text, S);
    print_cut(1, L.hint_y + 1,
              "[ ] bright  ; ' contrast  T hue  Y/U tint  Ctrl+S save  ESC revert",
              w, COLORS.Text, S);
    if (status_line[0]) print_cut(1, L.hint_y + 2, status_line, w, COLORS.Brighttext, S);

    need_refresh = 0;
    updated = 2;
    S->unlock();
}
