#pragma once

// Layer A: EFL bootstrap, init/shutdown, version checks

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <unordered_map>
#include <random>
#include "efl/core/log_service.h"
#include "efl/core/diagnostics.h"
#include "efl/core/manifest.h"
#include "efl/core/config_service.h"
#include "efl/core/compatibility_service.h"
#include "efl/core/registry_service.h"
#include "efl/core/event_bus.h"
#include "efl/core/save_service.h"
#include "efl/core/hot_reload.h"
#include "efl/ipc/pipe_writer.h"
#include "efl/ipc/command_pipe.h"
#include "efl/areas/IRoomBackend.h"

#ifndef EFL_STUB_SDK
// Forward declarations for real SDK types and bridge classes
namespace Aurie { struct AurieModule; }
namespace YYTK { struct YYTKInterface; }
namespace efl::bridge {
    class HookRegistry;
    class RoomTracker;
    class RoutineInvoker;
    class InstanceWalker;
}
#endif

namespace efl {

class EflBootstrap {
public:
    EflBootstrap();
    ~EflBootstrap();

    // Stub/test initialization — no bridge layer
    bool initialize(const std::string& contentDir);

#ifndef EFL_STUB_SDK
    // Real initialization — sets up bridge layer with Aurie/YYTK
    bool initialize(const std::string& contentDir,
                    Aurie::AurieModule* module, YYTK::YYTKInterface* yytk);
#endif

    void shutdown();

    LogService& log();
    DiagnosticEmitter& diagnostics();
    PipeWriter& pipe();
    RegistryService& registries();
    EventBus& events();
    SaveService& saves();
    const std::vector<Manifest>& manifests() const;

private:
    LogService log_;
    DiagnosticEmitter diagnostics_;
    std::unique_ptr<PipeWriter> pipe_;
    std::unique_ptr<CommandPipeListener> commandPipe_;
    RegistryService registries_;
    EventBus events_;
    SaveService saves_;
    ConfigService config_;
    std::vector<Manifest> manifests_;
    std::string contentDir_;
    HotReloadWatcher hotReload_;

#ifndef EFL_STUB_SDK
    // Bridge layer (Layer B) — owned by bootstrap, null in stub mode
    std::unique_ptr<bridge::HookRegistry> hooks_;
    std::unique_ptr<bridge::RoomTracker> roomTracker_;
    std::unique_ptr<bridge::RoutineInvoker> routineInvoker_;
    std::unique_ptr<bridge::InstanceWalker> instanceWalker_;

    // Area backend — selected based on manifest settings.areaBackend
    std::unique_ptr<IRoomBackend> roomBackend_;

    // Resource spawn/harvest/respawn state
    uint64_t currentDay_    = 0;  // incremented each new_day hook
    int      currentSeason_ = -1; // 0=spring 1=summer 2=fall 3=winter; -1=unread
    std::unordered_map<std::string, uint64_t> harvestedAt_; // resourceId → day harvested
    std::mt19937 rng_{std::random_device{}()};

    // WorldNpc instance tracking: npcId → GML instance id (double from instance_create_layer)
    std::unordered_map<std::string, double> worldNpcInstanceIds_;

    void stepRegisterHooks();
    void stepConnectAreaRegistry();
    void stepConnectWarpService();

    // Helpers
    void     spawnResourceNode(const ResourceDef& res, const std::string& areaId);
    void     spawnWorldNpc(const WorldNpcDef& def, const std::string& areaId, const std::string& anchorId);
    void     despawnWorldNpc(const std::string& npcId);
    void     despawnAllWorldNpcs();
    int64_t  readGameTime()  const; // calls unified_time@Calendar@Calendar via RoutineInvoker
    int      readSeason()    const; // calls season@Calendar@Calendar via RoutineInvoker
    int      readDayOfSeason() const; // 1..28 derived from unified_time; -1 on unread
    void     fireCalendarEvents(int season, int dayOfSeason);
    void     grantItem(int itemId, int qty); // calls give_item@Ari@Ari
    static uint64_t respawnThresholdDays(const std::string& policy);
#endif

    bool stepVersionCheck();
    bool stepDiscoverManifests(const std::string& contentDir);
    bool stepValidateManifests();
    void stepLoadContent(const std::string& contentDir);
    void stepValidateScriptHooks();
    void reloadContentType(const std::string& contentType,
                           const std::filesystem::path& filePath);
    void emitBootStatus(const std::string& step, const std::string& status,
                        const std::string& detail = "");

    // Inbound command handling (DevKit -> engine over the command pipe).
    void handleCommand(const CommandMessage& cmd);
    void reloadAllContent(const std::string& reason);
};

} // namespace efl
