#include "zt.h"
#include "CUI_PaletteEditor.h"
#include "skins.h"
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
// Navigation:
//   Arrow keys           - move within current panel (grid or presets)
//   Tab / Shift+Tab      - switch focus between swatch grid and preset panel
//   Mouse                - left-click any swatch, RGB slider, preset, or
//                          global slider; drag a slider to scrub
//   R / G / B            - keyboard-select a channel of the focused swatch
//   + / -                - increase/decrease selected channel by 1
//                          (Shift for x16)
//   0..9                 - replace selected channel with that decimal digit
//   Enter                - on a preset, load that preset
//   [ / ]                - global brightness -/+ 8
//   ; / '                - global contrast   -/+ 4
//   T                    - cycle global tint hue (none, purple, red, blue,
//                          green, amber, cyan, magenta)
//   Y / U                - tint amount -/+ 8
//   Ctrl+S               - save current palette to palettes/custom.conf
//   ESC                  - revert to snapshot taken on entry and return
//                          to pattern editor
// ---------------------------------------------------------------------------

// Layout constants (character cells are 8x8 px).
// Each slot: 1 row label + PE_SWATCH_H rows swatch + 3 rows R/G/B sliders.
// Step of 8 leaves a 1-row gap before the next row's label.
#define PE_GRID_COL_X       6
#define PE_GRID_COL_STEP    23
#define PE_GRID_ROW_Y       12
#define PE_GRID_ROW_STEP    8
#define PE_GRID_COLS        3
#define PE_GRID_ROWS        ((NUM_PALETTE_SLOTS + PE_GRID_COLS - 1) / PE_GRID_COLS)

#define PE_SWATCH_W         5
#define PE_SWATCH_H         3

#define PE_SLIDER_W_CHARS   10   // RGB slider width in char cells (80 px)

#define PE_PRESET_X         76
#define PE_PRESET_Y         12
#define PE_PRESET_W         22
#define PE_PRESET_VISIBLE   24   // visible preset rows (1-row pitch)

// Global controls strip — single horizontal row above the swatch grid,
// well clear of the toolbar at the bottom of the screen.
#define PE_GLOBAL_ROW_Y     10   // char-row directly under the page title
#define PE_GLOBAL_X         2
#define PE_GLOBAL_BAR_W     14   // chars per slider
#define PE_GLOBAL_LABEL_W   12   // label cells before each bar
#define PE_GLOBAL_NUMBER_W  4    // value readout after each bar
#define PE_GLOBAL_GROUP_W   (PE_GLOBAL_LABEL_W + PE_GLOBAL_BAR_W + PE_GLOBAL_NUMBER_W + 2)

// Checkbox: "[X] Reset skin to default on palette load" — shown on row 11.
#define PE_CHECKBOX_X       2
#define PE_CHECKBOX_Y       11
#define PE_CHECKBOX_LABEL   "Reset skin to default on palette load"
#define PE_CHECKBOX_W       (3 + 1 + (int)sizeof(PE_CHECKBOX_LABEL))

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
    { "Light Blue",        "light_blue.conf" },
    { "Gold",              "gold.conf" },
    { "Camouflage",        "camouflage.conf" },
    { "Midnight Tracking", "midnight_tracking.conf" },
    { "Pine Colours",      "pine_colours.conf" },
    { "Soundtracker",      "soundtracker.conf" },
    { "Volcanic",          "volcanic.conf" },
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

    // 1. Static palette presets (palettes/*.conf next to the binary).
    for (int i = 0; i < NUM_STATIC_PRESETS && num_presets < 64; ++i) {
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
    }
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
    // authored colors. Just force a redraw so the new toolbar paints.
    if (screen_buffer) redrawscreen(screen_buffer);
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
// stays in sync with COLORS.* no matter which path edited it (per-slot RGB
// slider drag, R/G/B + 0-9 keystroke entry, global brightness/contrast/tint,
// preset load, or ESC revert).
//
// Crucially, this calls redrawscreen() inline so the PNG recolor, the
// page-background fill, and the toolbar PNG re-blit all land in the same
// frame as the input event. Without the inline redraw the user would see
// the PNG flash recolored on frame N and the page background catch up on
// frame N+1 — a visible 2-step transition for what should be one update.
static void palette_changed(void) {
    if (CurrentSkin) {
        CurrentSkin->recolor_to_palette(COLORS.Lowlight,
                                        COLORS.Background,
                                        COLORS.Highlight);
        make_toolbar();
    }
    if (screen_buffer) {
        redrawscreen(screen_buffer);
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

// Geometry helpers used by both draw() and update() so click hit-tests stay
// in sync with what the user sees.

static void slot_origin(int slot, int *x0, int *y0, int *x1, int *y1) {
    int gr = slot / PE_GRID_COLS;
    int gc = slot % PE_GRID_COLS;
    int cx = PE_GRID_COL_X + gc * PE_GRID_COL_STEP;
    int cy = PE_GRID_ROW_Y + gr * PE_GRID_ROW_STEP;
    *x0 = col(cx);
    *y0 = row(cy);
    *x1 = col(cx + PE_SWATCH_W);
    *y1 = row(cy + PE_SWATCH_H);
}

static void rgb_slider_rect(int slot, int channel, int *x0, int *y0, int *x1, int *y1) {
    int sx0, sy0, sx1, sy1;
    slot_origin(slot, &sx0, &sy0, &sx1, &sy1);
    int rgb_y = sy1 + 1;
    *x0 = sx0 + col(2);                          // 2 chars left for "R"/"G"/"B"
    *x1 = *x0 + col(PE_SLIDER_W_CHARS);
    *y0 = rgb_y + (channel - 1) * row(1);
    *y1 = *y0 + row(1) - 2;
}

static void global_slider_rect(int which, int *x0, int *y0, int *x1, int *y1) {
    // which: 0 = brightness, 1 = contrast, 2 = tint amount
    // Three groups laid out horizontally on PE_GLOBAL_ROW_Y.
    int group_x = PE_GLOBAL_X + which * PE_GLOBAL_GROUP_W;
    *y0 = row(PE_GLOBAL_ROW_Y);
    *y1 = *y0 + row(1) - 2;
    *x0 = col(group_x + PE_GLOBAL_LABEL_W);
    *x1 = *x0 + col(PE_GLOBAL_BAR_W);
}

static int preset_row_rect(int idx, int top_y, int *x0, int *y0, int *x1, int *y1) {
    int ppx = col(PE_PRESET_X);
    int ppw = col(PE_PRESET_W);
    *x0 = ppx;
    *x1 = ppx + ppw;
    *y0 = top_y + row(2 + idx);                  // 1-row pitch (denser list)
    *y1 = *y0 + row(1);
    return 1;
}

void CUI_PaletteEditor::update(void) {
    static int digit_accum = -1;
    static int digit_channel = 0;
    // Mouse-drag tracking for slider scrubbing while button is held.
    // 0 = no drag, 1..3 = swatch RGB channel, 10/11/12 = global brightness/
    // contrast/tint amount.
    static int drag_kind = 0;
    static int drag_slot = 0;

    UI->update();
    if (!Keys.size()) {
        // Continue scrubbing while button is held even without new key events.
        if (drag_kind && bMouseIsDown) {
            int x = LastX;
            if (drag_kind >= 1 && drag_kind <= 3) {
                int rx0, ry0, rx1, ry1;
                rgb_slider_rect(drag_slot, drag_kind, &rx0, &ry0, &rx1, &ry1);
                int v = clampi((x - rx0) * 255 / (rx1 - rx0 - 1), 0, 255);
                channel_edit = drag_kind;
                int saved = selected_slot;
                selected_slot = drag_slot;
                apply_channel_set(v);
                selected_slot = saved;
            } else if (drag_kind == 10 || drag_kind == 11 || drag_kind == 12) {
                int gx0, gy0, gx1, gy1;
                global_slider_rect(drag_kind - 10, &gx0, &gy0, &gx1, &gy1);
                int frac = clampi((x - gx0) * 1000 / (gx1 - gx0 - 1), 0, 1000);
                if (drag_kind == 10)      brightness = -128 + (frac * 256) / 1000;
                else if (drag_kind == 11) contrast   = -100 + (frac * 200) / 1000;
                else {
                    tint_amount = (frac * 255) / 1000;
                    // If the user is scrubbing the tint amount but no hue is
                    // selected, the slider would silently do nothing. Bump
                    // to the first real hue (Purple) so dragging visibly
                    // tints the palette.
                    if (tint_amount > 0 && tint_index == 0) tint_index = 1;
                }
                recompute_globals();
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

    if (key == ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_UP << 8) | SDL_BUTTON_LEFT))) {
        drag_kind = 0;
        return;
    }

    if (key == ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_LEFT))) {
        int mx = MousePressX;
        int my = MousePressY;

        // 1. Preset list (1-row pitch list down the right column).
        int ppx = col(PE_PRESET_X);
        int ppy = row(PE_PRESET_Y);
        int ppw = col(PE_PRESET_W);
        int visible = num_presets < PE_PRESET_VISIBLE ? num_presets : PE_PRESET_VISIBLE;
        for (int i = 0; i < visible; ++i) {
            int ly = ppy + row(2 + i);
            if (mx >= ppx && mx < ppx + ppw && my >= ly && my < ly + row(1)) {
                focus_panel = 1;
                selected_slot = i;
                channel_edit = 0;
                if (preset_is_skin[i]) {
                    load_skin_full(preset_path[i]);
                } else {
                    load_palette_file(preset_path[i]);
                }
                reset_accum();
                need_refresh++;
                return;
            }
        }

        // 2. Per-slot RGB sliders.
        for (int slot = 0; slot < NUM_PALETTE_SLOTS; ++slot) {
            for (int ch = 1; ch <= 3; ++ch) {
                int rx0, ry0, rx1, ry1;
                rgb_slider_rect(slot, ch, &rx0, &ry0, &rx1, &ry1);
                if (mx >= rx0 && mx < rx1 && my >= ry0 && my < ry1) {
                    focus_panel = 0;
                    selected_slot = slot;
                    channel_edit = ch;
                    int v = clampi((mx - rx0) * 255 / (rx1 - rx0 - 1), 0, 255);
                    apply_channel_set(v);
                    drag_kind = ch;
                    drag_slot = slot;
                    reset_accum();
                    need_refresh++;
                    return;
                }
            }
        }

        // 3. Swatch hit-test (selects without scrubbing).
        for (int i = 0; i < NUM_PALETTE_SLOTS; ++i) {
            int x0, y0, x1, y1;
            slot_origin(i, &x0, &y0, &x1, &y1);
            if (mx >= x0 && mx < x1 && my >= y0 && my < y1) {
                focus_panel = 0;
                selected_slot = i;
                channel_edit = 0;
                reset_accum();
                snprintf(status_line, sizeof(status_line),
                         "Selected %s — drag the R/G/B bars or press R/G/B then 0-9",
                         g_slots[i].name);
                need_refresh++;
                return;
            }
        }

        // 3b. "Reset skin to default on palette load" checkbox.
        {
            int cbx0 = col(PE_CHECKBOX_X);
            int cbx1 = cbx0 + col(PE_CHECKBOX_W);
            int cby0 = row(PE_CHECKBOX_Y);
            int cby1 = cby0 + row(1);
            if (mx >= cbx0 && mx < cbx1 && my >= cby0 && my < cby1) {
                reset_skin_on_palette = !reset_skin_on_palette;
                snprintf(status_line, sizeof(status_line),
                         "Reset skin to default on palette load: %s",
                         reset_skin_on_palette ? "ON" : "OFF");
                doredraw++;
                need_refresh++;
                screenmanager.UpdateAll();
                return;
            }
        }

        // 4. Global brightness / contrast / tint sliders.
        for (int which = 0; which < 3; ++which) {
            int gx0, gy0, gx1, gy1;
            global_slider_rect(which, &gx0, &gy0, &gx1, &gy1);
            if (mx >= gx0 && mx < gx1 && my >= gy0 && my < gy1) {
                int frac = clampi((mx - gx0) * 1000 / (gx1 - gx0 - 1), 0, 1000);
                if (which == 0)      brightness = -128 + (frac * 256) / 1000;
                else if (which == 1) contrast   = -100 + (frac * 200) / 1000;
                else {
                    tint_amount = (frac * 255) / 1000;
                    if (tint_amount > 0 && tint_index == 0) tint_index = 1;
                }
                drag_kind = 10 + which;
                recompute_globals();
                reset_accum();
                return;
            }
        }
        return;
    }

    switch (key) {
        case SDLK_ESCAPE:
            for (int i = 0; i < NUM_PALETTE_SLOTS; ++i) {
                TColor *c = slot_color_ptr(i);
                if (c) *c = snapshot[i];
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
            focus_panel = (focus_panel + 1) % 2;
            channel_edit = 0;
            selected_slot = 0;
            reset_accum();
            need_refresh++;
            return;

        case SDLK_UP:
            reset_accum();
            if (focus_panel == 0) {
                // Walk up through R/G/B sliders within a slot, then jump to
                // the previous row's B slider (and so on). This mirrors what
                // the eye does when scanning the column visually.
                if (channel_edit > 1) {
                    channel_edit--;
                } else if (channel_edit == 1) {
                    channel_edit = 0;
                } else {
                    int rr = selected_slot / PE_GRID_COLS;
                    int cc = selected_slot % PE_GRID_COLS;
                    if (rr > 0) {
                        selected_slot = (rr - 1) * PE_GRID_COLS + cc;
                        channel_edit = 3;   // land on previous row's B
                    }
                }
            } else {
                if (selected_slot > 0) selected_slot--;
            }
            need_refresh++;
            return;

        case SDLK_DOWN:
            reset_accum();
            if (focus_panel == 0) {
                if (channel_edit == 0) {
                    channel_edit = 1;       // swatch -> R
                } else if (channel_edit < 3) {
                    channel_edit++;         // R -> G -> B
                } else {
                    int rr = selected_slot / PE_GRID_COLS;
                    int cc = selected_slot % PE_GRID_COLS;
                    int next = (rr + 1) * PE_GRID_COLS + cc;
                    if (next < NUM_PALETTE_SLOTS) {
                        selected_slot = next;
                        channel_edit = 0;   // land on the next swatch
                    }
                }
            } else {
                if (selected_slot < num_presets - 1) selected_slot++;
            }
            need_refresh++;
            return;

        case SDLK_LEFT:
            reset_accum();
            if (focus_panel == 0) {
                int cc = selected_slot % PE_GRID_COLS;
                if (cc > 0) selected_slot--;
            } else {
                focus_panel = 0;
                if (selected_slot >= NUM_PALETTE_SLOTS) selected_slot = NUM_PALETTE_SLOTS - 1;
            }
            channel_edit = 0;
            need_refresh++;
            return;

        case SDLK_RIGHT:
            reset_accum();
            if (focus_panel == 0) {
                int cc = selected_slot % PE_GRID_COLS;
                if (cc < PE_GRID_COLS - 1 && (selected_slot + 1) < NUM_PALETTE_SLOTS) {
                    selected_slot++;
                } else {
                    focus_panel = 1;
                    selected_slot = 0;
                }
            }
            channel_edit = 0;
            need_refresh++;
            return;

        case SDLK_RETURN:
            reset_accum();
            if (focus_panel == 1 && selected_slot >= 0 && selected_slot < num_presets) {
                if (preset_is_skin[selected_slot]) {
                    load_skin_full(preset_path[selected_slot]);
                } else {
                    load_palette_file(preset_path[selected_slot]);
                }
            }
            return;

        case SDLK_R:
            if (focus_panel == 0) {
                channel_edit = 1;
                reset_accum();
                snprintf(status_line, sizeof(status_line),
                         "Editing R channel of %s", g_slots[selected_slot].name);
                need_refresh++;
            }
            return;

        case SDLK_G:
            if (focus_panel == 0) {
                channel_edit = 2;
                reset_accum();
                snprintf(status_line, sizeof(status_line),
                         "Editing G channel of %s", g_slots[selected_slot].name);
                need_refresh++;
            }
            return;

        case SDLK_B:
            if (focus_panel == 0) {
                channel_edit = 3;
                reset_accum();
                snprintf(status_line, sizeof(status_line),
                         "Editing B channel of %s", g_slots[selected_slot].name);
                need_refresh++;
            }
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
                need_refresh++;
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

static void draw_slider_bar(Drawable *S, int x0, int y0, int x1, int y1,
                            int value, int max_value, TColor fill,
                            int highlight) {
    TColor border = highlight ? COLORS.Highlight : COLORS.Lowlight;
    S->fillRect(x0, y0, x1, y1, COLORS.EditBG);
    int span = x1 - x0 - 2;
    if (span < 1) span = 1;
    int filled = (value * span) / (max_value > 0 ? max_value : 1);
    if (filled < 0) filled = 0;
    if (filled > span) filled = span;
    if (filled > 0) {
        S->fillRect(x0 + 1, y0 + 1, x0 + 1 + filled, y1 - 1, fill);
    }
    S->drawHLine(y0,     x0, x1, border);
    S->drawHLine(y1 - 1, x0, x1, border);
    S->drawVLine(x0,     y0, y1, border);
    S->drawVLine(x1 - 1, y0, y1, border);
}

static void draw_swatch(Drawable *S, int slot, int selected, int channel_edit) {
    int x0, y0, x1, y1;
    slot_origin(slot, &x0, &y0, &x1, &y1);

    TColor c = *slot_color_ptr(slot);
    unsigned char r, g, b;
    unpack_rgb(c, &r, &g, &b);

    S->fillRect(x0, y0, x1, y1, c);
    TColor border = selected ? COLORS.Highlight : COLORS.Lowlight;
    S->drawHLine(y0,     x0, x1, border);
    S->drawHLine(y1 - 1, x0, x1, border);
    S->drawVLine(x0,     y0, y1, border);
    S->drawVLine(x1 - 1, y0, y1, border);

    // Pattern-line preview to the right of the swatch.
    int px0 = x1 + col(1);
    int py0 = y0;
    int px1 = px0 + col(PE_SWATCH_W);
    int py1 = py0 + row(PE_SWATCH_H);
    S->fillRect(px0, py0, px1, py1, COLORS.EditBG);
    print(px0 + 2, py0 + 2,              "C-5 01..", c, S);
    print(px0 + 2, py0 + 2 + row(1) + 1, "...v40..", c, S);
    S->drawHLine(py0,     px0, px1, COLORS.Lowlight);
    S->drawHLine(py1 - 1, px0, px1, COLORS.Lowlight);
    S->drawVLine(px0,     py0, py1, COLORS.Lowlight);
    S->drawVLine(px1 - 1, py0, py1, COLORS.Lowlight);

    print(x0, y0 - row(1), g_slots[slot].name, COLORS.Text, S);

    // Three RGB sliders beneath the swatch — clickable.
    static const TColor chan_fill[3] = {
        0xFFE04040,   // R: red
        0xFF40C040,   // G: green
        0xFF4080E0,   // B: blue
    };
    static const char *chan_label[3] = { "R", "G", "B" };
    int values[3] = { (int)r, (int)g, (int)b };
    for (int ch = 1; ch <= 3; ++ch) {
        int rx0, ry0, rx1, ry1;
        rgb_slider_rect(slot, ch, &rx0, &ry0, &rx1, &ry1);
        int hl = (selected && channel_edit == ch);
        TColor lc = hl ? COLORS.Highlight : COLORS.Text;
        print(x0, ry0, chan_label[ch - 1], lc, S);
        draw_slider_bar(S, rx0, ry0, rx1, ry1,
                        values[ch - 1], 255, chan_fill[ch - 1], hl);
        char buf[8];
        snprintf(buf, sizeof(buf), "%3d", values[ch - 1]);
        print(rx1 + col(1) - 4, ry0, buf, lc, S);
    }
}

static void draw_global_slider(Drawable *S, int which, const char *label,
                               int value, int min_v, int max_v,
                               TColor fill) {
    int x0, y0, x1, y1;
    global_slider_rect(which, &x0, &y0, &x1, &y1);
    int label_x = col(PE_GLOBAL_X + which * PE_GLOBAL_GROUP_W);
    print(label_x, y0, label, COLORS.Text, S);
    int range = max_v - min_v;
    int v = value - min_v;
    draw_slider_bar(S, x0, y0, x1, y1, v, range, fill, 0);
    char buf[16];
    snprintf(buf, sizeof(buf), "%+5d", value);
    print(x1 + 2, y0, buf, COLORS.Text, S);
}

void CUI_PaletteEditor::draw(Drawable *S) {
    if (S->lock() != 0) return;

    UI->draw(S);
    draw_status(S);

    printtitle(PAGE_TITLE_ROW_Y,
               "Palette Editor (Shift+Ctrl+F12)",
               COLORS.Text, COLORS.Background, S);

    for (int i = 0; i < NUM_PALETTE_SLOTS; ++i) {
        int sel = (focus_panel == 0 && i == selected_slot);
        draw_swatch(S, i, sel, sel ? channel_edit : 0);
    }

    // Preset panel — dense 1-row list so all skins fit alongside palettes.
    int px = col(PE_PRESET_X);
    int py = row(PE_PRESET_Y);
    int visible = num_presets < PE_PRESET_VISIBLE ? num_presets : PE_PRESET_VISIBLE;
    int panel_height_rows = 2 + visible + 1;
    // IT-style listbox: black panel background under the title, with the
    // header row using the panel chrome color so the list reads as a
    // self-contained widget.
    S->fillRect(px - 2, py - 2,
                px + col(PE_PRESET_W) + 2,
                py + row(panel_height_rows),
                COLORS.Black);
    S->drawHLine(py - 2,                                  px - 2, px + col(PE_PRESET_W) + 2, COLORS.Lowlight);
    S->drawHLine(py + row(panel_height_rows) - 1,         px - 2, px + col(PE_PRESET_W) + 2, COLORS.Lowlight);
    S->drawVLine(px - 2,                                  py - 2, py + row(panel_height_rows), COLORS.Lowlight);
    S->drawVLine(px + col(PE_PRESET_W) + 1,               py - 2, py + row(panel_height_rows), COLORS.Lowlight);

    {
        char head[32];
        snprintf(head, sizeof(head), " %-20s ", "Palettes & Skins");
        printBG(px, py, head, COLORS.Brighttext, COLORS.Black, S);
    }
    for (int i = 0; i < visible; ++i) {
        int sel = (focus_panel == 1 && i == selected_slot);
        TColor fg = sel ? COLORS.Highlight : COLORS.EditText;
        TColor bg = sel ? COLORS.SelectedBGHigh : COLORS.Black;
        char line[64];
        snprintf(line, sizeof(line), " %-20.20s ", preset_label[i]);
        printBG(px, py + row(2 + i), line, fg, bg, S);
    }

    // Global brightness / contrast / tint slider strip — single horizontal
    // row positioned above the swatch grid so it can never collide with the
    // toolbar at the bottom of the screen.
    draw_global_slider(S, 0, "Brightness  ", brightness, -128, 127, 0xFFE0E0E0);
    draw_global_slider(S, 1, "Contrast    ", contrast,   -100, 100, 0xFFD0D080);
    {
        int x0, y0, x1, y1;
        global_slider_rect(2, &x0, &y0, &x1, &y1);
        int label_x = col(PE_GLOBAL_X + 2 * PE_GLOBAL_GROUP_W);
        char lbl[32];
        snprintf(lbl, sizeof(lbl), "Tint %-7s", g_tints[tint_index].label);
        print(label_x, y0, lbl, COLORS.Text, S);
        TColor fill = (tint_index == 0) ? COLORS.Lowlight : g_tints[tint_index].color;
        draw_slider_bar(S, x0, y0, x1, y1, tint_amount, 255, fill, 0);
        char buf[16];
        snprintf(buf, sizeof(buf), "%5d", tint_amount);
        print(x1 + 2, y0, buf, COLORS.Text, S);
    }

    // "Reset skin to default on palette load" checkbox just under the
    // global slider strip. Click anywhere on the row to toggle.
    {
        char box[64];
        snprintf(box, sizeof(box), "[%c] %s",
                 reset_skin_on_palette ? 'X' : ' ',
                 PE_CHECKBOX_LABEL);
        print(col(PE_CHECKBOX_X), row(PE_CHECKBOX_Y), box,
              reset_skin_on_palette ? COLORS.Highlight : COLORS.Text, S);
    }

    // Hint / status line at the bottom of the page area (rows 60–61, well
    // above the toolbar which starts around row 62).
    int hy = row(PE_GRID_ROW_Y + PE_GRID_ROWS * PE_GRID_ROW_STEP);
    const char *hint;
    if (focus_panel == 0) {
        if (channel_edit) {
            hint = "Editing channel: 0-9 or +/- (Shift x16). Up/Down step R/G/B. Click bar to scrub. Tab cancels.";
        } else {
            hint = "Click swatch / RGB bar / preset / global. Up/Down step R/G/B. R/G/B keys. [/] bright ;/' contrast T tint Y/U amount Ctrl+S save ESC revert";
        }
    } else {
        hint = "Click preset / Arrows: move  Enter: load  Tab/Left: back to grid  ESC: revert";
    }
    print(col(2), hy, hint, COLORS.Text, S);
    if (status_line[0]) {
        print(col(2), hy + row(1), status_line, COLORS.Brighttext, S);
    }
    if (dirty) {
        print(col(70), hy, "[modified]", COLORS.LCDHigh, S);
    }

    need_refresh = 0;
    updated = 2;
    S->unlock();
}
