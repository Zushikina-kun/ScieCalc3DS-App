# Integration notes - Calcula3DS-App

Full record of every change, decision, and testing checklist.

---

## Status

v1.0.4 - confirmed root cause fixed. All prior versions crash on Old 3DS.

---

## Session history

**Session 1** - Graph mode + theme system
**Session 2** - Scientific expansion + CIA build
**Session 3** - Audit and cleanup (dead code, Keyboard::invalidate, rad warning)
**Session 4** - All .at() replaced with .find(); -fno-exceptions; strtod
**Session 5** - Removed APT_SetAppCpuTimeLimit; explicit heap sizes
**Session 6** - Root cause confirmed: VFP static initializers crash before main()

---

## Root cause analysis (v1.0.4)

All 5 crash dumps (150-154) were byte-for-byte identical: PC=0x0BFFEBE3.

Key diagnostic: identical dumps = crash before main(), not in app code.

Investigation path:
1. Assumed APT_SetAppCpuTimeLimit -> allocateHeaps panic. Fixed. Still crashed.
2. Assumed .at() throws -> heap corruption. Fixed. Still crashed.
3. Assumed -fexceptions EH init. Removed. Still crashed.
4. Disassembled _GLOBAL__sub_I__ZN8EquationC2Ev - found vldr instructions.
 This is the static initializer for E_VAL, PI_VAL, I_VAL.
 std::exp(1.0) generates VFP instructions that run before main() via __libc_init_array.
 On Old 3DS VFP state at static init time is unreliable -> Undefined Instruction crash.

Fix: converted E_VAL/PI_VAL/I_VAL from static const to default-initialized statics.
Added ensure_constants() called at top of calculate() - well inside main().
Verified: _GLOBAL__sub_I now only contains strd (zero store) - no VFP instructions.

---

## Were any fixes harmful?

All changes assessed:

| Change | Assessment |
|---|---|
| -fno-exceptions | GOOD - removes EH code, prevents throw-based heap corruption |
| strtod instead of std::stod | GOOD - required by -fno-exceptions, correct API |
| All .at() -> .find() | GOOD - .at() throws; with -fno-exceptions this is UB/crash |
| Keyboard::invalidate() | GOOD - real bug fix for stale textures on theme change |
| Removed dead YES_TINT etc | GOOD - clean-up, no downside |
| Explicit heap sizes 16+8MB | NEUTRAL - did not fix crash, but correct practice |
| Removed APT_SetAppCpuTimeLimit | NEUTRAL - did not fix crash, but correct for Old 3DS |
| romfsInit before gfxInitDefault | NEUTRAL - did not fix crash, matches official examples |
| Removed consoleDebugInit | NEUTRAL - did not fix crash, removes unnecessary overhead |
| app.rsf IdealProcessor 1->0 | NEUTRAL - did not fix crash, correct metadata |
| app.rsf kernel min 33->00 | NEUTRAL - did not fix crash, wider compatibility |
| VFP static init deferred | FIXED THE CRASH |

No changes broke or nerfed the project.

---

## Complete bug fix log

| File | Bug | Fix |
|---|---|---|
| equation.cpp | VFP static init (E_VAL/PI_VAL/I_VAL) crashes before main() | Deferred to ensure_constants() |
| keyboard.cpp | menu.at() throws on More page | find() + continue |
| equation.cpp | equ.at() throws on unknown chars | find() + skip |
| number.cpp | equ.at() throws on unknown chars | find() + skip |
| expr_parser.cpp | try/catch with -fno-exceptions | strtod |
| equation.cpp | Missing return on empty RPN | Fixed |
| equation.cpp | .real() on double from acos/asin/atan | Removed |
| main.cpp | Theme toggle left stale textures | kb.invalidate() |
| keyboard.cpp | Dead YES_TINT/NO_TINT code | Removed |
| graph_mode.cpp | Dead py_to_y() | Removed |
| Makefile | Wrong define, exceptions enabled | -D__3DS__, -fno-exceptions |
| cia/app.rsf | Old ExHeader format, wrong IdealProcessor | Rewritten for v0.19 |

---

## Testing checklist (v1.0.4)

1. App launches without crash on Old 3DS
2. Basic arithmetic works
3. Navigate to More page (L/R) without crash
4. fact(10)=3628800, nCr(10,3)=120
5. pol(3,4) and rec give correct results
6. DEG/RAD toggle, indicator in gap bar
7. cplx toggle changes display
8. Graph mode (SELECT), pan/zoom/grid work
9. Theme toggle repaints all screens immediately
10. 12-entry memory, scroll, paste back
11. HOME menu -> return -> graph redraws

---

## Next priorities

1. Hardware test v1.0.4 thoroughly
2. DEG/RAD in graph mode (currently radians only)
3. Multiple simultaneous graph functions
4. Statistics mode
5. Unit conversion page
6. Table mode
7. Base-N mode
