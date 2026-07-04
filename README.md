# Calcula3DS

Scientific calculator for Nintendo 3DS with graph mode, dark/light theme, and scientific function expansion.

Built on [CalculaThreeDS](https://github.com/LiquidFenrir/CalculaThreeDS) by LiquidFenrir.

---

## Version history

| Version | Changes |
|---|---|
| v1.0.4 | **Real startup crash fix.** VFP static initializers (E/PI/i constants) deferred from static init to first calculate() call. All prior versions crash on Old 3DS. |
| v1.0.3 | Removed APT_SetAppCpuTimeLimit; explicit heap sizes. Crash still present. |
| v1.0.2 | -fno-exceptions, null sprite check, strtod. Crash still present. |
| v1.0.1 | All .at() -> .find(). Crash still present. |
| v1.0.0 | Initial release. Crashes on Old 3DS. |

---

## Features

### Scientific Calculator

Pretty-printed equation editor - fractions, exponents, roots render like a textbook.

**Keyboard pages**

| Page | Buttons |
|---|---|
| Basic | ( ) ^ / (fraction) del . 0-9 + - * = |
| Functions | sin cos tan asin acos atan exp abs sqrt ln cosh sinh tanh log conj acosh asinh atanh |
| Variables | a-n, x-z, ans, i, pi, > (store to variable) |
| More | fact nCr nPr mod logn / floor ceil round pol rec / cplx deg |

**More page**

| Button | Syntax | What it does |
|---|---|---|
| fact | fact(n) | n! |
| nCr | nCr(n,r) | Binomial coefficient |
| nPr | nPr(n,r) | Permutations |
| mod | mod(a,b) | Modulo |
| logn | logn(base,x) | Log to arbitrary base |
| floor | floor(x) | Round down |
| ceil | ceil(x) | Round up |
| round | round(x) | Round to nearest |
| pol | pol(x,y) | Cartesian to polar |
| rec | rec(r,t) | Polar to Cartesian |
| deg | -- | Toggle DEG/RAD |
| cplx | -- | Toggle a+bi / r>theta display |

Number types: real, complex, pi, e, i, ans, variables a-z.
Memory: 12-entry history, auto-saved. Press A on top screen to paste entry back.

---

### Graph mode

Press SELECT to switch to the f(x) graph plotter.

- sin(x), x^2-3, sqrt(1-x^2), tan(x), etc.
- Asymptotes render as gaps
- Pan: circle pad | Zoom: L/R | Reset: X | Grid: Y
- Parse errors shown in red - never crashes on bad input
- Note: graph mode always uses radians. A warning shows when DEG mode is active.

---

### Dark / light theme

- Defaults to dark mode
- Toggle: hold START then press SELECT
- Repaints all screens including equation history

---

## Controls

| Input | Action |
|---|---|
| SELECT | Toggle calculator / graph mode |
| START tap | Quit |
| Hold START + SELECT | Toggle dark / light theme |

**Calculator:** Touch=enter, A=calc, B=del, Y=clear, L/R=page, d-pad=cursor, X=focus, cpad=scroll

**Graph:** Touch f(x)=keyboard, cpad=pan, R/L=zoom, X=reset, Y=grid

---

## Building

| Tool | Notes |
|---|---|
| [devkitPro](https://devkitpro.org/wiki/Getting_Started) | Select 3DS Development group |
| devkitARM, libctru, citro2d/citro3d | Included in devkitPro |
| [makerom v0.19+](https://github.com/3DSGuy/Project_CTR/releases) | CIA only - place in devkitPro\tools\bin |
| [bannertool](https://github.com/diasurgical/bannertool/releases) | CIA only - place in devkitPro\tools\bin |

 git clone --recurse-submodules https://github.com/Zushikina-kun/Calcula3DS-App.git
 cd Calcula3DS-App/CalculaThreeDS
 make # out/CalculaThreeDS.3dsx
 make cia # out/CalculaThreeDS.cia

---

## Key fixes

| Where | Fix |
|---|---|
| equation.cpp | VFP static initializers for E/PI/i deferred to first calculate() call |
| main.cpp | romfsInit before gfxInitDefault; null sprite check; removed debug init |
| main.cpp | Explicit heap sizes 16MB+8MB; removed APT_SetAppCpuTimeLimit |
| keyboard.cpp | All .at() -> .find(); dead YES_TINT code removed; theme repaint fix |
| equation.cpp | All .at() -> .find(); .real() on double removed |
| Makefile | -fno-exceptions -fno-rtti -D__3DS__ |
| cia/app.rsf | IdealProcessor:0; makerom v0.19 format |

---

## Known limitations

- One function at a time in graph mode
- Graph mode evaluates in radians only
- No root-finding, derivative, or integral overlays

---

## Credits

Original CalculaThreeDS by [LiquidFenrir](https://github.com/LiquidFenrir/CalculaThreeDS).
Graph mode, theme, scientific expansion, and fixes by [Zushikina-kun](https://github.com/Zushikina-kun).
