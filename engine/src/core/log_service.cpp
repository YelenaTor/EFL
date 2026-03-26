#include "efl/core/log_service.h"

#include <iomanip>
#include <sstream>

namespace efl {

static const char* levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

LogService::LogService(size_t maxBuffer)
    : maxBuffer_(maxBuffer) {}

void LogService::debug(const std::string& category, const std::string& message) {
    log(LogLevel::Debug, category, message);
}

void LogService::info(const std::string& category, const std::string& message) {
    log(LogLevel::Info, category, message);
}

void LogService::warn(const std::string& category, const std::string& message) {
    log(LogLevel::Warn, category, message);
}

void LogService::error(const std::string& category, const std::string& message) {
    log(LogLevel::Error, category, message);
}

void LogService::log(LogLevel level, const std::string& category, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    buffer_.push_back(LogEntry{level, category, message, now});

    while (buffer_.size() > maxBuffer_) {
        buffer_.pop_front();
    }

    if (fileOut_.is_open()) {
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        fileOut_ << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
                 << " [" << levelToString(level) << "] "
                 << "[" << category << "] "
                 << message << "\n";
        fileOut_.flush();
    }
}

std::vector<LogEntry> LogService::recent(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t n = std::min(count, buffer_.size());
    return std::vector<LogEntry>(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(n));
}

void LogService::setFileOutput(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    fileOut_.open(path, std::ios::app);
}

bool LogService::isFileOutputOpen() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return fileOut_.is_open();
}

} // namespace efl
