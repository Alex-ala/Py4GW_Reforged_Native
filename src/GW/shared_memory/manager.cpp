#include "GW/shared_memory/manager.h"

#include "GW/shared_memory/segments.h"
#include "GW/common/constants/constants.h"
#include "GW/context/account.h"
#include "GW/context/agent.h"
#include "GW/context/camera.h"
#include "GW/context/character.h"
#include "GW/context/friend_list.h"
#include "GW/context/game.h"
#include "GW/context/gameplay.h"
#include "GW/context/guild.h"
#include "GW/context/item.h"
#include "GW/context/map.h"
#include "GW/context/party.h"
#include "GW/context/pregame.h"
#include "GW/context/render.h"
#include "GW/context/text_parser.h"
#include "GW/context/trade.h"
#include "GW/context/world.h"
#include "GW/map/map.h"
#include "base/logger.h"

#include <algorithm>
#include <cstring>

namespace GW::shared_memory {

namespace {

size_t AlignUp(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

std::string Narrow(std::wstring_view value) {
    std::string out;
    out.reserve(value.size());
    for (const wchar_t ch : value) {
        out.push_back(ch >= 0 && ch <= 0x7f ? static_cast<char>(ch) : '?');
    }
    return out;
}

void CopyName(char (&dest)[Context::kSharedMemorySegmentNameCapacity], std::string_view src) {
    const size_t length = std::min(src.size(), Context::kSharedMemorySegmentNameCapacity - 1);
    std::memset(dest, 0, sizeof(dest));
    if (length != 0) {
        std::memcpy(dest, src.data(), length);
    }
}

Manager g_runtime_manager;


}  // namespace

Manager::~Manager() {
    Destroy();
}

bool Manager::RegisterSegment(std::string_view name, size_t size, WriterCallback writer, const SegmentOptions& options) {
    if (IsValid() || name.empty() || size == 0 || !writer) {
        return false;
    }
    if (name.size() >= Context::kSharedMemorySegmentNameCapacity) {
        Logger::Instance().LogWarning("Shared memory segment name too long: " + std::string(name));
        return false;
    }
    if (FindRegistration(name)) {
        Logger::Instance().LogWarning("Shared memory segment already registered: " + std::string(name));
        return false;
    }

    SegmentRegistration registration;
    CopyName(registration.descriptor.name, name);
    registration.descriptor.size = static_cast<uint32_t>(size);
    registration.descriptor.update_interval_frames = std::max(1u, options.update_interval_frames);
    registration.descriptor.last_result = static_cast<uint32_t>(SegmentResult::NotDue);
    registration.writer = std::move(writer);
    registration.options = options;
    registration.options.update_interval_frames = std::max(1u, registration.options.update_interval_frames);

    registrations_.push_back(std::move(registration));
    return true;
}

bool Manager::Create(const std::wstring& name) {
    Destroy();

    if (name.empty() || registrations_.empty()) {
        return false;
    }

    const size_t descriptor_offset = sizeof(SharedMemoryHeader);
    const size_t descriptors_size = registrations_.size() * sizeof(SegmentDescriptor);
    size_t payload_offset = AlignUp(descriptor_offset + descriptors_size, alignof(std::max_align_t));

    for (auto& registration : registrations_) {
        registration.descriptor.offset = static_cast<uint32_t>(payload_offset);
        payload_offset = AlignUp(payload_offset + registration.descriptor.size, alignof(std::max_align_t));
    }

    total_size_ = payload_offset;
    const DWORD high = static_cast<DWORD>((static_cast<unsigned long long>(total_size_) >> 32) & 0xFFFFFFFFull);
    const DWORD low = static_cast<DWORD>(static_cast<unsigned long long>(total_size_) & 0xFFFFFFFFull);

    mapping_handle_ = ::CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, high, low, name.c_str());
    if (!mapping_handle_) {
        Logger::Instance().LogError("Failed to create shared memory mapping: " + Narrow(name));
        Destroy();
        return false;
    }

    view_ = ::MapViewOfFile(mapping_handle_, FILE_MAP_ALL_ACCESS, 0, 0, total_size_);
    if (!view_) {
        Logger::Instance().LogError("Failed to map shared memory view: " + Narrow(name));
        Destroy();
        return false;
    }

    name_ = name;
    std::memset(view_, 0, total_size_);

    auto* header = Header();
    header->version = Context::kSharedMemoryVersion;
    header->total_size = static_cast<uint32_t>(total_size_);
    header->sequence = 0;
    header->process_id = ::GetCurrentProcessId();
    header->window_handle = 0;
    header->frame_counter = 0;
    header->last_publish_tick = 0;
    header->descriptor_count = static_cast<uint32_t>(registrations_.size());
    header->descriptor_offset = static_cast<uint32_t>(descriptor_offset);

    auto* descriptors = PayloadAs<SegmentDescriptor>(descriptor_offset);
    for (size_t i = 0; i < registrations_.size(); ++i) {
        descriptors[i] = registrations_[i].descriptor;
    }

    Logger::Instance().LogInfo("Shared memory created: " + Narrow(name_));
    return true;
}

void Manager::Destroy() {
    if (view_) {
        ::UnmapViewOfFile(view_);
        view_ = nullptr;
    }
    if (mapping_handle_) {
        ::CloseHandle(mapping_handle_);
        mapping_handle_ = nullptr;
    }
    total_size_ = 0;
    name_.clear();
}

bool Manager::Update() {
    auto* header = Header();
    if (!header) {
        return false;
    }

    const uint64_t frame_counter = header->frame_counter + 1;
    bool any_due = false;
    for (auto& registration : registrations_) {
        const uint32_t interval = std::max(1u, registration.options.update_interval_frames);
        if (((frame_counter - 1) % interval) == 0) {
            any_due = true;
            break;
        }
    }

    header->frame_counter = frame_counter;
    if (!any_due) {
        return true;
    }

    BeginWrite();
    bool all_ok = true;
    for (auto& registration : registrations_) {
        const uint32_t interval = std::max(1u, registration.options.update_interval_frames);
        if (((frame_counter - 1) % interval) != 0) {
            continue;
        }
        if (!PublishRegistration(registration, frame_counter, false)) {
            all_ok = false;
        }
    }
    header->last_publish_tick = ::GetTickCount64();
    EndWrite();
    return all_ok;
}

bool Manager::PublishSegment(std::string_view name) {
    auto* header = Header();
    auto* registration = FindRegistration(name);
    if (!(header && registration)) {
        return false;
    }

    const uint64_t frame_counter = header->frame_counter + 1;
    header->frame_counter = frame_counter;
    BeginWrite();
    const bool ok = PublishRegistration(*registration, frame_counter, true);
    header->last_publish_tick = ::GetTickCount64();
    EndWrite();
    return ok;
}

bool Manager::SetSegmentUpdateInterval(std::string_view name, uint32_t update_interval_frames) {
    auto* registration = FindRegistration(name);
    if (!registration) {
        return false;
    }
    const uint32_t interval = std::max(1u, update_interval_frames);
    registration->options.update_interval_frames = interval;
    registration->descriptor.update_interval_frames = interval;

    const auto index = static_cast<size_t>(registration - registrations_.data());
    if (auto* descriptor = DescriptorAt(index)) {
        descriptor->update_interval_frames = interval;
    }
    return true;
}

bool Manager::SetSegmentEnabled(std::string_view name, bool enabled) {
    auto* registration = FindRegistration(name);
    if (!registration) {
        return false;
    }
    registration->options.enabled = enabled;
    return true;
}

bool Manager::PublishGuardReady() {
    const auto instance_type = map::GetInstanceType();
    return map::GetIsMapLoaded() &&
        !map::GetIsObserving() &&
        instance_type != Constants::InstanceType::Loading;
}

bool Manager::SubscribePointer(std::string_view name, std::function<uintptr_t()> getter, bool enabled) {
    SegmentOptions options;
    options.enabled = enabled;
    return RegisterSegment(
        name,
        sizeof(uintptr_t),
        [getter = std::move(getter)](void* payload, size_t size, uint64_t) -> bool {
            if (!(payload && size == sizeof(uintptr_t) && getter)) {
                return false;
            }
            *static_cast<uintptr_t*>(payload) = getter();
            return true;
        },
        options);
}

std::optional<SegmentDescriptor> Manager::GetSegmentDescriptor(std::string_view name) const {
    const auto* registration = FindRegistration(name);
    if (!registration) {
        return std::nullopt;
    }
    const auto index = static_cast<size_t>(registration - registrations_.data());
    if (const auto* descriptor = DescriptorAt(index)) {
        return *descriptor;
    }
    return registration->descriptor;
}

std::vector<SegmentDescriptor> Manager::GetSegmentDescriptors() const {
    std::vector<SegmentDescriptor> descriptors;
    descriptors.reserve(registrations_.size());
    for (size_t i = 0; i < registrations_.size(); ++i) {
        if (const auto* descriptor = DescriptorAt(i)) {
            descriptors.push_back(*descriptor);
        } else {
            descriptors.push_back(registrations_[i].descriptor);
        }
    }
    return descriptors;
}

std::vector<std::string> Manager::GetSegmentNames() const {
    std::vector<std::string> names;
    names.reserve(registrations_.size());
    for (const auto& registration : registrations_) {
        names.emplace_back(registration.descriptor.name);
    }
    return names;
}

bool Manager::IsValid() const {
    return mapping_handle_ != nullptr && view_ != nullptr;
}

const std::wstring& Manager::Name() const {
    return name_;
}

size_t Manager::Size() const {
    return total_size_;
}

void* Manager::Data() const {
    return view_;
}

SharedMemoryHeader* Manager::Header() const {
    return view_ ? static_cast<SharedMemoryHeader*>(view_) : nullptr;
}

std::wstring Manager::BuildName(const wchar_t* prefix, DWORD process_id, HWND window_handle) {
    wchar_t buffer[160] = {};
    swprintf_s(
        buffer,
        L"%ls_%lu_%p",
        prefix ? prefix : L"Py4GW_SharedMemory_PID",
        static_cast<unsigned long>(process_id),
        window_handle);
    return std::wstring(buffer);
}

Manager::SegmentRegistration* Manager::FindRegistration(std::string_view name) {
    const auto it = std::find_if(registrations_.begin(), registrations_.end(), [name](const SegmentRegistration& value) {
        return name == value.descriptor.name;
    });
    return it == registrations_.end() ? nullptr : &(*it);
}

const Manager::SegmentRegistration* Manager::FindRegistration(std::string_view name) const {
    const auto it = std::find_if(registrations_.begin(), registrations_.end(), [name](const SegmentRegistration& value) {
        return name == value.descriptor.name;
    });
    return it == registrations_.end() ? nullptr : &(*it);
}

SegmentDescriptor* Manager::DescriptorAt(size_t index) const {
    auto* header = Header();
    if (!(header && index < header->descriptor_count)) {
        return nullptr;
    }
    auto* descriptors = PayloadAs<SegmentDescriptor>(header->descriptor_offset);
    return descriptors ? &descriptors[index] : nullptr;
}

void* Manager::PayloadData(size_t offset, size_t size) const {
    if (!(view_ && offset + size <= total_size_)) {
        return nullptr;
    }
    return static_cast<uint8_t*>(view_) + offset;
}

bool Manager::PublishRegistration(SegmentRegistration& registration, uint64_t frame_counter, bool force) {
    auto* payload = PayloadData(registration.descriptor.offset, registration.descriptor.size);
    if (!payload) {
        return false;
    }

    if (!force) {
        const uint32_t interval = std::max(1u, registration.options.update_interval_frames);
        if (((frame_counter - 1) % interval) != 0) {
            return true;
        }
    }

    // Disabled subscription: keep the slot, publish all zeros. The layout never
    // changes, so a reader's offsets stay valid whether a struct is on or off.
    if (!registration.options.enabled) {
        std::memset(payload, 0, registration.descriptor.size);
        registration.descriptor.publish_count += 1;
        registration.descriptor.last_published_frame = frame_counter;
        registration.descriptor.last_result = static_cast<uint32_t>(SegmentResult::Ok);
        const auto disabled_index = static_cast<size_t>(&registration - registrations_.data());
        if (auto* descriptor = DescriptorAt(disabled_index)) {
            *descriptor = registration.descriptor;
        }
        return true;
    }

    if (registration.options.zero_before_write) {
        std::memset(payload, 0, registration.descriptor.size);
    }

    bool ok = false;
    uint32_t result = static_cast<uint32_t>(SegmentResult::CallbackFailed);
    try {
        ok = registration.writer(payload, registration.descriptor.size, frame_counter);
        result = static_cast<uint32_t>(ok ? SegmentResult::Ok : SegmentResult::CallbackFailed);
    } catch (const std::exception& error) {
        Logger::Instance().LogError(std::string("Shared memory segment exception [") + registration.descriptor.name + "]: " + error.what());
        result = static_cast<uint32_t>(SegmentResult::Exception);
    } catch (...) {
        Logger::Instance().LogError(std::string("Shared memory segment exception [") + registration.descriptor.name + "]: unknown exception");
        result = static_cast<uint32_t>(SegmentResult::Exception);
    }

    if (!ok && registration.options.clear_on_failure && !registration.options.zero_before_write) {
        std::memset(payload, 0, registration.descriptor.size);
    }

    registration.descriptor.publish_count += 1;
    registration.descriptor.last_published_frame = frame_counter;
    registration.descriptor.last_result = result;

    const auto index = static_cast<size_t>(&registration - registrations_.data());
    if (auto* descriptor = DescriptorAt(index)) {
        *descriptor = registration.descriptor;
    }

    return ok;
}

void Manager::BeginWrite() const {
    auto* header = Header();
    if (header) {
        ++header->sequence;
    }
}

void Manager::EndWrite() const {
    auto* header = Header();
    if (header) {
        ++header->sequence;
    }
}

Manager& RuntimeManager() {
    return g_runtime_manager;
}

bool RegisterDefaultSegments(Manager& manager) {
    // Aggregator only. Each subscription lives in its module's
    // <module>_shared_memory.cpp (one commentable line per struct); this just
    // calls them in a fixed order. Layout = subscription order across these.
    RegisterContextSegments(manager);
    RegisterAgentSegments(manager);
    RegisterMapSegments(manager);
    RegisterPartySegments(manager);
    RegisterItemSegments(manager);
    RegisterGuildSegments(manager);
    RegisterTradeSegments(manager);
    RegisterFriendListSegments(manager);
    RegisterCameraSegments(manager);
    RegisterRenderSegments(manager);
    RegisterSkillSegments(manager);
    RegisterQuestSegments(manager);
    return true;
}

}  // namespace GW::shared_memory
