#pragma once

#include "base/error_handling.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace GW::packet_sniffer {

// Direction of a captured packet.
enum class PacketDirection : uint8_t {
    StoC = 0,
    CToS = 1,
};

// One captured packet: server tick, direction, header, resolved size, and the
// raw bytes copied out of the live packet buffer.
struct PacketLogEntry {
    uint64_t tick = 0;
    PacketDirection direction = PacketDirection::StoC;
    uint32_t header = 0;
    uint32_t size = 0;
    std::vector<uint8_t> data;
};

// Capture bounds (parity with the legacy PacketSniffer).
constexpr size_t kMaxStoCPacketBuffer = 512;
constexpr uint32_t kMaxReasonableCToSPacketSize = 4096;
constexpr size_t kMaxLogs = 10000;

// Lifecycle. Python-driven and lazy: capture stays off until a consumer calls
// one of these, matching the legacy sniffer. Deliberately not wired into the
// GW::Initialize step table because auto-starting capture on every injection
// would change runtime behavior (see packet_sniffer.cpp notes).
bool Initialize();
bool InitializeStoC();
bool InitializeCToS();

void Terminate();
void TerminateStoC();
void TerminateCToS();

// Public callable surface (bodies in packet_sniffer_methods.cpp).
void LogPacket(PacketLogEntry entry);
std::vector<PacketLogEntry> GetLogs();
std::vector<PacketLogEntry> GetLogsByDirection(PacketDirection direction);
void ClearLogs();
void ClearLogsByDirection(PacketDirection direction);

// Outbound send wrapper: void __cdecl SendPacket(void* ctx, uint32_t size, const void* packet).
using SendPacketFn = void(__cdecl*)(void* ctx, uint32_t size, const void* packet);

// Module-owned resolver for the CToS send wrapper. Definition lives in
// packet_sniffer_patterns.cpp; it resolves "packet_sniffer.ctos_send_target"
// (primary byte pattern with two assertion fallbacks). Returns false and
// leaves *out_target at 0 when no attempt resolves.
bool ResolveCToSSendTarget(uintptr_t* out_target);

}  // namespace GW::packet_sniffer
