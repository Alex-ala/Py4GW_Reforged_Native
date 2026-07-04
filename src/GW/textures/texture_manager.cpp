#include "base/error_handling.h"

#include "GW/textures/texture_manager.h"

#include "GW/textures/gw_dat_reader.h"

#include <sstream>
#include <vector>

#include <wincodec.h>

namespace GW::textures {

namespace {

// Ensure COM is initialized on the calling thread for WIC. Idempotent per
// thread; tolerates an already-initialized apartment.
void EnsureComInitialized() {
    static thread_local bool initialized = false;
    if (initialized) {
        return;
    }
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // S_FALSE = already initialized on this thread; RPC_E_CHANGED_MODE = COM
    // already up in a different apartment. Both are fine for our use.
    if (hr == S_OK || hr == S_FALSE || hr == RPC_E_CHANGED_MODE) {
        initialized = true;
    }
}

IWICImagingFactory* GetWicFactory() {
    static IWICImagingFactory* factory = nullptr;
    if (factory) {
        return factory;
    }
    EnsureComInitialized();
    const HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    return SUCCEEDED(hr) ? factory : nullptr;
}

}  // namespace

TextureManager& TextureManager::Instance() {
    static TextureManager instance;
    return instance;
}

void TextureManager::SetDevice(IDirect3DDevice9* device) {
    d3d_device_ = device;
    GWDatReader::Instance().SetDevice(device);
}

void TextureManager::AddTexture(const std::wstring& name, IDirect3DTexture9* texture) {
    textures_[name] = TimedTexture(texture, name);
}

void TextureManager::AddInMemoryTexture(const std::wstring& name, IDirect3DTexture9* texture) {
    if (!texture) {
        return;
    }
    textures_[name] = TimedTexture(texture, name);
}

IDirect3DTexture9* TextureManager::LoadTextureFromFileWIC(const std::wstring& path) {
    if (!d3d_device_ || path.empty()) {
        return nullptr;
    }

    IWICImagingFactory* factory = GetWicFactory();
    if (!factory) {
        return nullptr;
    }

    IWICBitmapDecoder* decoder = nullptr;
    if (FAILED(factory->CreateDecoderFromFilename(
            path.c_str(), nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnDemand, &decoder)) || !decoder) {
        return nullptr;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    IDirect3DTexture9* tex = nullptr;

    if (SUCCEEDED(decoder->GetFrame(0, &frame)) && frame
        && SUCCEEDED(factory->CreateFormatConverter(&converter)) && converter
        && SUCCEEDED(converter->Initialize(
               frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone,
               nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
        UINT width = 0;
        UINT height = 0;
        if (SUCCEEDED(converter->GetSize(&width, &height)) && width && height) {
            // Decode into a tightly packed BGRA buffer, then upload row by row
            // to respect the D3D surface pitch.
            const UINT stride = width * 4;
            std::vector<uint8_t> pixels(static_cast<size_t>(stride) * height);
            if (SUCCEEDED(converter->CopyPixels(
                    nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()))) {
                if (d3d_device_->CreateTexture(
                        width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, nullptr) == D3D_OK
                    && tex) {
                    D3DLOCKED_RECT rect;
                    if (tex->LockRect(0, &rect, nullptr, D3DLOCK_DISCARD) == D3D_OK) {
                        const uint8_t* src = pixels.data();
                        for (UINT y = 0; y < height; ++y) {
                            memcpy(reinterpret_cast<uint8_t*>(rect.pBits) + static_cast<size_t>(y) * rect.Pitch,
                                   src, stride);
                            src += stride;
                        }
                        tex->UnlockRect(0);
                    }
                    else {
                        tex->Release();
                        tex = nullptr;
                    }
                }
            }
        }
    }

    if (converter) converter->Release();
    if (frame) frame->Release();
    decoder->Release();
    return tex;
}

IDirect3DTexture9* TextureManager::GetTexture(const std::wstring& name) {
    // Dat-backed keys resolve through the GWDatReader (its own cache).
    // "gwdat://<file_id>" is a plain texture; "gwdat://<model>/<tint>/<d1>/<d2>/<d3>/<d4>"
    // is a dyed/colored model texture.
    if (GWDatReader::IsDatTextureKey(name)) {
        std::vector<uint32_t> parts;
        std::wstringstream stream(name.substr(8));
        std::wstring part;
        while (std::getline(stream, part, L'/')) {
            if (part.empty()) {
                return nullptr;
            }
            try {
                parts.push_back(static_cast<uint32_t>(std::stoul(part)));
            }
            catch (...) {
                return nullptr;
            }
        }
        if (parts.size() == 1) {
            return GWDatReader::Instance().GetTextureByFileId(parts[0]);
        }
        if (parts.size() == 6) {
            return GWDatReader::Instance().GetColoredModelTexture(
                parts[0],
                static_cast<uint8_t>(parts[1]),
                static_cast<uint8_t>(parts[2]),
                static_cast<uint8_t>(parts[3]),
                static_cast<uint8_t>(parts[4]),
                static_cast<uint8_t>(parts[5]));
        }
        return nullptr;
    }

    auto it = textures_.find(name);
    if (it != textures_.end()) {
        it->second.Touch();
        return it->second.texture;
    }

    if (!d3d_device_) {
        return nullptr;
    }

    IDirect3DTexture9* new_texture = LoadTextureFromFileWIC(name);
    if (new_texture) {
        AddTexture(name, new_texture);
    }
    return new_texture;
}

void TextureManager::CleanupOldTextures(int timeout_seconds) {
    const auto now = std::chrono::steady_clock::now();
    for (auto it = textures_.begin(); it != textures_.end();) {
        const auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.last_used).count();
        if (duration > timeout_seconds) {
            if (it->second.texture) {
                it->second.texture->Release();
            }
            it = textures_.erase(it);
        }
        else {
            ++it;
        }
    }
    GWDatReader::Instance().CleanupOldTextures(timeout_seconds);
}

}  // namespace GW::textures
