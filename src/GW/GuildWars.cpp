#include "base/error_handling.h"

#include "GW/GuildWars.h"

#include "GW/render/render.h"

namespace gw {

bool Initialize() {
    return render::Initialize();
}

void Shutdown() {
    render::Shutdown();
}

}  // namespace gw
