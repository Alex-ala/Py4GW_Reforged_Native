#include "base/error_handling.h"

#include "GW/packet_sniffer/packet_sniffer.h"

#include <algorithm>
#include <mutex>
#include <vector>

namespace GW::packet_sniffer {

// Shared log buffer. Defined here and consumed by the CToS detour and StoC
// callback in packet_sniffer.cpp via LogPacket.
std::mutex g_mutex;
std::vector<PacketLogEntry> g_logs;

void LogPacket(PacketLogEntry entry) {
    std::scoped_lock lock(g_mutex);
    g_logs.push_back(std::move(entry));
    while (g_logs.size() > kMaxLogs) {
        g_logs.erase(g_logs.begin());
    }
}

std::vector<PacketLogEntry> GetLogs() {
    std::scoped_lock lock(g_mutex);
    return g_logs;
}

std::vector<PacketLogEntry> GetLogsByDirection(PacketDirection direction) {
    std::scoped_lock lock(g_mutex);
    std::vector<PacketLogEntry> filtered;
    filtered.reserve(g_logs.size());
    for (const auto& entry : g_logs) {
        if (entry.direction == direction) {
            filtered.push_back(entry);
        }
    }
    return filtered;
}

void ClearLogs() {
    std::scoped_lock lock(g_mutex);
    g_logs.clear();
}

void ClearLogsByDirection(PacketDirection direction) {
    std::scoped_lock lock(g_mutex);
    g_logs.erase(
        std::remove_if(
            g_logs.begin(),
            g_logs.end(),
            [direction](const PacketLogEntry& entry) {
                return entry.direction == direction;
            }),
        g_logs.end());
}

}  // namespace GW::packet_sniffer
