#include "base/error_handling.h"

#include "GW/GuildWars.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/memory_manager.h"
#include "base/memory_patcher.h"
#include "GW/camera/camera.h"
#include "GW/context/context.h"
#include "GW/effects/effects.h"
#include "GW/game_thread/game_thread.h"
#include "GW/render/render.h"
#include "GW/ui/ui.h"

namespace gw {

bool Initialize() {
    CrashHandler::SetContext("startup", "game_thread", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing game_thread.");
    PY4GW_ASSERT(game_thread::Initialize());

    CrashHandler::SetContext("startup", "render", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing render.");
    if (!render::Initialize()) {
        Logger::Instance().LogError("[gw] render initialization failed.");
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "ui", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing ui.");
    if (!ui::Initialize()) {
        Logger::Instance().LogError("[gw] ui initialization failed.");
        render::Shutdown();
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "camera", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing camera.");
    if (!camera::Initialize()) {
        Logger::Instance().LogError("[gw] camera initialization failed.");
        ui::Shutdown();
        render::Shutdown();
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "memory_manager", "scan");
    Logger::Instance().LogInfo("[gw] Scanning memory manager.");
    if (!py4gw::MemoryManager::Scan()) {
        Logger::Instance().LogError("[gw] memory manager scan failed.");
        ui::Shutdown();
        camera::Shutdown();
        render::Shutdown();
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "context", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing context.");
    if (!context::Initialize()) {
        Logger::Instance().LogError("[gw] context initialization failed.");
        ui::Shutdown();
        camera::Shutdown();
        render::Shutdown();
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "effects", "initialize");
    Logger::Instance().LogInfo("[gw] Initializing effects.");
    if (!effects::Initialize()) {
        Logger::Instance().LogError("[gw] effects initialization failed.");
        context::Shutdown();
        ui::Shutdown();
        camera::Shutdown();
        render::Shutdown();
        game_thread::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "memory_patcher", "enable_hooks");
    Logger::Instance().LogInfo("[gw] Enabling memory patcher hooks.");
    py4gw::MemoryPatcher::EnableHooks();
    CrashHandler::SetContext("runtime", "gw", "initialized");
    Logger::Instance().LogInfo("[gw] Guild Wars initialization complete.");
    return true;
}

void Shutdown() {
    CrashHandler::SetContext("shutdown", "effects", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down effects.");
    effects::Shutdown();
    CrashHandler::SetContext("shutdown", "context", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down context.");
    context::Shutdown();
    CrashHandler::SetContext("shutdown", "render", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down render.");
    render::Shutdown();
    CrashHandler::SetContext("shutdown", "ui", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down ui.");
    ui::Shutdown();
    CrashHandler::SetContext("shutdown", "camera", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down camera.");
    camera::Shutdown();
    CrashHandler::SetContext("shutdown", "memory_patcher", "disable_hooks");
    Logger::Instance().LogInfo("[gw] Disabling memory patcher hooks.");
    py4gw::MemoryPatcher::DisableHooks();
    CrashHandler::SetContext("shutdown", "game_thread", "shutdown");
    Logger::Instance().LogInfo("[gw] Shutting down game_thread.");
    game_thread::Shutdown();
    CrashHandler::SetContext("shutdown", "gw", "shutdown_complete");
}

}  // namespace gw
