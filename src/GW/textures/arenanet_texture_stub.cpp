// MinGW build stub for arenanet_texture.cpp.
//
// The real file implements the ATEX (.dat texture) decompressor entirely in
// MSVC `__asm { }` / `__declspec(naked)` blocks that GCC cannot compile. Texture
// decompression is only needed to render textures pulled from Gw.dat and is not
// used by the scanner / pathing / bot APIs, so under MinGW we exclude the real
// file and provide an empty AtexDecompress (the only symbol referenced outside
// the translation unit, from gw_dat_reader.cpp).
#include "GW/textures/arenanet_texture.h"

namespace GW::textures {

void AtexDecompress(unsigned int* /*input*/, unsigned int /*unknown*/,
                    unsigned int /*imageformat*/, SImageDescriptor /*ImageDescriptor*/,
                    unsigned int* /*output*/) {
    // no-op: texture decompression unsupported in the MinGW/Linux build.
}

}  // namespace GW::textures
