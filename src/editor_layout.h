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

// Column where the LAST character of every left-side label sits.
// "Slot"   prints at col 6  -> chars 6..9     -> right edge col 10.
// "Name"   prints at col 6  -> chars 6..9     -> right edge col 10.
// "Length" prints at col 4  -> chars 4..9     -> right edge col 10.
// "Speed"  prints at col 5  -> chars 5..9     -> right edge col 10.
// "Repeat" prints at col 4  -> chars 4..9     -> right edge col 10.
// "NumCC"  prints at col 5  -> chars 5..9     -> right edge col 10.
// "CC#"    prints at col 7  -> chars 7..9     -> right edge col 10.
#define ZT_EDITOR_LABEL_RIGHT_COL  10

// Where every textfield / slider / data-grid starts. One column after
// the label's right edge, so the visible gap is exactly one space.
#define ZT_EDITOR_INPUT_X          (ZT_EDITOR_LABEL_RIGHT_COL + 1)

#endif // ZT_EDITOR_LAYOUT_H
