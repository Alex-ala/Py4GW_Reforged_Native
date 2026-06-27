#pragma once

#include "base/error_handling.h"

#include <cstddef>
#include <cstdint>

namespace py4gw {

class MemoryPatcher {
public:
    MemoryPatcher() = default;
    MemoryPatcher(const MemoryPatcher&) = delete;
    MemoryPatcher& operator=(const MemoryPatcher&) = delete;
    ~MemoryPatcher();

    void Reset();
    bool IsValid() const;
    void SetPatch(uintptr_t address, const void* patch, size_t size);

    uintptr_t GetAddress() const {
        return reinterpret_cast<uintptr_t>(address_);
    }

    bool SetRedirect(uintptr_t call_instruction_address, void* redirect_func);

    bool TogglePatch(bool enabled);
    bool TogglePatch() {
        return TogglePatch(!active_);
    }

    bool GetIsActive() const {
        return active_;
    }

    static void DisableHooks();
    static void EnableHooks();

private:
    void PatchActual(bool patch);

    void* address_ = nullptr;
    uint8_t* patch_ = nullptr;
    uint8_t* backup_ = nullptr;
    size_t size_ = 0;
    bool active_ = false;
};

}  // namespace py4gw
