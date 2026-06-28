#include "base/error_handling.h"

#include "GW/GuildWars.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/memory_manager.h"
#include "base/memory_patcher.h"
#include "GW/camera/camera.h"
#include "GW/context/context.h"
#include "GW/effects/effects.h"
#include "GW/events/events.h"
#include "GW/friend_list/friend_list.h"
#include "GW/game_thread/game_thread.h"
#include "GW/guild/guild.h"
#include "GW/map/map.h"
#include "GW/player/player.h"
#include "GW/quest/quest.h"
#include "GW/render/render.h"
#include "GW/stoc/stoc.h"
#include "GW/ui/ui.h"

namespace GW {

bool Initialize() {
    CrashHandler::SetContext("startup", "game_thread", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing game_thread.");
    PY4GW_ASSERT(game_thread::Initialize());

    CrashHandler::SetContext("startup", "stoc", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing stoc.");
    if (!StoC::Initialize()) {
        Logger::Instance().LogError("[gw] stoc initialization failed.");
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "render", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing render.");
    if (!render::Initialize()) {
        Logger::Instance().LogError("[gw] render initialization failed.");
        StoC::Shutdown();
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "ui", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing ui.");
    if (!ui::Initialize()) {
        Logger::Instance().LogError("[gw] ui initialization failed.");
        render::Shutdown();
        StoC::Shutdown();
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "camera", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing camera.");
    if (!camera::Initialize()) {
        Logger::Instance().LogError("[gw] camera initialization failed.");
        ui::Shutdown();
        render::Shutdown();
        StoC::Shutdown();
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "memory_manager", "scan");
    Logger::Instance().LogInfo("[gw] Scanning memory manager.");
    if (!PY4GW::MemoryManager::Scan()) {
        Logger::Instance().LogError("[gw] memory manager scan failed.");
        ui::Shutdown();
        camera::Shutdown();
        render::Shutdown();
        StoC::Shutdown();
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "context", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing context.");
    if (!Context::Initialize()) {
        Logger::Instance().LogError("[gw] context initialization failed.");
        ui::Shutdown();
        camera::Shutdown();
        render::Shutdown();
        StoC::Shutdown();
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "effects", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing effects.");
    if (!effects::Initialize()) {
        Logger::Instance().LogError("[gw] effects initialization failed.");
        Context::Shutdown();
        ui::Shutdown();
        camera::Shutdown();
        render::Shutdown();
        StoC::Shutdown();
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "events", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing events.");
    if (!events::Initialize()) {
        Logger::Instance().LogError("[gw] events initialization failed.");
        effects::Shutdown();
        Context::Shutdown();
        ui::Shutdown();
        camera::Shutdown();
        render::Shutdown();
        StoC::Shutdown();
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "friend_list", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing friend_list.");
    if (!friend_list::Initialize()) {
        Logger::Instance().LogError("[gw] friend_list initialization failed.");
        events::Shutdown();
        effects::Shutdown();
        Context::Shutdown();
        ui::Shutdown();
        camera::Shutdown();
        render::Shutdown();
        StoC::Shutdown();
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "player", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing player.");
    if (!player::Initialize()) {
        Logger::Instance().LogError("[gw] player initialization failed.");
        friend_list::Shutdown();
        events::Shutdown();
        effects::Shutdown();
        Context::Shutdown();
        ui::Shutdown();
        camera::Shutdown();
        render::Shutdown();
        StoC::Shutdown();
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "quest", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing quest.");
    if (!quest::Initialize()) {
        Logger::Instance().LogError("[gw] quest initialization failed.");
        player::Shutdown();
        friend_list::Shutdown();
        events::Shutdown();
        effects::Shutdown();
        Context::Shutdown();
        ui::Shutdown();
        camera::Shutdown();
        render::Shutdown();
        StoC::Shutdown();
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "map", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing map.");
    if (!map::Initialize()) {
        Logger::Instance().LogError("[gw] map initialization failed.");
        quest::Shutdown();
        player::Shutdown();
        friend_list::Shutdown();
        events::Shutdown();
        effects::Shutdown();
        Context::Shutdown();
        ui::Shutdown();
        camera::Shutdown();
        render::Shutdown();
        StoC::Shutdown();
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "guild", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing guild.");
    if (!guild::Initialize()) {
        Logger::Instance().LogError("[gw] guild initialization failed.");
        map::Shutdown();
        quest::Shutdown();
        player::Shutdown();
        friend_list::Shutdown();
        events::Shutdown();
        effects::Shutdown();
        Context::Shutdown();
        ui::Shutdown();
        camera::Shutdown();
        render::Shutdown();
        StoC::Shutdown();
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "memory_patcher", "enable_hooks");
    Logger::Instance().LogInfo("[gw] Enabling memory patcher hooks.");
    PY4GW::MemoryPatcher::EnableHooks();
    CrashHandler::SetContext("runtime", "gw", "initialized");
    Logger::Instance().LogInfo("[gw] Guild Wars initialization complete.");
    return true;
}

void Shutdown() {
    CrashHandler::SetContext("shutdown", "guild", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down guild.");
    guild::Shutdown();
    CrashHandler::SetContext("shutdown", "map", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down map.");
    map::Shutdown();
    CrashHandler::SetContext("shutdown", "quest", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down quest.");
    quest::Shutdown();
    CrashHandler::SetContext("shutdown", "player", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down player.");
    player::Shutdown();
    CrashHandler::SetContext("shutdown", "events", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down events.");
    events::Shutdown();
    CrashHandler::SetContext("shutdown", "friend_list", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down friend_list.");
    friend_list::Shutdown();
    CrashHandler::SetContext("shutdown", "effects", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down effects.");
    effects::Shutdown();
    CrashHandler::SetContext("shutdown", "context", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down context.");
    Context::Shutdown();
    CrashHandler::SetContext("shutdown", "render", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down render.");
    render::Shutdown();
    CrashHandler::SetContext("shutdown", "stoc", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down stoc.");
    StoC::Shutdown();
    CrashHandler::SetContext("shutdown", "ui", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down ui.");
    ui::Shutdown();
    CrashHandler::SetContext("shutdown", "camera", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down camera.");
    camera::Shutdown();
    CrashHandler::SetContext("shutdown", "memory_patcher", "disable_hooks");
    Logger::Instance().LogInfo("[gw] Disabling memory patcher hooks.");
    PY4GW::MemoryPatcher::DisableHooks();
    CrashHandler::SetContext("shutdown", "game_thread", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down game_thread.");
    game_thread::Shutdown();
    CrashHandler::SetContext("shutdown", "gw", "shutdown_complete");
}

}  // namespace GW
