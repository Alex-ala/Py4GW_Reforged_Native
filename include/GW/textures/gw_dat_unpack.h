#pragma once

#include "base/error_handling.h"

// GW.dat block decompressor, migrated verbatim from the legacy GwDatXentax.
// Decodes the game's Huffman/LZ-compressed MFT blobs (compression flag 8) used
// by the offline linked-icon resolver in GWDatReader.
namespace GW::textures {

void UnpackGWDat(unsigned char* input, int insize, unsigned char*& output, int& outsize);

}  // namespace GW::textures
