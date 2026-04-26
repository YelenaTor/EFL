#pragma once

#include <chrono>
#include <string>
#include <mutex>
#include <nlohmann/json.hpp>

namespace efl {

class PipeWriter {
public:
    explicit PipeWriter(const std::string& pipeName);
    ~PipeWriter();

    bool create();
    void write(const std::string& msgType, const nlohmann::json& payload);
    void close();
    bool isConnected() const;

private:
    std::string pipeName_;
    void* pipeHandle_ = nullptr;
    mutable std::mutex mutex_;
    bool connected_ = false;
    bool disconnectedLogged_ = false; // one-time log on first drop
    std::chrono::steady_clock::time_point lastReconnectAttempt_{};

    // Attempt to accept a new client connection. Called from write() when disconnected.
    // Rate-limited to once per second to avoid hammering the pipe handle.
    // Returns true if a client connected.
    bool tryReconnect();
};

} // namespace efl
