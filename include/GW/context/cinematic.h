#pragma once

#include "base/error_handling.h"

#include <cstdint>

namespace GW::Context {

    struct Cinematic {
        /* +h0000 */ uint32_t h0000;
        /* +h0004 */ uint32_t h0004; // pointer to data
        // ...
    };

}  // namespace GW::Context
