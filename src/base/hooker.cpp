#include "base/error_handling.h"

#include "base/hooker.h"

#include "base/scanner.h"

#include <MinHook.h>

#include <atomic>

namespace {

std::atomic<int> init_count = 0;
std::atomic<int> in_hook_count = 0;

}

namespace py4gw {

void HookBase::Initialize()
{
    ++init_count;
    MH_Initialize();
}

void HookBase::Deinitialize()
{
    if (--init_count == 0)
        MH_Uninitialize();
}

void HookBase::EnterHook()
{
    ++in_hook_count;
}

void HookBase::LeaveHook()
{
    --in_hook_count;
}

int HookBase::GetInHookCount()
{
    return in_hook_count;
}

void HookBase::EnableHooks(void* target)
{
    if (!target)
        target = MH_ALL_HOOKS;
    MH_EnableHook(target);
}

void HookBase::DisableHooks(void* target)
{
    if (!target)
        target = MH_ALL_HOOKS;
    MH_DisableHook(target);
}

int HookBase::CreateHook(void** target, void* detour, void** trampoline)
{
    if (!(target && *target))
        return -1;
    if (const auto nested = Scanner::FunctionFromNearCall(reinterpret_cast<uintptr_t>(*target), false))
        *target = reinterpret_cast<void*>(nested);
    return static_cast<int>(MH_CreateHook(*target, detour, trampoline));
}

int HookBase::CreateHookRaw(void* target, void* detour, void** trampoline)
{
    if (!target)
        return -1;
    return static_cast<int>(MH_CreateHook(target, detour, trampoline));
}

void HookBase::RemoveHook(void* target)
{
    if (target)
        MH_RemoveHook(target);
}

}  // namespace py4gw
