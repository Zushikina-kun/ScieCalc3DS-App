#ifndef INC_SLEEP_HOOK_H
#define INC_SLEEP_HOOK_H

#include <3ds.h>

// IMPORTANT CONTEXT: I could not reproduce or confirm an actual
// black-screen-after-sleep bug in CalculaThreeDS - I have no hardware or
// emulator access here. This is prophylactic hardening for a known *class*
// of citro3d/citro2d homebrew issue (apps that come back from the HOME menu
// or system sleep with stale/blank output), not a fix for a bug I've
// witnessed in this specific app. It's cheap to have and easy to delete if
// testing shows it's unnecessary.
//
// API verified against the current libctru reference (v2.3.1):
// https://libctru.devkitpro.org/apt_8h.html - the hook registration
// function is aptHook(aptHookCookie*, aptHookFn, void*), where
// aptHookFn = void(*)(APT_HookType, void*). Some older forum examples use
// aptHookAdd, which is not the current name - flagging this explicitly
// since I got it wrong once already in an earlier design pass and had to
// verify rather than trust memory here.
//
// Usage in main.cpp:
//
//   AppLifecycleHook lifecycle;
//   ...
//   while(aptMainLoop())
//   {
//       if(lifecycle.consume_resume_flag())
//       {
//           graph.invalidate(); // forces a full resample + redraw
//           // The standard calculator screen has no cached "is this still
//           // valid" state - Keyboard redraws its current equation/memory
//           // from scratch every frame already, so nothing extra is
//           // needed there.
//       }
//       ...
//   }
class AppLifecycleHook {
public:
    AppLifecycleHook();
    ~AppLifecycleHook();

    AppLifecycleHook(const AppLifecycleHook&) = delete;
    AppLifecycleHook& operator=(const AppLifecycleHook&) = delete;

    // Returns true exactly once per resume event (restore-from-HOME-menu or
    // wake-from-sleep), then clears itself. Safe to call every frame.
    bool consume_resume_flag();

private:
    static void callback(APT_HookType hook, void* param);
    aptHookCookie cookie{};
    volatile bool resumed = false;
};

#endif
