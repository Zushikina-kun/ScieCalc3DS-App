#include "sleep_hook.h"

AppLifecycleHook::AppLifecycleHook()
{
    aptHook(&cookie, &AppLifecycleHook::callback, this);
}

AppLifecycleHook::~AppLifecycleHook()
{
    aptUnhook(&cookie);
}

void AppLifecycleHook::callback(APT_HookType hook, void* param)
{
    auto* self = static_cast<AppLifecycleHook*>(param);
    switch(hook)
    {
        case APTHOOK_ONRESTORE:
        case APTHOOK_ONWAKEUP:
            self->resumed = true;
            break;
        default:
            // ONSUSPEND / ONSLEEP / ONEXIT: nothing to do for this app -
            // no open file handles, no network sockets, no GPU resources
            // that need explicit teardown before backgrounding.
            break;
    }
}

bool AppLifecycleHook::consume_resume_flag()
{
    if(resumed)
    {
        resumed = false;
        return true;
    }
    return false;
}
