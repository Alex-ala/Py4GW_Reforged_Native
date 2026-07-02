#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/patterns.h"
#include "GW/gw_dat_reader/gw_dat_reader.h"

namespace GW::gw_dat_reader {

// Definitions for the module-owned resolved symbols.
OpenFileByFileId_pt g_open_file_by_file_id_func = nullptr;
FileIdToRecObj_pt g_file_hash_to_rec_obj_func = nullptr;
GetRecObjectBytes_pt g_read_file_buffer_func = nullptr;
DecodeImage_pt g_decode_image_func = nullptr;
UnkRecObjBytes_pt g_free_file_buffer_func = nullptr;
CloseRecObj_pt g_close_rec_obj_func = nullptr;
AllocateImage_pt g_allocate_image_func = nullptr;
Depalletize_pt g_depalletize_func = nullptr;

bool ResolveDecodeImageFunc() {
    CrashContextScope context("startup", "gw_dat_reader", "resolve_decode_image");
    return PY4GW::Patterns::Resolve("gw_dat_reader.decode_image_func", &g_decode_image_func);
}

bool ResolveOpenFileByFileIdFunc() {
    CrashContextScope context("startup", "gw_dat_reader", "resolve_open_file_by_file_id");
    return PY4GW::Patterns::Resolve("gw_dat_reader.open_file_by_file_id_func", &g_open_file_by_file_id_func);
}

bool ResolveFileHashToRecObjFunc() {
    CrashContextScope context("startup", "gw_dat_reader", "resolve_file_hash_to_rec_obj");
    return PY4GW::Patterns::Resolve("gw_dat_reader.file_hash_to_rec_obj_func", &g_file_hash_to_rec_obj_func);
}

bool ResolveReadFileBufferFunc() {
    CrashContextScope context("startup", "gw_dat_reader", "resolve_read_file_buffer");
    return PY4GW::Patterns::Resolve("gw_dat_reader.read_file_buffer_func", &g_read_file_buffer_func);
}

bool ResolveFreeFileBufferFunc() {
    CrashContextScope context("startup", "gw_dat_reader", "resolve_free_file_buffer");
    return PY4GW::Patterns::Resolve("gw_dat_reader.free_file_buffer_func", &g_free_file_buffer_func);
}

bool ResolveCloseRecObjFunc() {
    CrashContextScope context("startup", "gw_dat_reader", "resolve_close_rec_obj");
    return PY4GW::Patterns::Resolve("gw_dat_reader.close_rec_obj_func", &g_close_rec_obj_func);
}

bool ResolveAllocateImageFunc() {
    CrashContextScope context("startup", "gw_dat_reader", "resolve_allocate_image");
    return PY4GW::Patterns::Resolve("gw_dat_reader.allocate_image_func", &g_allocate_image_func);
}

bool ResolveDepalletizeFunc() {
    CrashContextScope context("startup", "gw_dat_reader", "resolve_depalletize");
    return PY4GW::Patterns::Resolve("gw_dat_reader.depalletize_func", &g_depalletize_func);
}

}  // namespace GW::gw_dat_reader
