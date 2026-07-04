#include "base/error_handling.h"

#include "GW/textures/arenanet_file_parser.h"

#include "GW/textures/gw_dat_reader.h"

#include <cstring>

namespace GW::textures::ArenaNetFileParser {

void FileIdToFileHash(uint32_t file_id, wchar_t* fileHash) {
    fileHash[0] = static_cast<wchar_t>(((file_id - 1) % 0xff00) + 0x100);
    fileHash[1] = static_cast<wchar_t>(((file_id - 1) / 0xff00) + 0x100);
    fileHash[2] = 0;
}

uint32_t FileHashToFileId(const wchar_t* fileHash) {
    if (!fileHash) return 0;
    if (((0xff < *fileHash) && (0xff < fileHash[1])) && ((fileHash[2] == 0 || ((0xff < fileHash[2] && (fileHash[3] == 0)))))) {
        return (*fileHash - 0xff00ff) + (uint32_t)fileHash[1] * 0xff00;
    }
    return 0;
}

const char* GameAssetFile::fileType() const {
    if (data_size < 4) return 0;
    return (const char*)data.data(); // Read file type from the first 4 bytes
}

bool GameAssetFile::parse(std::vector<uint8_t>& _data) {
    data = std::move(_data);
    data_size = data.size();
    return isValid();
}

bool GameAssetFile::readFromDat(const uint32_t _file_id, uint32_t stream_id) {
    wchar_t fileHash[4] = {0};
    FileIdToFileHash(_file_id, fileHash);
    return readFromDat(fileHash, stream_id);
}

bool GameAssetFile::readFromDat(const wchar_t* file_hash, uint32_t stream_id) {
    std::vector<uint8_t> bytes;
    file_id = FileHashToFileId(file_hash);
    data_size = 0;
    data.clear();
    if (!GWDatReader::ReadDatFile(file_hash, &bytes, stream_id)) return false;
    return parse(bytes);
}

const uint8_t ArenaNetFile::getFFNAType() const {
    return (uint8_t)data[4];
}

const bool ArenaNetFile::isValid() {
    return GameAssetFile::isValid() && strncmp(fileType(), "ffna", 4) == 0;
}

const bool ATexFile::isValid() {
    return GameAssetFile::isValid() && strncmp(fileType(), "ATEX", 4) == 0;
}

const Chunk* ArenaNetFile::FindChunk(ChunkType chunk_type) {
    size_t offset = 5;
    // Parse chunk headers and record their locations
    while (offset < data_size) {
        const auto chunk = (Chunk*)&data[offset];
        if (chunk->chunk_id == chunk_type)
            return chunk;
        offset += chunk->chunk_size + 8;
    }
    return nullptr;
}

}  // namespace GW::textures::ArenaNetFileParser
