#include "efl/core/hot_reload.h"

#include <chrono>

#ifndef EFL_STUB_SDK
#include <windows.h>
#endif

namespace efl {

HotReloadWatcher::~HotReloadWatcher() {
    stop();
}

bool HotReloadWatcher::start(const std::filesystem::path& watchDir, ReloadCallback callback) {
    if (running_.load())
        return true;

    watchDir_ = watchDir;
    callback_ = std::move(callback);

#ifndef EFL_STUB_SDK
    HANDLE h = CreateFileW(
        watchDir_.wstring().c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr
    );

    if (h == INVALID_HANDLE_VALUE)
        return false;

    dirHandle_ = h;
#endif

    running_.store(true);
    thread_ = std::thread(&HotReloadWatcher::watchThread, this);
    return true;
}

void HotReloadWatcher::stop() {
    if (!running_.load())
        return;

    running_.store(false);

#ifndef EFL_STUB_SDK
    if (static_cast<HANDLE>(dirHandle_) != INVALID_HANDLE_VALUE) {
        CloseHandle(static_cast<HANDLE>(dirHandle_));
        dirHandle_ = INVALID_HANDLE_VALUE;
    }
#endif

    if (thread_.joinable())
        thread_.join();
}

void HotReloadWatcher::drainQueue() {
    std::queue<ReloadEvent> local;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        std::swap(local, pendingReloads_);
    }
    while (!local.empty()) {
        if (callback_)
            callback_(local.front());
        local.pop();
    }
}

void HotReloadWatcher::watchThread() {
#ifndef EFL_STUB_SDK
    alignas(DWORD) char buf[4096];
    HANDLE h = static_cast<HANDLE>(dirHandle_);

    while (running_.load()) {
        DWORD bytesReturned = 0;
        BOOL ok = ReadDirectoryChangesW(
            h,
            buf,
            sizeof(buf),
            TRUE, // recursive
            FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytesReturned,
            nullptr,
            nullptr
        );

        if (!ok || bytesReturned == 0) {
            // Handle was closed (stop() called) or error — exit loop
            break;
        }

        DWORD offset = 0;
        while (offset < bytesReturned) {
            auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buf + offset);

            std::wstring wname(info->FileName, info->FileNameLength / sizeof(WCHAR));
            std::filesystem::path relPath(wname);

            if (relPath.extension() == L".json") {
                std::filesystem::path fullPath = watchDir_ / relPath;
                // contentType is the immediate parent dir name: areas/crystal_cave.json -> "areas"
                std::string contentType;
                std::filesystem::path parent = relPath.parent_path();
                if (!parent.empty() && parent != relPath) {
                    auto it = parent.begin();
                    if (it != parent.end())
                        contentType = it->string();
                }

                if (!contentType.empty()) {
                    std::lock_guard<std::mutex> lock(queueMutex_);
                    pendingReloads_.push({contentType, fullPath});
                }
            }

            if (info->NextEntryOffset == 0)
                break;
            offset += info->NextEntryOffset;
        }
    }
#else
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
#endif
}

} // namespace efl
