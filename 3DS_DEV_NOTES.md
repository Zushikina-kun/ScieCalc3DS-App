
Complete reference from building CalculaThreeDS. Use this before starting any new 3DS project.

---

## 1. THE most important rule: no VFP in static initializers

Static const globals with float/double expressions run BEFORE main() via __libc_init_array.
On Old 3DS the VFP unit state at that time is unreliable.
VFP instructions (vldr, vstrd) in static initializers cause Undefined Instruction crashes.
This crash always lands at the same PC. All dump files will be byte-for-byte identical.

BAD - will crash before main():
`"
+=cpp
static const double E = std::exp(1.0); // vldr in _GLOBAL__sub_I -> crash on Old 3DS
static const Number PI_VAL(M_PI); // also triggers VFP init
`"
+=

GOOD - deferred to first use inside main():
`"
+=cpp
static double E_val = 0.0;
static bool constants_init = false;
void ensure_constants() {
 if(constants_init) return;
 E_val = std::exp(1.0); // safe here, VFP is ready inside main()
 constants_init = true;
}
`"
+=

How to check your ELF for VFP static inits:
`"
+=bash
arm-none-eabi-nm app.elf | grep _GLOBAL__sub_I
arm-none-eabi-objdump -d app.elf | grep -A20 _GLOBAL__sub_I
# If you see vldr or vstrd instructions, you have the problem
`"
+=

---

## 2. Set explicit heap sizes before main()

Never rely on libctru auto-split. Declare at global scope before main():
`"
+=cpp
u32 __ctru_heap_size = 16 * 1024 * 1024; // 16 MB regular heap
u32 __ctru_linear_heap_size = 8 * 1024 * 1024; // 8 MB linear (GPU DMA)
`"
+=

Without this, libctru auto-splits remaining memory.
APT_SetAppCpuTimeLimit() modifies the committed memory count used by auto-split,
potentially causing svcBreak(USERBREAK_PANIC) -> crash at 0x0BFFEBE3.

Old 3DS memory budget (64MB total, ~40-48MB usable per app):
 Stack: 512 KB, Code+data: ~400 KB, Heap: 16 MB, Linear: 8 MB, OS: ~16-20 MB

---

## 3. APT_SetAppCpuTimeLimit

Only use it if you need New3DS syscore access. For simple apps: remove it entirely.
If you must use it: set heap sizes FIRST at global scope, then call inside main().

---

## 4. Required build flags
`"
+=makefile
CXXFLAGS := \ -fno-rtti -fno-exceptions -std=gnu++17
CFLAGS += -D__3DS__ # NOT -DARM11 or -D_3DS
`"
+=

---

## 5. Never use throwing STL

| Instead of | Use |
|---|---|
| map.at(key) | auto it = map.find(key); if(it != end()) ... |
| std::stod(s) | strtod(s.c_str(), &endp) |
| std::stoi(s) | strtol(s.c_str(), &endp, 10) |
| vector::at(i) | check i < size() first, then [] |
| try/catch | return error codes |

---

## 6. Correct initialization order in main()
`"
+=cpp
int main(int argc, char** argv)
{
 romfsInit();
 C2D_SpriteSheet sprites = C2D_SpriteSheetLoad(romfs:/gfx/sprites.t3x);
 romfsExit();
 if(!sprites) return 1; // always null-check

 gfxInitDefault();
 C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
 C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
 C2D_Prepare();
}
`"
+=

Do NOT call consoleDebugInit(debugDevice_SVC) in production.
Do NOT call APT_SetAppCpuTimeLimit() unless heap sizes are pre-set.

---

## 7. Crash dump analysis

Dumps at: SD:/luma/dumps/arm11/crash_dump_XXXXXXXX.dmp (588 bytes)

Header layout:
 0x00: Magic 0xDEADC0DE
 0x04: Magic 0xDEADCAFE
 0x08: Processor (3=ARM11)
 0x09: Core
 0x0A: Exception type: 0=FIQ 1=Undefined 2=Prefetch 3=Data 4=IRQ
 0x50: Registers r0-r12, sp, lr, pc, cpsr, dfsr, ifsr, far, fpexc, fpinst, fpinst2

PC diagnosis:

| PC range | Meaning |
|---|---|
| 0x00100000 - 0x0014FFFF | Your app code - use arm-none-eabi-addr2line |
| 0x08000000 - 0x0E000000 | Heap area - VFP static init OR svcBreak in allocateHeaps |
| 0x0BFFEBE3 specifically | Both causes land here on Old 3DS |
| Anything else | Corrupted PC - examine LR and dump hex |

If ALL dumps are identical: crash is before main() - check VFP statics and heap setup.

Decode PC from a dump (PowerShell):
  = [System.IO.File]::ReadAllBytes(path)
  = [BitConverter]::ToUInt32(, 0x50 + 15*4)
 Write-Host (0x{0:X8} -f )

Map PC to source (bash):
 arm-none-eabi-addr2line -e out/App.elf 0xYOUR_PC

---

## 8. makerom v0.19 RSF

Old ExHeader: block removed. Fields moved to AccessControlInfo / SystemControlInfo.

Common mistakes:
- SaveDataSize: 0 is invalid -> use SaveDataSize: 0KB
- IdealProcessor: 1 = system core (unavailable to apps) -> use 0
- ReleaseKernelMinor: use 00 for widest compatibility
- SystemCallAccess: now required
- ServiceAccessControl: now required
- SdApplication removed from FileSystemAccess -> use DirectSdmc

Tools: bannertool (diasurgical/bannertool) + makerom (3DSGuy/Project_CTR)
Place both exes in devkitPro/tools/bin/

---

## 9. Old 3DS vs New 3DS

| Feature | Old 3DS | New 3DS |
|---|---|---|
| RAM | 64 MB | 256 MB |
| App CPU cores | 1 | 2 |
| CPU speed | 268 MHz | 268 or 804 MHz |
| VRAM | 6 MB | 6 MB |
| VFP at static init time | Unreliable - avoid | Usually fine |
| APT_SetAppCpuTimeLimit | Dangerous without explicit heaps | Grants syscore |

Always develop and test with Old 3DS constraints.

---

## 10. New project checklist

- [ ] No float/double VFP in static const globals or static initializers
- [ ] Check arm-none-eabi-objdump for vldr in _GLOBAL__sub_I output
- [ ] __stacksize__, __ctru_heap_size, __ctru_linear_heap_size at global scope
- [ ] -fno-exceptions -fno-rtti in CXXFLAGS, -D__3DS__
- [ ] .find() not .at() everywhere; strtod not std::stod
- [ ] romfsInit before gfxInitDefault; null-check sprite sheet
- [ ] No APT_SetAppCpuTimeLimit without pre-set heap sizes
- [ ] No consoleDebugInit(debugDevice_SVC) in production
- [ ] app.rsf: IdealProcessor:0, SaveDataSize:0KB, v0.19 format
- [ ] Test in Citra first, then real hardware
- [ ] All crash dumps identical + Undefined Instr = VFP static init issue
