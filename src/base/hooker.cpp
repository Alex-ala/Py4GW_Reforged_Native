#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/hooker.h"

#include "base/scanner.h"

#include <MinHook.h>

#include <atomic>
#include <cstdio>

namespace {

std::atomic<int> init_count = 0;
std::atomic<int> in_hook_count = 0;

}

namespace py4gw {

void HookBase::Initialize()
{
    CrashContextScope context("runtime", "hooker", "initialize");
    ++init_count;
    MH_Initialize();
}

void HookBase::Deinitialize()
{
    CrashContextScope context("shutdown", "hooker", "deinitialize");
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
    char detail[64] = {};
    _snprintf_s(detail, sizeof(detail), _TRUNCATE, "target=0x%p", target ? target : MH_ALL_HOOKS);
    CrashContextScope context("runtime", "hooker", "enable_hooks", detail);
    if (!target)
        target = MH_ALL_HOOKS;
    MH_EnableHook(target);
}

void HookBase::DisableHooks(void* target)
{
    char detail[64] = {};
    _snprintf_s(detail, sizeof(detail), _TRUNCATE, "target=0x%p", target ? target : MH_ALL_HOOKS);
    CrashContextScope context("shutdown", "hooker", "disable_hooks", detail);
    if (!target)
        target = MH_ALL_HOOKS;
    MH_DisableHook(target);
}

int HookBase::CreateHook(void** target, void* detour, void** trampoline)
{
    char detail[96] = {};
    _snprintf_s(detail, sizeof(detail), _TRUNCATE, "target=0x%p detour=0x%p", target ? *target : nullptr, detour);
    CrashContextScope context("startup", "hooker", "create_hook", detail);
    if (!(target && *target))
        return -1;
    if (const auto nested = Scanner::FunctionFromNearCall(reinterpret_cast<uintptr_t>(*target), false))
        *target = reinterpret_cast<void*>(nested);
    return static_cast<int>(MH_CreateHook(*target, detour, trampoline));
}

int HookBase::CreateHookRaw(void* target, void* detour, void** trampoline)
{
    char detail[96] = {};
    _snprintf_s(detail, sizeof(detail), _TRUNCATE, "target=0x%p detour=0x%p", target, detour);
    CrashContextScope context("startup", "hooker", "create_hook_raw", detail);
    if (!target)
        return -1;
    return static_cast<int>(MH_CreateHook(target, detour, trampoline));
}

void HookBase::RemoveHook(void* target)
{
    char detail[64] = {};
    _snprintf_s(detail, sizeof(detail), _TRUNCATE, "target=0x%p", target);
    CrashContextScope context("shutdown", "hooker", "remove_hook", detail);
    if (target)
        MH_RemoveHook(target);
}

}  // namespace py4gw
