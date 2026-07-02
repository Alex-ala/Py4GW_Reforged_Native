#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>
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
