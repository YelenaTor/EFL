#include "efl/ipc/pipe_writer.h"

#include <chrono>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#endif

namespace efl {

PipeWriter::PipeWriter(const std::string& pipeName)
    : pipeName_(pipeName) {}

PipeWriter::~PipeWriter() {
    close();
}

bool PipeWriter::create() {
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(mutex_);

    pipeHandle_ = CreateNamedPipeA(
        pipeName_.c_str(),
        PIPE_ACCESS_OUTBOUND,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT,
        1,       // max instances
        65536,   // out buffer size (64KB)
        0,       // in buffer size
        0,       // default timeout
        nullptr  // security attributes
    );

    if (pipeHandle_ == INVALID_HANDLE_VALUE) {
        pipeHandle_ = nullptr;
        return false;
    }

    connected_ = true;
    return true;
#else
    return false;
#endif
}

bool PipeWriter::tryReconnect() {
#ifdef _WIN32
    // Rate-limit: only attempt reconnect once per second.
    auto now = std::chrono::steady_clock::now();
    if (now - lastReconnectAttempt_ < std::chrono::seconds(1))
        return false;
    lastReconnectAttempt_ = now;

    // Reset the server side so it can accept a new client.
    DisconnectNamedPipe(pipeHandle_);
    BOOL ok = ConnectNamedPipe(pipeHandle_, nullptr);
    DWORD err = GetLastError();

    if (ok || err == ERROR_PIPE_CONNECTED) {
        connected_ = true;
        disconnectedLogged_ = false;
        return true;
    }
#endif
    return false;
}

void PipeWriter::write(const std::string& msgType, const nlohmann::json& payload) {
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(mutex_);

    if (pipeHandle_ == nullptr)
        return;

    if (!connected_) {
        if (!disconnectedLogged_) {
            // One-time log — subsequent drops are silent until reconnect.
            // (LogService not available here; the disconnect already logged in write() below.)
            disconnectedLogged_ = true;
        }
        if (!tryReconnect())
            return;
    }

    // Build timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf;
    localtime_s(&tm_buf, &time_t_now);

    std::ostringstream ts;
    ts << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count();

    // Build JSON Lines envelope
    nlohmann::json envelope;
    envelope["type"] = msgType;
    envelope["timestamp"] = ts.str();
    envelope["payload"] = payload;

    std::string line = envelope.dump() + "\n";

    DWORD bytesWritten = 0;
    BOOL ok = WriteFile(pipeHandle_, line.data(),
                        static_cast<DWORD>(line.size()),
                        &bytesWritten, nullptr);

    if (!ok) {
        // Write failed — client likely disconnected. Mark for reconnect attempt next write.
        connected_ = false;
        disconnectedLogged_ = false; // allow one-time log to fire again on next drop
    }
#endif
}

void PipeWriter::close() {
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(mutex_);

    if (pipeHandle_ != nullptr) {
        CloseHandle(pipeHandle_);
        pipeHandle_ = nullptr;
    }
    connected_ = false;
#endif
}

bool PipeWriter::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connected_;
}

} // namespace efl
