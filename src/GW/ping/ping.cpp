#include "base/error_handling.h"

#include "GW/ping/ping.h"

#include "GW/common/opcodes.h"
#include "GW/common/stoc.h"
#include "GW/stoc/stoc.h"

#include <algorithm>

namespace GW::ping {

PingTracker::PingTracker(size_t history_size)
    : ping_history_(history_size, 0), history_size_(history_size) {
    Initialize();
}

PingTracker::~PingTracker() {
    Terminate();
}

void PingTracker::Initialize() {
    if (is_initialized_) {
        return;
    }
    GW::StoC::RegisterPacketCallback(
        &ping_callback_,
        GAME_SMSG_PING_REPLY,
        [this](PY4GW::HookStatus*, Packet::StoC::PacketBase* packet) {
            OnPingReceived(packet);
        });
    is_initialized_ = true;
}

void PingTracker::Terminate() {
    if (!is_initialized_) {
        return;
    }
    GW::StoC::RemoveCallback(GAME_SMSG_PING_REPLY, &ping_callback_);
    is_initialized_ = false;
}

void PingTracker::OnPingReceived(const void* packet) {
    // Parity with legacy: the reply payload is a raw uint32 array; the ping
    // value sits at word index 1 (index 0 is the header).
    const auto* packet_data = static_cast<const uint32_t*>(packet);
    const uint32_t ping = packet_data[1];

    if (ping == 0 || ping > 4999) {
        return;  // Ignore invalid pings
    }

    ping_history_[ping_index_] = ping;
    ping_index_ = (ping_index_ + 1) % history_size_;
    if (ping_count_ < history_size_) {
        ping_count_++;
    }
}

uint32_t PingTracker::GetCurrentPing() const {
    if (ping_count_ == 0) {
        return 0;
    }
    return ping_history_[(ping_index_ + history_size_ - 1) % history_size_];
}

uint32_t PingTracker::GetAveragePing() const {
    if (ping_count_ == 0) {
        return 0;
    }
    uint32_t sum = 0;
    for (size_t i = 0; i < ping_count_; ++i) {
        sum += ping_history_[i];
    }
    return sum / ping_count_;
}

uint32_t PingTracker::GetMinPing() const {
    if (ping_count_ == 0) {
        return 0;
    }
    return *std::min_element(ping_history_.begin(), ping_history_.begin() + ping_count_);
}

uint32_t PingTracker::GetMaxPing() const {
    if (ping_count_ == 0) {
        return 0;
    }
    return *std::max_element(ping_history_.begin(), ping_history_.begin() + ping_count_);
}

}  // namespace GW::ping
