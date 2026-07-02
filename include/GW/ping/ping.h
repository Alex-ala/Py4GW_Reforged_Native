#pragma once

#include "base/error_handling.h"

#include "base/hook_types.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace GW::ping {

// Rolling ping tracker migrated from the legacy PingTracker (py_pinghandler.h).
// Registers a StoC PING_REPLY callback on construction and keeps a bounded
// history of recent ping values. Scripts create instances directly.
class PingTracker {
public:
    explicit PingTracker(size_t history_size = 10);
    ~PingTracker();

    void Initialize();
    void Terminate();

    uint32_t GetCurrentPing() const;
    uint32_t GetAveragePing() const;
    uint32_t GetMinPing() const;
    uint32_t GetMaxPing() const;

private:
    void OnPingReceived(const void* packet);

    std::vector<uint32_t> ping_history_;
    size_t ping_index_ = 0;
    size_t history_size_;
    size_t ping_count_ = 0;
    bool is_initialized_ = false;
    PY4GW::HookEntry ping_callback_;
};

}  // namespace GW::ping
