#include "base/error_handling.h"

#include "GW/packet_sniffer/packet_sniffer.h"

#include "base/CrashHandler.h"
#include "base/patterns.h"

namespace GW::packet_sniffer {

bool ResolveCToSSendTarget(uintptr_t* out_target) {
    CrashContextScope context("startup", "packet_sniffer", "resolve_ctos_send_target");
    if (!out_target) {
        return false;
    }

    *out_target = 0;
    PY4GW::Patterns::Resolve("packet_sniffer.ctos_send_target", out_target);
    return *out_target != 0;
}

}  // namespace GW::packet_sniffer
