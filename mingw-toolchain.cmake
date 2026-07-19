set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

set(CMAKE_C_COMPILER   i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
set(CMAKE_RC_COMPILER  i686-w64-mingw32-windres)

set(CMAKE_SHARED_LIBRARY_SUFFIX ".dll")

set(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)

# ── Python 3.13 32-bit from the gw_wine prefix ───────────────────────────────
set(PY_DIR "${CMAKE_CURRENT_LIST_DIR}/../gw_wine/drive_c/Program Files (x86)/Python313-32")

# Native interpreter for build-time scripts (pybind11 version detection etc.)
set(PYTHON_EXECUTABLE "/usr/bin/python3" CACHE FILEPATH "" FORCE)
set(Python3_EXECUTABLE "${PYTHON_EXECUTABLE}" CACHE FILEPATH "" FORCE)

# Target Python headers + import library (32-bit, from the Wine prefix)
set(PY4GW_PY_INCLUDE "${PY_DIR}/include" CACHE PATH "" FORCE)
set(PY4GW_PY_LIB     "${PY_DIR}/libs/python313.lib" CACHE FILEPATH "" FORCE)

# pybind11's FindPythonLibsNew early-return: pre-seed every var it needs so it
# does not try to run/inspect a target-arch interpreter.
set(PYTHONLIBS_FOUND          TRUE   CACHE BOOL "" FORCE)
set(PYTHONLIBS_VERSION_STRING "3.13.5" CACHE STRING "" FORCE)
set(PYTHON_INCLUDE_DIRS "${PY4GW_PY_INCLUDE}" CACHE PATH "" FORCE)
set(PYTHON_LIBRARIES    "${PY4GW_PY_LIB}"     CACHE FILEPATH "" FORCE)
set(PYTHON_MODULE_EXTENSION ".pyd" CACHE STRING "" FORCE)
set(PYTHON_MODULE_PREFIX    ""     CACHE STRING "" FORCE)
set(PYTHON_VERSION_MAJOR 3  CACHE STRING "" FORCE)
set(PYTHON_VERSION_MINOR 13 CACHE STRING "" FORCE)
set(PYTHON_VERSION_PATCH 5  CACHE STRING "" FORCE)
set(PYTHON_SIZEOF_VOID_P 4  CACHE STRING "" FORCE)
set(PYTHON_IS_DEBUG      FALSE CACHE BOOL "" FORCE)
set(_PYBIND11_CROSSCOMPILING TRUE CACHE BOOL "" FORCE)

# Case-shim headers (Windows.h -> windows.h etc.) searched before the sysroot.
# _INIT so the flag is baked in before compiler-detection tests run.
set(MINGW_SHIM_DIR "${CMAKE_CURRENT_LIST_DIR}/mingw-shims")
set(CMAKE_C_FLAGS_INIT   "-I${MINGW_SHIM_DIR}")
set(CMAKE_CXX_FLAGS_INIT "-I${MINGW_SHIM_DIR}")
