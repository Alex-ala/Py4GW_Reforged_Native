#include "GW/shared_memory/manager.h"

#include "GW/agent/agent.h"
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

template <typename T, typename Getter>
bool CopyContextSnapshot(T& snapshot, Getter getter) {
    // Legacy parity guard (SharedMemory.cpp is_map_ready): never deep-copy a
    // game context while a map transition is in flight. During load the game
    // thread rebuilds the context tree concurrently with this (runtime-thread)
    // copy, and a partially built CharContext produced a reproducible
    // 0xC0000005 in the memcpy below (crash sidecars 2026-07-02).
    const auto instance_type = map::GetInstanceType();
    const bool is_map_ready = map::GetIsMapLoaded() &&
        !map::GetIsObserving() &&
        instance_type != Constants::InstanceType::Loading;
    if (!is_map_ready) {
        return false;
    }
    const auto* source = getter();
    if (!source) {
        return false;
    }
    std::memcpy(&snapshot, source, sizeof(T));
    return true;
}

bool FillRuntimePointersSnapshot(Context::RuntimePointersSnapshot& snapshot, uint64_t) {
    snapshot = {};
    snapshot.mission_map_context = reinterpret_cast<uintptr_t>(Context::GetMissionMapContext());
    snapshot.world_map_context = reinterpret_cast<uintptr_t>(Context::GetWorldMapContext());
    snapshot.gameplay_context = reinterpret_cast<uintptr_t>(Context::GetGameplayContext());
    snapshot.instance_info = Context::GetInstanceInfoPtr();
    snapshot.map_context = reinterpret_cast<uintptr_t>(Context::GetMapContext());
    snapshot.game_context = reinterpret_cast<uintptr_t>(Context::GetGameContext());
    snapshot.pregame_context = reinterpret_cast<uintptr_t>(Context::GetPreGameContext());
    snapshot.world_context = reinterpret_cast<uintptr_t>(Context::GetWorldContext());
    snapshot.char_context = reinterpret_cast<uintptr_t>(Context::GetCharContext());
    snapshot.agent_context = reinterpret_cast<uintptr_t>(Context::GetAgentContext());
    snapshot.guild_context = reinterpret_cast<uintptr_t>(Context::GetGuildContext());
    snapshot.party_context = reinterpret_cast<uintptr_t>(Context::GetPartyContext());
    snapshot.trade_context = reinterpret_cast<uintptr_t>(Context::GetTradeContext());
    snapshot.item_context = reinterpret_cast<uintptr_t>(Context::GetItemContext());
    snapshot.friend_list = reinterpret_cast<uintptr_t>(Context::GetFriendList());
    snapshot.render_context = reinterpret_cast<uintptr_t>(Context::GetRenderContext());
    snapshot.text_parser = reinterpret_cast<uintptr_t>(Context::GetTextParser());
    snapshot.camera = reinterpret_cast<uintptr_t>(Context::GetCamera());
    snapshot.window_handle_ptr = Context::GetWindowHandlePtrAddress();
    return true;
}

bool FillAgentArraySnapshot(Context::AgentArraySnapshot& snapshot, uint64_t) {
    snapshot = {};
    snapshot.max_size = Context::kSharedMemoryAgentArrayMaxSize;

    const auto instance_type = map::GetInstanceType();
    const bool is_map_ready = map::GetIsMapLoaded() &&
        !map::GetIsObserving() &&
        instance_type != Constants::InstanceType::Loading;
    if (!is_map_ready) {
        return false;
    }

    auto* agents = Context::GetAgentArray();
    const auto* agent_context = Context::GetAgentContext();
    if (!(agents && agent_context)) {
        return false;
    }

    auto push_ref = [](Context::AgentRefSnapshotArray& array, uint32_t agent_id, uint32_t index) {
        if (array.count >= Context::kSharedMemoryAgentArrayMaxSize) {
            return;
        }
        auto& entry = array.entries[array.count++];
        entry.agent_id = agent_id;
        entry.index = index;
    };

    for (const auto* agent : *agents) {
        if (!(agent && agent->agent_id)) {
            continue;
        }
        if (!(agent_context->agent_movement.size() > agent->agent_id &&
              agent_context->agent_movement[agent->agent_id])) {
            continue;
        }
        if (snapshot.count >= Context::kSharedMemoryAgentArrayMaxSize) {
            break;
        }

        const uint32_t slot = snapshot.count++;
        auto& out = snapshot.entries[slot];
        out.ptr = reinterpret_cast<uintptr_t>(agent);
        out.agent_id = agent->agent_id;

        push_ref(snapshot.all, agent->agent_id, slot);

        if (agent->GetIsGadgetType()) {
            push_ref(snapshot.gadget, agent->agent_id, slot);
            continue;
        }

        if (agent->GetIsItemType()) {
            const auto* item = agent->GetAsAgentItem();
            if (!item) {
                continue;
            }
            push_ref(snapshot.item, agent->agent_id, slot);
            if (item->owner != 0) {
                push_ref(snapshot.owned_item, agent->agent_id, slot);
            }
            continue;
        }

        const auto* living = agent->GetAsAgentLiving();
        if (!living) {
            continue;
        }

        push_ref(snapshot.living, agent->agent_id, slot);

        switch (living->allegiance) {
        case Constants::Allegiance::Ally_NonAttackable:
            push_ref(snapshot.ally, agent->agent_id, slot);
            if (living->GetIsDead()) {
                push_ref(snapshot.dead_ally, agent->agent_id, slot);
            }
            break;
        case Constants::Allegiance::Neutral:
            push_ref(snapshot.neutral, agent->agent_id, slot);
            break;
        case Constants::Allegiance::Enemy:
            push_ref(snapshot.enemy, agent->agent_id, slot);
            if (living->GetIsDead()) {
                push_ref(snapshot.dead_enemy, agent->agent_id, slot);
            }
            break;
        case Constants::Allegiance::Spirit_Pet:
            push_ref(snapshot.spirit_pet, agent->agent_id, slot);
            break;
        case Constants::Allegiance::Minion:
            push_ref(snapshot.minion, agent->agent_id, slot);
            break;
        case Constants::Allegiance::Npc_Minipet:
            push_ref(snapshot.npc_minipet, agent->agent_id, slot);
            break;
        default:
            break;
        }
    }

    return true;
}

bool FillMapContextSnapshot(Context::MapContextSnapshot& snapshot, uint64_t) {
    snapshot = {};

    const auto* map_context = Context::GetMapContext();
    if (!map_context) {
        return false;
    }

    std::memcpy(snapshot.map_boundaries, map_context->map_boundaries, sizeof(snapshot.map_boundaries));
    std::memcpy(snapshot.trapezoid_bounds, map_context->h005C, sizeof(snapshot.trapezoid_bounds));

    snapshot.terrain = reinterpret_cast<uintptr_t>(map_context->terrain);
    snapshot.zones = reinterpret_cast<uintptr_t>(map_context->zones);
    snapshot.props = reinterpret_cast<uintptr_t>(map_context->props);
    snapshot.pathing_sub1 = reinterpret_cast<uintptr_t>(map_context->sub1);

    if (map_context->sub1) {
        snapshot.pathing_sub2 = reinterpret_cast<uintptr_t>(map_context->sub1->sub2);
        snapshot.pathing_block_count = static_cast<uint32_t>(map_context->sub1->pathing_map_block.size());
        snapshot.total_trapezoid_count = map_context->sub1->total_trapezoid_count;
        if (map_context->sub1->sub2) {
            snapshot.pathing_map_count = static_cast<uint32_t>(map_context->sub1->sub2->pmaps.size());
        }
    }

    if (map_context->props) {
        snapshot.prop_model_count = static_cast<uint32_t>(map_context->props->propModels.size());
        snapshot.prop_array_count = static_cast<uint32_t>(map_context->props->propArray.size());
    }

    snapshot.instance_info = Context::GetInstanceInfoPtr();
    if (const auto* instance_info = Context::GetInstanceInfo()) {
        snapshot.instance_type = static_cast<uint32_t>(instance_info->instance_type);
        snapshot.current_map_info = reinterpret_cast<uintptr_t>(instance_info->current_map_info);
        snapshot.terrain_count = instance_info->terrain_count;
    }

    return true;
}

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
    const bool pointers_ok = manager.RegisterStruct<Context::RuntimePointersSnapshot>(
        "runtime.pointers",
        [](Context::RuntimePointersSnapshot& value, uint64_t frame_counter) {
            return FillRuntimePointersSnapshot(value, frame_counter);
        });
    const bool agents_ok = manager.RegisterStruct<Context::AgentArraySnapshot>(
        "runtime.agents",
        [](Context::AgentArraySnapshot& value, uint64_t frame_counter) {
            return FillAgentArraySnapshot(value, frame_counter);
        });
    const bool map_summary_ok = manager.RegisterStruct<Context::MapContextSnapshot>(
        "runtime.map.summary",
        [](Context::MapContextSnapshot& value, uint64_t frame_counter) {
            return FillMapContextSnapshot(value, frame_counter);
        });
    const bool game_ok = manager.RegisterStruct<Context::GameContext>(
        "runtime.ctx.game",
        [](Context::GameContext& value, uint64_t) {
            return CopyContextSnapshot(value, Context::GetGameContext);
        });
    const bool pregame_ok = manager.RegisterStruct<Context::PreGameContext>(
        "runtime.ctx.pregame",
        [](Context::PreGameContext& value, uint64_t) {
            return CopyContextSnapshot(value, Context::GetPreGameContext);
        });
    const bool world_ok = manager.RegisterStruct<Context::WorldContext>(
        "runtime.ctx.world",
        [](Context::WorldContext& value, uint64_t) {
            return CopyContextSnapshot(value, Context::GetWorldContext);
        });
    const bool party_ok = manager.RegisterStruct<Context::PartyContext>(
        "runtime.ctx.party",
        [](Context::PartyContext& value, uint64_t) {
            return CopyContextSnapshot(value, Context::GetPartyContext);
        });
    const bool char_ok = manager.RegisterStruct<Context::CharContext>(
        "runtime.ctx.char",
        [](Context::CharContext& value, uint64_t) {
            return CopyContextSnapshot(value, Context::GetCharContext);
        });
    const bool guild_ok = manager.RegisterStruct<Context::GuildContext>(
        "runtime.ctx.guild",
        [](Context::GuildContext& value, uint64_t) {
            return CopyContextSnapshot(value, Context::GetGuildContext);
        });
    const bool item_ok = manager.RegisterStruct<Context::ItemContext>(
        "runtime.ctx.item",
        [](Context::ItemContext& value, uint64_t) {
            return CopyContextSnapshot(value, Context::GetItemContext);
        });
    const bool agent_ctx_ok = manager.RegisterStruct<Context::AgentContext>(
        "runtime.ctx.agent",
        [](Context::AgentContext& value, uint64_t) {
            return CopyContextSnapshot(value, Context::GetAgentContext);
        });
    const bool map_ok = manager.RegisterStruct<Context::MapContext>(
        "runtime.ctx.map",
        [](Context::MapContext& value, uint64_t) {
            return CopyContextSnapshot(value, Context::GetMapContext);
        });
    const bool account_ok = manager.RegisterStruct<Context::AccountContext>(
        "runtime.ctx.account",
        [](Context::AccountContext& value, uint64_t) {
            return CopyContextSnapshot(value, Context::GetAccountContext);
        });
    const bool trade_ok = manager.RegisterStruct<Context::TradeContext>(
        "runtime.ctx.trade",
        [](Context::TradeContext& value, uint64_t) {
            return CopyContextSnapshot(value, Context::GetTradeContext);
        });
    const bool gameplay_ok = manager.RegisterStruct<Context::GameplayContext>(
        "runtime.ctx.gameplay",
        [](Context::GameplayContext& value, uint64_t) {
            return CopyContextSnapshot(value, Context::GetGameplayContext);
        });
    const bool mission_map_ok = manager.RegisterStruct<Context::MissionMapContext>(
        "runtime.ctx.mission_map",
        [](Context::MissionMapContext& value, uint64_t) {
            return CopyContextSnapshot(value, Context::GetMissionMapContext);
        });
    const bool world_map_ok = manager.RegisterStruct<Context::WorldMapContext>(
        "runtime.ctx.world_map",
        [](Context::WorldMapContext& value, uint64_t) {
            return CopyContextSnapshot(value, Context::GetWorldMapContext);
        });
    const bool render_ok = manager.RegisterStruct<Context::GwDxContext>(
        "runtime.ctx.render",
        [](Context::GwDxContext& value, uint64_t) {
            return CopyContextSnapshot(value, Context::GetRenderContext);
        });
    const bool camera_ok = manager.RegisterStruct<Context::Camera>(
        "runtime.ctx.camera",
        [](Context::Camera& value, uint64_t) {
            return CopyContextSnapshot(value, Context::GetCamera);
        });
    const bool friends_ok = manager.RegisterStruct<Context::FriendList>(
        "runtime.ctx.friends",
        [](Context::FriendList& value, uint64_t) {
            return CopyContextSnapshot(value, Context::GetFriendList);
        });
    const bool text_parser_ok = manager.RegisterStruct<Context::TextParser>(
        "runtime.ctx.text_parser",
        [](Context::TextParser& value, uint64_t) {
            return CopyContextSnapshot(value, Context::GetTextParser);
        });
    return pointers_ok &&
        agents_ok &&
        map_summary_ok &&
        game_ok &&
        pregame_ok &&
        world_ok &&
        party_ok &&
        char_ok &&
        guild_ok &&
        item_ok &&
        agent_ctx_ok &&
        map_ok &&
        account_ok &&
        trade_ok &&
        gameplay_ok &&
        mission_map_ok &&
        world_map_ok &&
        render_ok &&
        camera_ok &&
        friends_ok &&
        text_parser_ok;
}

}  // namespace GW::shared_memory
