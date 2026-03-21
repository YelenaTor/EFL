#include "efl/core/diagnostics.h"
#include "efl/ipc/pipe_writer.h"

#include <algorithm>

namespace efl {

void DiagnosticEmitter::setPipeWriter(PipeWriter* pipe) {
    pipe_ = pipe;
}

std::string severityWireName(Severity s) {
    switch (s) {
        case Severity::Error:   return "error";
        case Severity::Warning: return "warning";
        case Severity::Hazard:  return "hazard";
    }
    return "unknown";
}

void DiagnosticEmitter::emit(const std::string& code, Severity severity,
                              const std::string& category, const std::string& message,
                              const std::string& suggestion) {
    DiagnosticEntry entry{code, severity, category, message, suggestion};
    entries_.push_back(entry);

    if (pipe_) {
        pipe_->write("diagnostic", toJson(entry));
    }
}

const std::vector<DiagnosticEntry>& DiagnosticEmitter::all() const {
    return entries_;
}

size_t DiagnosticEmitter::countBySeverity(Severity severity) const {
    return static_cast<size_t>(
        std::count_if(entries_.begin(), entries_.end(),
                      [severity](const DiagnosticEntry& e) {
                          return e.severity == severity;
                      }));
}

nlohmann::json DiagnosticEmitter::toJson(const DiagnosticEntry& entry) const {
    return nlohmann::json{
        {"code", entry.code},
        {"severity", severityWireName(entry.severity)},
        {"category", entry.category},
        {"message", entry.message},
        {"suggestion", entry.suggestion}
    };
}

} // namespace efl
