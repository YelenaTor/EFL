#pragma once

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
};

} // namespace efl
