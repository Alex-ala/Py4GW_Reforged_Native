#pragma once
//
// Force-included (via -include) into every Py4GW translation unit when building
// with the i686-w64-mingw32 cross toolchain. It provides the MSVC-only bits the
// source relies on. No effect under MSVC.
//
#ifndef _MSC_VER

// Pull in a libstdc++ header so <bits/exception_defines.h> runs and defines
// __try -> try (with exceptions enabled). exception_defines.h does NOT define
// __except / __finally (they are MSVC SEH), so we supply them: __except(x)
// becomes catch(...) to complete the try-block, and __finally becomes a plain
// block. NB: on 32-bit MinGW this turns SEH guards into C++ try/catch, which
// won't intercept hardware faults the way __except would -- acceptable for a
// scripting/bot build (same tradeoff as the classic Py4GW MinGW port).
#include <exception>

#undef  __except
#define __except(x) catch (...)
#undef  __finally
#define __finally

// NB: _ReturnAddress is NOT shimmed here -- MinGW's <intrin.h> already provides
// it as a real intrinsic, and a macro would clash with that declaration.

#endif  // !_MSC_VER

#include "base/error_handling.h"
