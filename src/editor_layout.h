// editor_layout.h
//
// Shared character-grid layout constants for the F4 (MIDI Macro) and
// Shift+F4 (Arpeggio) editors. Labels in both pages are right-aligned
// so the last character sits at the same column. Textfields and sliders
// then start ONE column after that, giving a single empty-character
// spacer between the label and the input.
//
// All editor cpps must use these constants, NOT hardcoded numbers --
// otherwise the two pages drift out of alignment as one moves.

#ifndef ZT_EDITOR_LAYOUT_H
#define ZT_EDITOR_LAYOUT_H

// Column just past the LAST character of every left-side label. The
// longest labels start at col 1 (one blank column of screen margin):
// "Slot"   prints at col 3  -> chars 3..6     -> right edge col 7.
// "Name"   prints at col 3  -> chars 3..6     -> right edge col 7.
// "Length" prints at col 1  -> chars 1..6     -> right edge col 7.
// "Speed"  prints at col 2  -> chars 2..6     -> right edge col 7.
// "Repeat" prints at col 1  -> chars 1..6     -> right edge col 7.
// "NumCC"  prints at col 2  -> chars 2..6     -> right edge col 7.
// "CC#"    prints at col 4  -> chars 4..6     -> right edge col 7.
#define ZT_EDITOR_LABEL_RIGHT_COL  7

// Preset list on the right of both editors, just past the 32-col Name
// field (frame at col 40); it runs to one column before the screen edge.
#define ZT_EDITOR_PRESET_X         43

// Where every textfield / slider / data-grid starts. One column after
// the label's right edge, so the visible gap is exactly one space.
#define ZT_EDITOR_INPUT_X          (ZT_EDITOR_LABEL_RIGHT_COL + 1)

#endif // ZT_EDITOR_LAYOUT_H
