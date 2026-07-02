#include "graph_mode.h"
#include "colors.h"
#include "ui_text.h"
#include <3ds.h>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace {
    constexpr float CLAMP_PX = 2000.0f; // several screen-heights of headroom -
                                         // enough to draw a curve cleanly
                                         // shooting off-screen at an asymptote,
                                         // without handing the PICA200 GPU an
                                         // arbitrarily large vertex coordinate
}

GraphMode::GraphMode(C2D_SpriteSheet /*sprites*/)
    : func(func_text)
{
}

void GraphMode::reset_view()
{
    xmin = -10.0; xmax = 10.0;
    ymin = -10.0; ymax = 10.0;
    mark_dirty();
}

float GraphMode::px_to_x(int px) const
{
    return static_cast<float>(xmin + (xmax - xmin) * (px / static_cast<double>(TOP_W)));
}
float GraphMode::py_to_y(int py) const
{
    // screen y grows downward, math y grows upward
    return static_cast<float>(ymax - (ymax - ymin) * (py / static_cast<double>(TOP_H)));
}
int GraphMode::x_to_px(float x) const
{
    double px = (x - xmin) / (xmax - xmin) * TOP_W;
    if(px < -CLAMP_PX) px = -CLAMP_PX;
    if(px > CLAMP_PX) px = CLAMP_PX;
    return static_cast<int>(px);
}
int GraphMode::y_to_py(float y) const
{
    const double t = (ymax - y) / (ymax - ymin);
    double py = t * TOP_H;
    if(py < -CLAMP_PX) py = -CLAMP_PX;
    if(py > CLAMP_PX) py = CLAMP_PX;
    return static_cast<int>(py);
}

void GraphMode::open_function_editor()
{
    // NOTE: this is the one part of the patch I'm least certain of without
    // a devkitARM install to check against - swkbdSetValidation's exact
    // parameter meaning (filterFlags vs a callback pointer) has drifted
    // slightly across libctru versions. If this doesn't compile, check
    // <3ds/applets/swkbd.h> in your devkitPro install and adjust this one
    // call; everything else in this function is version-stable libctru API.
    SwkbdState swkbd;
    char buf[Expr::kMaxInputLength + 1] = {0};

    // SWKBD_TYPE_NORMAL keeps letters+numbers+symbols available, which is
    // everything the expression parser needs. 1 button (just "OK") keeps
    // this simple; user can back out with B same as system convention.
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, static_cast<int>(Expr::kMaxInputLength));
    swkbdSetHintText(&swkbd, "e.g. sin(x)+x^2 or sqrt(1-x^2)");
    swkbdSetInitialText(&swkbd, func_text.c_str());
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);

    const SwkbdButton button = swkbdInputText(&swkbd, buf, sizeof(buf));
    if(button == SWKBD_BUTTON_CONFIRM)
    {
        func_text.assign(buf);
        func = Expr(func_text); // Expr's constructor never throws / can't fail
                                 // destructively - ok() just becomes false on
                                 // bad input, see expr_parser.h
        mark_dirty();
    }
    // SWKBD_BUTTON_LEFT (cancel) or any error: leave the previous function
    // and graph exactly as they were. Nothing to clean up.
}

void GraphMode::handle_buttons(u32 kDown, u32 kDownRepeat)
{
    if(kDown & KEY_X)
    {
        reset_view();
    }
    if(kDown & KEY_Y)
    {
        show_grid = !show_grid; // pure draw-time flag, no resample needed
    }

    // Zoom responds to held-repeat (not just the initial press) so you can
    // hold L/R the way you'd hold a button on a real graphing calculator,
    // instead of having to tap repeatedly. X/Y deliberately stay kDown-only
    // above - a reset or grid toggle repeating while held would be a
    // surprising, easy-to-trigger-by-accident behavior.
    const u32 zoom_keys = kDown | kDownRepeat;
    if(zoom_keys & KEY_L)
    {
        const double cx = (xmin + xmax) * 0.5, cy = (ymin + ymax) * 0.5;
        const double hw = (xmax - xmin) * 0.5 * 1.25, hh = (ymax - ymin) * 0.5 * 1.25;
        xmin = cx - hw; xmax = cx + hw;
        ymin = cy - hh; ymax = cy + hh;
        mark_dirty();
    }
    if(zoom_keys & KEY_R)
    {
        const double cx = (xmin + xmax) * 0.5, cy = (ymin + ymax) * 0.5;
        const double hw = (xmax - xmin) * 0.5 * 0.8, hh = (ymax - ymin) * 0.5 * 0.8;
        if(hw > 1e-6 && hh > 1e-6) // guard against zooming into a degenerate/zero-size view
        {
            xmin = cx - hw; xmax = cx + hw;
            ymin = cy - hh; ymax = cy + hh;
            mark_dirty();
        }
    }
}

void GraphMode::handle_circle_pad(int dx, int dy)
{
    // dx/dy come in roughly [-156, 156] from hidCircleRead, same as the
    // range main.cpp already thresholds at +/-20 before calling this.
    //
    // Pushing right should reveal content further right (xmin/xmax
    // increase) and pushing up should reveal content further up
    // (ymin/ymax increase) - the standard "joystick pans the camera in the
    // direction you push it" convention. An earlier version of this had
    // the x-axis inverted relative to y; fixed so both axes agree.
    const double panx = (xmax - xmin) * (dx / 2000.0);
    const double pany = (ymax - ymin) * (dy / 2000.0);
    xmin += panx; xmax += panx;
    ymin += pany; ymax += pany;
    mark_dirty();
}

void GraphMode::handle_touch(int x, int y)
{
    if(x >= BOX_X && x < BOX_X + BOX_W && y >= BOX_Y && y < BOX_Y + BOX_H)
    {
        open_function_editor();
    }
}

void GraphMode::update()
{
    resample_if_needed();
}

void GraphMode::resample_if_needed()
{
    if(!dirty) return;
    dirty = false;

    if(!func.ok())
    {
        sample_valid.fill(false);
        return;
    }

    for(int px = 0; px < TOP_W; ++px)
    {
        const double x = px_to_x(px);
        const double y = func.eval(x);
        if(std::isfinite(y))
        {
            sample_valid[px] = true;
            sample_y[px] = static_cast<float>(y_to_py(static_cast<float>(y)));
        }
        else
        {
            sample_valid[px] = false;
        }
    }
}

void GraphMode::draw_top(C2D_SpriteSheet sprites) const
{
    C2D_DrawRectSolid(0, 0, 0.0f, TOP_W, TOP_H, COLOR_WHITE);

    if(show_grid)
    {
        constexpr int DIVS = 8;
        for(int i = 1; i < DIVS; ++i)
        {
            const int gx = TOP_W * i / DIVS;
            const int gy = TOP_H * i / DIVS;
            C2D_DrawRectSolid(gx, 0, 0.3f, 1, TOP_H, COLOR_GRAY);
            C2D_DrawRectSolid(0, gy, 0.3f, TOP_W, 1, COLOR_GRAY);
        }
    }

    // axes (only drawn if the origin is actually in view)
    if(xmin <= 0.0 && 0.0 <= xmax)
    {
        C2D_DrawRectSolid(x_to_px(0.0f), 0, 0.4f, 2, TOP_H, COLOR_BLACK);
    }
    if(ymin <= 0.0 && 0.0 <= ymax)
    {
        C2D_DrawRectSolid(0, y_to_py(0.0f), 0.4f, TOP_W, 2, COLOR_BLACK);
    }

    if(!func.ok())
    {
        UiText::draw(sprites, "error see fx box below", 8, 8, 0.9f, COLOR_BLACK);
        return;
    }

    // the curve: consecutive valid samples become a line segment; a gap
    // (invalid sample, e.g. an asymptote or domain edge) simply isn't
    // connected, matching how a real graphing calculator draws it.
    constexpr u32 CURVE_COLOR = C2D_Color32(30, 90, 220, 255);
    for(int px = 0; px + 1 < TOP_W; ++px)
    {
        if(sample_valid[px] && sample_valid[px + 1])
        {
            C2D_DrawLine(
                static_cast<float>(px), sample_y[px], CURVE_COLOR,
                static_cast<float>(px + 1), sample_y[px + 1], CURVE_COLOR,
                2.0f, 0.6f
            );
        }
    }

    UiText::draw_number(sprites, xmin, 4, TOP_H - 16, 0.8f, COLOR_GRAY);
    UiText::draw_number(sprites, xmax, TOP_W - 30, TOP_H - 16, 0.8f, COLOR_GRAY);
    UiText::draw_number(sprites, ymax, 4, 2, 0.8f, COLOR_GRAY);
    UiText::draw_number(sprites, ymin, 4, TOP_H - 32, 0.8f, COLOR_GRAY);

    // Small persistent mode indicator, since SELECT-to-toggle has no other
    // visible affordance telling you which screen you're looking at.
    UiText::draw_right_aligned(sprites, "graph", TOP_W - 4, 2, 0.8f, COLOR_GRAY);
}

void GraphMode::draw_bottom(C2D_SpriteSheet sprites) const
{
    C2D_DrawRectSolid(0, 0, 0.0f, 320, 240, COLOR_WHITE);

    const u32 box_color = func.ok() ? COLOR_GRAY : C2D_Color32(220, 120, 120, 255);
    C2D_DrawRectSolid(BOX_X, BOX_Y, 0.3f, BOX_W, BOX_H, box_color);
    UiText::draw(sprites, func_text.empty() ? std::string("tap to edit") : func_text,
                 BOX_X + 4, BOX_Y + 6, 0.5f, COLOR_BLACK);

    if(!func.ok())
    {
        UiText::draw(sprites, func.error_message(), BOX_X, BOX_Y + BOX_H + 6, 0.5f, C2D_Color32(180, 0, 0, 255));
    }

    UiText::draw(sprites, "circlepad pan", 8, 200, 0.5f, COLOR_GRAY);
    UiText::draw(sprites, "l r zoom", 8, 212, 0.5f, COLOR_GRAY);
    UiText::draw(sprites, "x reset y grid", 8, 224, 0.5f, COLOR_GRAY);
}
