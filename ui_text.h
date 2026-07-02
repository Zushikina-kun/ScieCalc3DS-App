#ifndef INC_UI_TEXT_H
#define INC_UI_TEXT_H

#include <citro2d.h>
#include <string_view>

// Reuses TextMap::char_to_sprite->equ (built in text.cpp / TextMap::generate),
// which already has sprites for '0'-'9', 'a'-'z', '.', '-', '+', '*', 'P' (pi), '>'.
//
// NOTE: only lowercase letters exist in the atlas today. Uppercase input is
// folded to lowercase. Characters with no sprite (space, punctuation not
// listed above) are skipped and simply advance the cursor by a fixed width,
// so unsupported characters can't crash or corrupt layout - they just render
// as a gap. This keeps things safe for any string you build at runtime
// (e.g. formatted floats from snprintf) without needing to sanitize first.
//
// This intentionally does NOT touch text.cpp/text.h - it is additive so it
// cannot regress the existing equation/memory rendering.
namespace UiText {

// Returns total pixel width the string would take, without drawing.
// Useful for right-aligning or centering labels (e.g. axis numbers).
int measure(std::string_view str, float scale = 1.0f);

// Draws left-aligned starting at (x, y). depth is the citro2d Z value
// (same convention as the rest of the app, e.g. 0.5f for UI chrome).
void draw(C2D_SpriteSheet sprites, std::string_view str, int x, int y,
          float depth, u32 color = 0xFF000000, float scale = 1.0f);

// Convenience: draws right-aligned so the string ends at x.
void draw_right_aligned(C2D_SpriteSheet sprites, std::string_view str, int x, int y,
                         float depth, u32 color = 0xFF000000, float scale = 1.0f);

// Convenience: formats a double the same way Number::render does (rounded,
// %.5g-ish) and draws it. Used for graph axis tick labels.
void draw_number(C2D_SpriteSheet sprites, double value, int x, int y,
                  float depth, u32 color = 0xFF000000, float scale = 1.0f);

}

#endif
