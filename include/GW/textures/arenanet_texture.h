#pragma once

#include "base/error_handling.h"

// ArenaNet texture decoder. Ported from the legacy AtexAsm decompressor,
// reverse-engineered from the Guild Wars client.
namespace GW::textures {

struct SImageDescriptor {
    int xres, yres;
    unsigned char* Data;
    int a;
    int b;
    unsigned char* image;
    int imageformat;
    int c;
};

void AtexDecompress(unsigned int* input, unsigned int unknown, unsigned int imageformat, SImageDescriptor ImageDescriptor, unsigned int* output);

}  // namespace GW::textures
