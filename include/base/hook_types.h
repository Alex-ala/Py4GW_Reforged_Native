#pragma once

#include "base/error_handling.h"

#include <functional>

namespace py4gw {

struct HookEntry {};

struct HookStatus {
    bool blocked = false;
    unsigned int altitude = 0;
};

template <typename... Ts>
using HookCallback = std::function<void(HookStatus* status, Ts...)>;

}  // namespace py4gw
