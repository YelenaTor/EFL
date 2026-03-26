#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>

namespace efl {

struct ReloadEvent {
    std::string contentType; // "areas", "npcs", "warps", etc.
    std::filesystem::path filePath;
};

class HotReloadWatcher {
public:
    using ReloadCallback = std::function<void(const ReloadEvent&)>;

    HotReloadWatcher() = default;
    ~HotReloadWatcher();

    bool start(const std::filesystem::path& watchDir, ReloadCallback callback);
    void stop();
    void drainQueue(); // called from game frame callback to process pending reloads on main thread

private:
    void watchThread();
    std::filesystem::path watchDir_;
    ReloadCallback callback_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::queue<ReloadEvent> pendingReloads_;
    std::mutex queueMutex_;

#ifndef EFL_STUB_SDK
    void* dirHandle_ = nullptr; // HANDLE, stored as void* to keep <windows.h> out of header
#endif
};

} // namespace efl
