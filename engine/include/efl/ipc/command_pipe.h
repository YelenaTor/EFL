#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

namespace efl {

// A single inbound command from the DevKit (or any other client) addressed at
// the engine's command pipe. Commands are JSON objects of the shape:
//
//   {"type": "<verb>", "payload": { ... }}
//
// The listener parses and enqueues them; the bootstrap drains the queue from
// the main thread inside the per-frame callback.
struct CommandMessage {
    std::string type;
    nlohmann::json payload;
};

// Inbound named-pipe listener. Mirrors the outbound `PipeWriter` surface:
//   - One pipe per process: \\.\pipe\efl-<pid>-cmd
//   - JSON Lines protocol (one command per line)
//   - Background reader thread, main-thread drain
//
// The listener is best-effort. If the pipe cannot be created (e.g. on a non-
// Windows build), `start()` returns false and the engine continues without
// inbound command handling. File-watcher hot-reload still works.
class CommandPipeListener {
public:
    using CommandHandler = std::function<void(const CommandMessage&)>;

    explicit CommandPipeListener(std::string pipeName);
    ~CommandPipeListener();

    CommandPipeListener(const CommandPipeListener&)            = delete;
    CommandPipeListener& operator=(const CommandPipeListener&) = delete;

    // Start listening on the configured pipe. The handler is invoked on the
    // main thread when `drainQueue()` is called, never from the reader thread.
    bool start(CommandHandler handler);

    // Stop the reader thread. Safe to call multiple times.
    void stop();

    // Pop everything that the reader thread has queued and dispatch via the
    // configured handler. Must be called from the main game thread.
    void drainQueue();

    bool isRunning() const { return running_.load(); }

    const std::string& pipeName() const { return pipeName_; }

private:
    void listenThread();

    std::string pipeName_;
    CommandHandler handler_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::queue<CommandMessage> pending_;
    std::mutex queueMutex_;

#ifdef _WIN32
    void* pipeHandle_ = nullptr;
#endif
};

} // namespace efl
