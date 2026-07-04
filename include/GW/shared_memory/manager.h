#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "GW/context/context.h"

namespace GW::shared_memory {

enum class SegmentResult : uint32_t {
    Ok = 0,
    NotDue = 1,
    CallbackFailed = 2,
    Exception = 3,
};

struct SharedMemoryHeader {
    uint32_t version = Context::kSharedMemoryVersion;
    uint32_t total_size = 0;
    uint32_t sequence = 0;
    uint32_t process_id = 0;
    uint64_t window_handle = 0;
    uint64_t frame_counter = 0;
    uint64_t last_publish_tick = 0;
    uint32_t descriptor_count = 0;
    uint32_t descriptor_offset = 0;
};

struct SegmentDescriptor {
    char name[Context::kSharedMemorySegmentNameCapacity] = {};
    uint32_t offset = 0;
    uint32_t size = 0;
    uint32_t update_interval_frames = 1;
    uint32_t publish_count = 0;
    uint64_t last_published_frame = 0;
    uint32_t last_result = static_cast<uint32_t>(SegmentResult::NotDue);
    uint32_t reserved = 0;
};

struct SegmentOptions {
    uint32_t update_interval_frames = 1;
    bool zero_before_write = true;
    bool clear_on_failure = true;
    // Toggle. When false the slot is kept but published as all zeros, so the
    // layout never changes. Flip at the subscribe line or at runtime via
    // SetSegmentEnabled. (To remove a struct entirely, comment its subscribe
    // line - that DOES shift the layout, by design.)
    bool enabled = true;
};

using WriterCallback = std::function<bool(void* payload, size_t size, uint64_t frame_counter)>;

class Manager {
public:
    Manager() = default;
    ~Manager();

    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;

    // Register an arbitrary payload writer. Writers are startup-only: the
    // shared memory layout becomes fixed once Create() succeeds.
    bool RegisterSegment(std::string_view name, size_t size, WriterCallback writer, const SegmentOptions& options = {});

    template <typename T>
    bool RegisterStruct(
        std::string_view name,
        std::function<bool(T& value, uint64_t frame_counter)> writer,
        const SegmentOptions& options = {}) {
        // Convenience wrapper for typed payloads. The writer receives a T
        // reference that points directly into the mapped shared memory region.
        return RegisterSegment(
            name,
            sizeof(T),
            [writer = std::move(writer)](void* payload, size_t size, uint64_t frame_counter) -> bool {
                if (!(payload && size == sizeof(T) && writer)) {
                    return false;
                }
                return writer(*static_cast<T*>(payload), frame_counter);
            },
            options);
    }

    // ------------------------------------------------------------------
    // Subscription surface (the per-module *_shared_memory.cpp files use
    // these). One line per struct. Every subscription carries the same
    // enable/disable toggle: enabled -> writes the data, disabled -> writes
    // zeros into the same slot (layout unchanged). Comment the line out to
    // remove the struct entirely (that shifts the layout - by design).
    //
    // SubscribeStruct  : copy the singleton's data into its slot each frame
    //                    (map-ready gated).
    // SubscribePointer : publish only the address (8 bytes); Python
    //                    materializes it via ctypes.
    // SubscribeSnapshot: a fixed-size POD the writer fills (e.g. the classified
    //                    agent array).
    // The full-struct vs pointer choice per struct is the caller's, not this
    // class's - this class only transports whatever a line subscribes.
    // ------------------------------------------------------------------
    template <typename T>
    bool SubscribeStruct(std::string_view name, T* (*getter)(), bool enabled = true) {
        SegmentOptions options;
        options.enabled = enabled;
        return RegisterStruct<T>(name, [getter](T& value, uint64_t) -> bool {
            if (!PublishGuardReady()) {
                return false;
            }
            const T* source = getter();
            if (!source) {
                return false;
            }
            std::memcpy(&value, source, sizeof(T));
            return true;
        }, options);
    }

    bool SubscribePointer(std::string_view name, std::function<uintptr_t()> getter, bool enabled = true);

    template <typename T>
    bool SubscribeSnapshot(std::string_view name, std::function<bool(T& value, uint64_t frame_counter)> filler, bool enabled = true) {
        SegmentOptions options;
        options.enabled = enabled;
        return RegisterStruct<T>(name, std::move(filler), options);
    }

    // Runtime toggle for any subscribed segment (enabled -> data, disabled ->
    // zeros; slot and layout preserved either way).
    bool SetSegmentEnabled(std::string_view name, bool enabled);

    // Publish safety gate shared by every SubscribeStruct writer: never deep-copy
    // a game context while a map transition is in flight (the game thread rebuilds
    // the context tree concurrently -> reproducible 0xC0000005 in the memcpy).
    static bool PublishGuardReady();

    // Materialize the shared-memory layout after every segment has been
    // registered. This computes descriptor offsets and allocates one backing
    // file mapping for all segments.
    bool Create(const std::wstring& name);
    void Destroy();

    // Publish every due segment according to its per-segment frame interval.
    bool Update();
    // Force publication of one segment regardless of its scheduled interval.
    bool PublishSegment(std::string_view name);

    bool SetSegmentUpdateInterval(std::string_view name, uint32_t update_interval_frames);
    std::optional<SegmentDescriptor> GetSegmentDescriptor(std::string_view name) const;
    std::vector<SegmentDescriptor> GetSegmentDescriptors() const;
    std::vector<std::string> GetSegmentNames() const;

    bool IsValid() const;
    const std::wstring& Name() const;
    size_t Size() const;
    void* Data() const;
    SharedMemoryHeader* Header() const;

    template <typename T>
    T* PayloadAs(size_t offset) const {
        auto* data = PayloadData(offset, sizeof(T));
        return data ? static_cast<T*>(data) : nullptr;
    }

    static std::wstring BuildName(const wchar_t* prefix, DWORD process_id, HWND window_handle = nullptr);

private:
    struct SegmentRegistration {
        SegmentDescriptor descriptor = {};
        WriterCallback writer;
        SegmentOptions options = {};
    };

    SegmentRegistration* FindRegistration(std::string_view name);
    const SegmentRegistration* FindRegistration(std::string_view name) const;
    SegmentDescriptor* DescriptorAt(size_t index) const;
    void* PayloadData(size_t offset, size_t size) const;
    bool PublishRegistration(SegmentRegistration& registration, uint64_t frame_counter, bool force);
    void BeginWrite() const;
    void EndWrite() const;

    HANDLE mapping_handle_ = nullptr;
    void* view_ = nullptr;
    size_t total_size_ = 0;
    std::wstring name_;
    std::vector<SegmentRegistration> registrations_;
};

Manager& RuntimeManager();
bool RegisterDefaultSegments(Manager& manager);

}  // namespace GW::shared_memory
