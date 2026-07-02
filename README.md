# Calcula3DS-App

A scientific calculator with graphing for the Nintendo 3DS, based on
[CalculaThreeDS](https://github.com/LiquidFenrir/CalculaThreeDS) by LiquidFenrir.

This repo adds a **graph mode** on top of the original calculator:
a fully self-contained function plotter that runs alongside the existing
calculator without touching any of its logic.

---

## Features

### Standard calculator (original)
- Complex number support
- Pretty-printed equations with fractions, exponents, roots, and absolute values
- 12-entry memory with scrollable history
- Scientific functions: sin, cos, tan, asin, acos, atan, sinh, cosh, tanh,
  ln, log, sqrt, abs, exp, conj

### Graph mode (new)
- Plot any `f(x)` expression: `sin(x)`, `x^2-3`, `sqrt(1-x^2)`, `1/x`, etc.
- Pan with the circle pad, zoom in/out with R/L
- Toggleable grid lines
- Asymptotes and domain edges render as gaps (not vertical lines)
- Parse errors shown inline with a description — never crashes on bad input
- Surviving sleep/HOME menu correctly (sleep hook)

---

## Controls

### Both modes
| Button | Action |
|--------|--------|
| SELECT | Toggle between calculator and graph mode |
| START  | Quit |

### Calculator mode
| Input | Action |
|-------|--------|
| Touch keyboard | Enter expression |
| A | Calculate |
| B | Delete character |
| Y | Clear (new equation) |
| L / R | Switch keyboard page |
| D-pad left/right | Move cursor |
| X | Switch selection (bottom/top screen) |
| Circle pad | Scroll equation / memory |

### Graph mode
| Input | Action |
|-------|--------|
| Touch f(x) box | Open keyboard to type function |
| Circle pad | Pan view |
| R | Zoom in |
| L | Zoom out |
| X | Reset view to -10..10 |
| Y | Toggle grid lines |

---

## Building

### Requirements
- [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the 3DS dev group:
  ```
  pacman -S 3ds-dev
  ```
- citro2d and citro3d (installed as part of `3ds-dev`)

### Environment (Windows)
Set these before running make, or add them to your system environment variables:
```powershell
$env:DEVKITPRO = "C:\devkitPro"
$env:DEVKITARM = "C:\devkitPro\devkitARM"
$env:CTRULIB   = "C:\devkitPro\libctru"
$env:PATH     += ";C:\devkitPro\devkitARM\bin;C:\devkitPro\tools\bin"
```

### Build (.3dsx)
```bash
cd CalculaThreeDS
make
```
Output is at `CalculaThreeDS/out/CalculaThreeDS.3dsx`.

To clean and rebuild from scratch:
```bash
make clean && make
```

### Install (.3dsx)
Copy `CalculaThreeDS.3dsx` to `/3ds/CalculaThreeDS/` on your SD card and
launch via the Homebrew Launcher.

### CIA build
CIA builds require `makerom` and `bannertool`, which are not part of the
standard devkitPro install. To build a CIA:

1. Download `makerom` from https://github.com/3DSGuy/Project_CTR/releases
2. Download `bannertool` from https://github.com/Steveice10/bannertool/releases
3. Place both executables somewhere on your PATH
4. Add a `Makefile.cia` or use a tool like
   [FBI](https://github.com/Steveice10/FBI) to install the `.3dsx` directly
   (FBI supports `.3dsx` install as of v2.6+, no CIA needed)

The easiest path on original 3DS: use FBI to install the `.3dsx` directly,
or just copy it to the SD card and use Homebrew Launcher.

---

## Project structure

```
Calcula3DS-App/
├── CalculaThreeDS/          # Base app submodule (original calculator)
│   ├── source/
│   │   ├── main.cpp         # Entry point — wires graph mode and sleep hook
│   │   ├── keyboard.cpp/h   # Calculator UI and input
│   │   ├── equation.cpp/h   # Expression renderer and evaluator
│   │   ├── graph_mode.cpp/h # Graph mode (new)
│   │   ├── expr_parser.cpp/h# f(x) expression parser/evaluator (new)
│   │   ├── ui_text.cpp/h    # Text rendering utility for graph mode (new)
│   │   └── sleep_hook.cpp/h # APT sleep/resume hook (new)
│   ├── assets/              # Sprite sheets
│   ├── romfs/               # Runtime assets
│   └── Makefile
├── INTEGRATION_NOTES.md     # Developer notes on the graph mode patch
└── README.md                # This file
```

---

## What changed from the base app

All changes are in `CalculaThreeDS/source/`. The submodule itself
(`CalculaThreeDS/`) is not a fork — the new files are dropped in alongside
the originals and the Makefile picks them up automatically.

### New files
| File | Purpose |
|------|---------|
| `graph_mode.cpp/h` | Graph mode: coordinate transforms, sampling, drawing, input |
| `expr_parser.cpp/h` | Self-contained recursive-descent `f(x)` parser and evaluator |
| `ui_text.cpp/h` | Text rendering for graph mode using the existing glyph atlas |
| `sleep_hook.cpp/h` | APT lifecycle hook for sleep/resume screen refresh |

### Modified files
| File | Change |
|------|--------|
| `main.cpp` | Added SELECT-toggle, graph mode draw/input dispatch, sleep hook |
| `keyboard.cpp` | Added "select graph" hint in the keyboard gap strip |
| `equation.cpp` | Fixed missing `return` on `final_rpn.empty()` check (silent crash risk) |
| `Makefile` | Updated `-DARM11 -D_3DS` to `-D__3DS__` (fixes compiler warning) |

---

## Known limitations (v1 scope)

- One function at a time — no multiple graphs on the same plot yet
- No degree/radian toggle (graph mode uses radians, matching the calculator)
- No root-finding, derivative, or integral overlays
- Axis tick labels use the existing sprite font (no decimal point in some
  edge cases where the glyph is missing — renders as a gap, never crashes)
- SELECT-to-toggle is functional but a proper tab bar UI is the right
  long-term solution

---

## Credits

Original CalculaThreeDS by [LiquidFenrir](https://github.com/LiquidFenrir/CalculaThreeDS).
Graph mode patch and fixes by [Zushikina-kun](https://github.com/Zushikina-kun).
