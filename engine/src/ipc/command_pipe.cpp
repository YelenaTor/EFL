#include "efl/ipc/command_pipe.h"

#include <chrono>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace efl {

CommandPipeListener::CommandPipeListener(std::string pipeName)
    : pipeName_(std::move(pipeName)) {}

CommandPipeListener::~CommandPipeListener() {
    stop();
}

bool CommandPipeListener::start(CommandHandler handler) {
    if (running_.load()) {
        return true;
    }

#ifdef _WIN32
    pipeHandle_ = CreateNamedPipeA(
        pipeName_.c_str(),
        PIPE_ACCESS_INBOUND,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,
        0,
        4096,
        0,
        nullptr);

    if (pipeHandle_ == INVALID_HANDLE_VALUE) {
        pipeHandle_ = nullptr;
        return false;
    }

    handler_ = std::move(handler);
    running_.store(true);
    thread_ = std::thread(&CommandPipeListener::listenThread, this);
    return true;
#else
    (void)handler;
    return false;
#endif
}

void CommandPipeListener::stop() {
    if (!running_.load()) {
#ifdef _WIN32
        if (pipeHandle_ != nullptr) {
            CloseHandle(static_cast<HANDLE>(pipeHandle_));
            pipeHandle_ = nullptr;
        }
#endif
        return;
    }

    running_.store(false);

#ifdef _WIN32
    if (pipeHandle_ != nullptr) {
        // Break the blocking ConnectNamedPipe / ReadFile by closing the handle.
        CancelIoEx(static_cast<HANDLE>(pipeHandle_), nullptr);
        CloseHandle(static_cast<HANDLE>(pipeHandle_));
        pipeHandle_ = nullptr;
    }
#endif

    if (thread_.joinable()) {
        thread_.join();
    }
}

void CommandPipeListener::drainQueue() {
    std::queue<CommandMessage> local;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        std::swap(local, pending_);
    }
    while (!local.empty()) {
        if (handler_) {
            handler_(local.front());
        }
        local.pop();
    }
}

void CommandPipeListener::listenThread() {
#ifdef _WIN32
    HANDLE h = static_cast<HANDLE>(pipeHandle_);
    std::vector<char> readBuffer;
    std::string lineBuffer;

    while (running_.load()) {
        // Wait for a client to connect. ConnectNamedPipe returns immediately
        // with ERROR_PIPE_CONNECTED if the client connected before this call.
        BOOL connected = ConnectNamedPipe(h, nullptr);
        if (!connected) {
            DWORD err = GetLastError();
            if (err != ERROR_PIPE_CONNECTED) {
                if (!running_.load()) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }

        // Read JSON Lines until the client disconnects.
        char chunk[1024];
        DWORD bytesRead = 0;
        while (running_.load()) {
            BOOL ok = ReadFile(h, chunk, sizeof(chunk), &bytesRead, nullptr);
            if (!ok || bytesRead == 0) {
                break;
            }
            for (DWORD i = 0; i < bytesRead; ++i) {
                char ch = chunk[i];
                if (ch == '\r') {
                    continue;
                }
                if (ch == '\n') {
                    if (!lineBuffer.empty()) {
                        try {
                            auto parsed = nlohmann::json::parse(lineBuffer);
                            CommandMessage msg;
                            msg.type    = parsed.value("type", "");
                            msg.payload = parsed.value("payload", nlohmann::json::object());
                            if (!msg.type.empty()) {
                                std::lock_guard<std::mutex> lock(queueMutex_);
                                pending_.push(std::move(msg));
                            }
                        } catch (const std::exception&) {
                            // Drop malformed lines silently; emitting from the
                            // worker thread would race against bootstrap state.
                        }
                        lineBuffer.clear();
                    }
                } else {
                    lineBuffer.push_back(ch);
                }
            }
        }

        // Reset the pipe so a new client can connect.
        DisconnectNamedPipe(h);
    }
#endif
}

} // namespace efl
