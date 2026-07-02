# Graph mode patch - integration notes

I don't have devkitARM or a 3DS/Citra here to compile or run this, so treat
this as a first draft: read it, build it, and tell me what breaks so we can
iterate. I optimized for **not touching any existing file's logic** so that
if something's wrong, you can rip these 6 files back out and be exactly
where you started.

## Files added
- `ui_text.h/.cpp`, `expr_parser.h/.cpp`, `graph_mode.h/.cpp` - additive
  only, described below.
- `sleep_hook.h/.cpp` - additive only, described below.
- Everything above is additive-only and can be removed with zero effect on
  the existing calculator. The **one exception** is the optional New3DS
  core-affinity change to `keyboard.cpp`, called out explicitly in its own
  section below - that one touches existing, working code, so it's opt-in
  and flagged separately from everything else.

Drop the 8 new files into `source/`. The Makefile already globs
`*.cpp`/`*.h` in `source/`, so no Makefile changes are needed for those.

## Why a second, separate expression parser

The existing `Equation` class is the pretty-printing editor (fractions,
exponents rendered as real 2D layouts) driven by `Keyboard`'s ~700 lines of
touch/button handling in `keyboard.cpp`. Its `variables` map and the code
that turns a keypress into `add_part_at()` calls are private to `Keyboard`.

I could have reused it, but doing so correctly would mean either exposing
private internals of `Keyboard` or re-deriving how it drives `Equation`
without being able to compile-test the result - exactly the kind of change
most likely to introduce a regression in the calculator you already rely on.
So graph mode gets its own small, self-contained, independently-testable
parser instead. It deliberately mirrors the existing engine's conventions
(`ln` = natural log, `log` = log10, same function names) so it *feels* like
one app even though it's two engines under the hood.

**v2 idea, not done here:** once this is stable, the real unification is to
extract `Equation::calculate`'s function-dispatch table into something
`Expr` can call directly, so there's truly one math engine. That's a bigger,
riskier refactor I'd rather do after the plotting/UI side is proven on
hardware.

## main.cpp changes

Everything below is additive. Nothing inside the existing `if(!calculating)`
block for the standard calculator changes.

```cpp
#include "graph_mode.h"   // add near the other includes

// after: Keyboard kb(sprites);
GraphMode graph(sprites);
bool in_graph_mode = false;

// inside while(aptMainLoop()), right after kDown is read:
if(kDown & KEY_SELECT)
{
    in_graph_mode = !in_graph_mode;
}

// replace the body between C3D_FrameBegin and C3D_FrameEnd with a branch:
if(in_graph_mode)
{
    graph.update();

    C2D_SceneBegin(top);
    graph.draw_top(sprites);

    C2D_SceneBegin(bottom);
    graph.draw_bottom(sprites);
}
else
{
    // <-- existing standard-calculator drawing code, unchanged -->
}

// and further down, the input dispatch also branches the same way:
if(in_graph_mode)
{
    if((kDownRepeat | kDown) & ~(KEY_TOUCH | CIRCLE_PAD_VALUES))
    {
        graph.handle_buttons(kDown, kDownRepeat);
    }
    else if(kDown & KEY_TOUCH)
    {
        touchPosition pos;
        hidTouchRead(&pos);
        graph.handle_touch(pos.px, pos.py);
    }
    else if(kHeld & CIRCLE_PAD_VALUES)
    {
        circlePosition pos;
        hidCircleRead(&pos);
        if(abs(pos.dx) > 20 || abs(pos.dy) > 20) graph.handle_circle_pad(pos.dx, pos.dy);
    }
}
else
{
    // <-- existing kb.handle_* calls, unchanged -->
}
```

`KEY_SELECT` was unused before this, so nothing else in the app reacts to
it. `START` still always quits, from either mode.

## Why SELECT-to-toggle instead of a proper tab bar

A touch-driven tab strip (Standard / Graph / Matrix / Converter icons you
tap) is the more discoverable, "intuitive for all users" UI and is where
this should end up. I didn't build it in this pass because it wants to live
on the same screen strip the equation editor currently owns, and I'd rather
not reshuffle `Keyboard`'s existing layout constants (`EQU_REGION_HEIGHT`
etc.) without being able to compile and look at the result. Once graph mode
itself is confirmed working on hardware, swapping SELECT for a real tab
strip is a small, low-risk follow-up - happy to draft that next.

## Controls in graph mode
- Touch the gray box at the top of the bottom screen -> opens the system
  keyboard to type `f(x)`. Try `sin(x)`, `x^2-3`, `sqrt(1-x^2)`, `1/x`.
- Circle pad -> pan
- L / R -> zoom out / in
- X -> reset view to -10..10
- Y -> toggle grid lines
- SELECT -> back to the standard calculator
- START -> quit (unchanged)

## Known limitations to be aware of (not bugs, just v1 scope)
- `UiText` only has glyphs for `0-9 a-z . - + * >` (whatever
  `TextMap::generate` already loaded). Error messages and hints use only
  those characters; anything else (spaces render as a small gap, other
  punctuation is silently skipped) so they won't look fully polished, but
  they also can't corrupt layout or crash.
- No log/semi-log axis scaling, no multiple functions on one graph, no
  derivative/integral/root-finding overlays yet - straightforward additions
  once the base plotting is confirmed stable.
- `Expr` input is capped at 128 characters and 32 levels of nesting on
  purpose, to keep the recursive-descent parser's stack usage bounded on
  the 3DS's small stack (`__stacksize__` is 512 KB for the whole app in
  `main.cpp`). If you want longer expressions, raise `kMaxInputLength` in
  `expr_parser.h` and re-test rather than removing the cap.

## Sleep/resume hardening (new: sleep_hook.h/.cpp)

Add to the same include block and construct once, near where `kb`/`graph`
are constructed:

```cpp
#include "sleep_hook.h"
AppLifecycleHook lifecycle;
```

Then near the top of the `while(aptMainLoop())` body, before the existing
draw logic:

```cpp
if(lifecycle.consume_resume_flag())
{
    graph.invalidate();
    // nothing extra needed for the standard calculator screen - Keyboard
    // already redraws its full state from scratch every frame.
}
```

I could not confirm this app actually has a black-screen-after-sleep bug -
no hardware/emulator access here. This is cheap defensive hardening for a
known class of citro3d/citro2d homebrew issue, not a fix for something
I've witnessed. If it turns out to be unnecessary, it's two lines to
remove.

## Optional: New3DS syscore for the calculation thread (touches keyboard.cpp)

**Skip this entirely if you are on an original 3DS or 2DS.** This change
only benefits New3DS hardware (the model with the extra CPU cores). On
original 3DS there is no syscore, so none of this applies and applying it
would be pointless at best.

If you are on a New3DS and want to apply it anyway, the original notes are
preserved below for reference. Everything above this section is what
actually matters for original 3DS.

<details>
<summary>New3DS-only: syscore thread affinity change (click to expand)</summary>

`main.cpp` already calls `APT_SetAppCpuTimeLimit(30)` at startup. On
New3DS, that grants permission to run a thread on the syscore (the extra
core Old3DS/2DS doesn't have). But `keyboard.cpp`'s calculation thread is
created with affinity `1`, so that permission is never used - the thread
competes with the render thread on core 1 instead of getting a core to
itself.

**The change** (`source/keyboard.cpp`, inside `Keyboard::start_calculating`):

```cpp
// before:
calcThread = threadCreate(calculation_loop, this, 256 * 1024, 31, 1, false);

// after:
s32 calc_core = 1;
bool is_n3ds = false;
if(R_SUCCEEDED(APT_CheckNew3DS(&is_n3ds)) && is_n3ds)
{
    calc_core = -2; // New3DS syscore
}
calcThread = threadCreate(calculation_loop, this, 256 * 1024, 31, calc_core, false);
```

On Old3DS/2DS `calc_core` stays `1` and behavior is identical to today.
Test on both hardware types before trusting this.

</details>

## Testing checklist before you trust this

1. Builds clean with `make` under devkitARM (I could not verify this here -
   this is the most important thing to check first).
2. Runs in Citra first, then on real hardware - citro2d line/rect depth
   ordering can occasionally differ subtly between the two.
3. Toggle SELECT back and forth several times - confirm the standard
   calculator's state (current equation, memory) is untouched after
   visiting graph mode.
4. Type a deliberately broken expression (`sin(`, `2+`, `x^^2`) - confirm
   you get the red error box, not a freeze/crash.
5. Graph `1/x` and `tan(x)` - confirm you see gaps at the asymptotes rather
   than vertical lines connecting across them.
6. Zoom in (`R`) repeatedly past a sane limit - confirm it stops shrinking
   instead of divide-by-zero/inverting the view (there's a guard for this
   in `handle_buttons`, worth specifically confirming).
7. Leave graph mode open on a flat line (e.g. `func_text = "1"`) for a
   couple minutes - confirm no slow memory growth (there's no per-frame
   allocation in the hot path, but worth a sanity check with 3dbrew's
   memory tools if you have them).
8. Press HOME, wait, return to the app - confirm the graph screen looks
   correct afterward (this is what the sleep hook is defending against).

## Next pieces, in the order I'd tackle them
1. Fix whatever the build/test pass above turns up.
2. Real touch-driven tab bar (Standard / Graph / ...), replacing SELECT.
3. Finish the 3 stubbed user functions in `equation.cpp` - lowest risk,
   pure win for the scientific-calculator side.
4. Base-N (hex/oct/bin) mode and a degree/radian toggle.
5. Multiple simultaneous graphs + a legend, using the same `Expr` engine.
6. The bigger unification: one shared math engine for both editors.
