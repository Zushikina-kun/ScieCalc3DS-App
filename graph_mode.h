#ifndef INC_GRAPH_MODE_H
#define INC_GRAPH_MODE_H

#include <citro2d.h>
#include <array>
#include <string>
#include "expr_parser.h"

// Top screen (400x240) = the plot.
// Bottom screen (320x240) = "f(x) = ..." box + a short control hint strip.
//
// Input model (deliberately reuses the same physical controls the rest of
// the app already uses, so nothing new has to be learned):
//   - Touch the f(x) box on the bottom screen -> opens the system software
//     keyboard (swkbd) to type the function, same as any 3DS app.
//   - Circle pad -> pan the view (same input path main.cpp already reads
//     for the standard calculator's circle pad handling).
//   - L / R shoulder -> zoom out / in, centered on the current view.
//   - X -> reset to the default -10..10 view.
//   - Y -> toggle grid lines.
//
// Safety properties (matches what was asked for - "without breaking or
// crashing"):
//   - A parse error in the typed function never crashes; it's shown as text
//     on the plot area and the last valid graph (if any) stays visible.
//   - Sampling never produces NaN/Inf into the draw call - invalid samples
//     just break the line (like a real graphing calculator does at
//     asymptotes), they don't get drawn as a jump across the screen.
//   - All sampling happens once when the function/view changes, not every
//     frame, so it can't cause frame drops or run into the CPU time limit
//     main.cpp sets via APT_SetAppCpuTimeLimit.
class GraphMode {
public:
    explicit GraphMode(C2D_SpriteSheet sprites);

    // Call once per frame, matching the calling convention already used by
    // Keyboard::handle_buttons / handle_circle_pad / handle_touch in
    // keyboard.cpp, so it drops into main.cpp's existing input dispatch.
    void handle_buttons(u32 kDown, u32 kDownRepeat);
    void handle_circle_pad(int dx, int dy);
    void handle_touch(int x, int y);

    // Call every frame regardless of input (mirrors Keyboard::update_equation
    // etc. in main.cpp's loop) - cheap no-op unless something is dirty.
    void update();

    // Forces a full resample + redraw on the next update()/draw_top() pass.
    // Used by the sleep/resume hook (see sleep_hook.h) as a defensive
    // measure after coming back from the HOME menu or system sleep.
    void invalidate() { mark_dirty(); }

    void draw_top(C2D_SpriteSheet sprites) const;
    void draw_bottom(C2D_SpriteSheet sprites) const;

private:
    static constexpr int TOP_W = 400;
    static constexpr int TOP_H = 240;

    void open_function_editor();
    void mark_dirty() { dirty = true; }
    void resample_if_needed();
    void reset_view();

    // pixel <-> math space conversions for the current viewport
    float px_to_x(int px) const;
    float py_to_y(int py) const;
    int   x_to_px(float x) const;
    int   y_to_py(float y) const;

    std::string func_text{"sin(x)"};
    Expr func;

    double xmin = -10.0, xmax = 10.0;
    double ymin = -10.0, ymax = 10.0;

    std::array<float, TOP_W> sample_y{};
    std::array<bool, TOP_W> sample_valid{};
    bool dirty = true;
    bool show_grid = true;

    // f(x)= box hit-test rectangle on the bottom screen
    static constexpr int BOX_X = 8, BOX_Y = 8, BOX_W = 304, BOX_H = 24;
};

#endif
