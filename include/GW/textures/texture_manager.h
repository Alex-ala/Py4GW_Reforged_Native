#pragma once

#include "base/error_handling.h"

#include <chrono>
#include <string>
#include <unordered_map>

#include <d3d9.h>

// Top-level texture cache, migrated from the legacy TextureManager (name kept
// verbatim). Loads file textures via WIC (the modern, dependency-free path -
// replaces the deprecated D3DXCreateTextureFromFileW which required the retired
// DirectX SDK), routes "gwdat://" keys to the GWDatReader, and auto-disposes
// unused textures via a timed LRU.
namespace GW::textures {

struct TimedTexture {
    IDirect3DTexture9* texture = nullptr;
    std::wstring name;
    std::chrono::steady_clock::time_point last_used;

    TimedTexture() = default;
    TimedTexture(IDirect3DTexture9* tex, const std::wstring& key)
        : texture(tex), name(key), last_used(std::chrono::steady_clock::now()) {
    }

    void Touch() {
        last_used = std::chrono::steady_clock::now();
    }
};

class TextureManager {
public:
    static TextureManager& Instance();

    // Assign the device before any texture use. Also forwards to GWDatReader.
    void SetDevice(IDirect3DDevice9* device);

    void AddTexture(const std::wstring& name, IDirect3DTexture9* texture);

    // Returns the cached texture for `name`, loading it on demand. "gwdat://"
    // keys resolve through the GWDatReader; other names are file paths loaded
    // via WIC.
    IDirect3DTexture9* GetTexture(const std::wstring& name);

    // Cache an externally-owned texture without loading from disk.
    void AddInMemoryTexture(const std::wstring& name, IDirect3DTexture9* texture);

    void CleanupOldTextures(int timeout_seconds = 30);

private:
    TextureManager() = default;
    ~TextureManager() = default;
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    // WIC file loader: decode to 32bpp BGRA and upload to a managed D3D9
    // texture (CreateTexture + LockRect). No D3DX.
    IDirect3DTexture9* LoadTextureFromFileWIC(const std::wstring& path);

    std::unordered_map<std::wstring, TimedTexture> textures_;
    IDirect3DDevice9* d3d_device_ = nullptr;
};

}  // namespace GW::textures
