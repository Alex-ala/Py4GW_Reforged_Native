#pragma once

#include "base/error_handling.h"

namespace PY4GW {

class HookBase {
protected:
    void* _detourFunc = nullptr;
    void* _retourFunc = nullptr;
    void* _sourceFunc = nullptr;

public:
    static void Initialize();
    static void Deinitialize();

    static void EnableHooks(void* target = nullptr);
    static void DisableHooks(void* target = nullptr);

    static int CreateHook(void** target, void* detour, void** trampoline);
    static int CreateHookRaw(void* target, void* detour, void** trampoline = nullptr);
    static void RemoveHook(void* target);

    static void EnterHook();
    static void LeaveHook();
    static int GetInHookCount();
};

template <typename T>
class THook : public HookBase {
public:
    T Original() { return reinterpret_cast<T>(_retourFunc); }
    bool Valid() { return _retourFunc != nullptr; }
    bool Empty() { return _retourFunc == nullptr; }

    T Detour(T source, T detour, const unsigned length = 0);
    T Retour(bool do_cleanup = true);
};

typedef THook<unsigned char*> Hook;

}  // namespace PY4GW

template <typename T>
T PY4GW::THook<T>::Detour(T source, T detour, const unsigned) {
    if (Empty()) {
        _sourceFunc = reinterpret_cast<void*>(source);
        _detourFunc = reinterpret_cast<void*>(detour);
        HookBase::CreateHook(&_sourceFunc, _detourFunc, &_retourFunc);
    }
    return reinterpret_cast<T>(_retourFunc);
}

template <typename T>
T PY4GW::THook<T>::Retour(bool do_cleanup) {
    if (Valid() && do_cleanup) {
        HookBase::RemoveHook(_sourceFunc);
    }
    return reinterpret_cast<T>(_sourceFunc);
}
