#pragma once

// Layer C: Structured logging to .efl.log

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <chrono>
#include <fstream>

namespace efl {

enum class LogLevel { Debug, Info, Warn, Error };

struct LogEntry {
    LogLevel level;
    std::string category;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
};

class LogService {
public:
    explicit LogService(size_t maxBuffer = 1000);

    void debug(const std::string& category, const std::string& message);
    void info(const std::string& category, const std::string& message);
    void warn(const std::string& category, const std::string& message);
    void error(const std::string& category, const std::string& message);

    std::vector<LogEntry> recent(size_t count) const;
    void setFileOutput(const std::string& path);
    bool isFileOutputOpen() const;

private:
    void log(LogLevel level, const std::string& category, const std::string& message);
    mutable std::mutex mutex_;
    std::deque<LogEntry> buffer_;
    size_t maxBuffer_;
    std::ofstream fileOut_;
};

} // namespace efl
