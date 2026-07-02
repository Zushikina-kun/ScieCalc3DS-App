#include "ui_text.h"
#include "text.h"
#include <cstdio>
#include <cctype>
#include <cmath>
#include <array>

namespace {
    constexpr int FALLBACK_ADVANCE = 8; // px, for glyphs we don't have (e.g. space)

    // Looks up a single ASCII char in the existing glyph atlas. Returns
    // nullptr if there is no sprite for it (caller should just advance).
    const C2D_Image* find_glyph(char c)
    {
        if(!TextMap::char_to_sprite) return nullptr; // atlas not built yet - be safe

        char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        char key[2] = { lc, '\0' };

        const auto& equ = TextMap::char_to_sprite->equ;
        auto it = equ.find(std::string_view(key, 1));
        if(it != equ.end()) return &it->second;
        return nullptr;
    }
}

namespace UiText {

int measure(std::string_view str, float scale)
{
    int w = 0;
    for(const char c : str)
    {
        const C2D_Image* img = find_glyph(c);
        const int gw = img ? img->subtex->width : FALLBACK_ADVANCE;
        w += static_cast<int>(gw * scale) + 1;
    }
    return w;
}

void draw(C2D_SpriteSheet sprites, std::string_view str, int x, int y,
          float depth, u32 color, float scale)
{
    (void)sprites; // glyphs come from the already-loaded atlas, kept for API symmetry

    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, color, 1.0f);

    int cx = x;
    for(const char c : str)
    {
        const C2D_Image* img = find_glyph(c);
        if(img)
        {
            C2D_DrawImageAt(*img, static_cast<float>(cx), static_cast<float>(y), depth, &tint, scale, scale);
            cx += static_cast<int>(img->subtex->width * scale) + 1;
        }
        else
        {
            cx += FALLBACK_ADVANCE; // e.g. spaces between words
        }
    }
}

void draw_right_aligned(C2D_SpriteSheet sprites, std::string_view str, int x, int y,
                         float depth, u32 color, float scale)
{
    const int w = measure(str, scale);
    draw(sprites, str, x - w, y, depth, color, scale);
}

void draw_number(C2D_SpriteSheet sprites, double value, int x, int y,
                  float depth, u32 color, float scale)
{
    if(!std::isfinite(value))
    {
        draw(sprites, "err", x, y, depth, color, scale);
        return;
    }

    char buf[32];
    const double rounded = std::round(value * 1000.0) / 1000.0;
    std::snprintf(buf, sizeof(buf), "%.4g", rounded + 0.0);
    draw(sprites, buf, x, y, depth, color, scale);
}

}
