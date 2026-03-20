#pragma once

// Diagnostic code definitions (BOOT-001, MANIFEST-003, etc.)

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace efl {

enum class Severity { Error, Warning, Hazard };

std::string severityWireName(Severity s);

struct DiagnosticEntry {
    std::string code;
    Severity severity;
    std::string category;
    std::string message;
    std::string suggestion;
};

class DiagnosticEmitter {
public:
    void emit(const std::string& code, Severity severity,
              const std::string& category, const std::string& message,
              const std::string& suggestion = "");

    const std::vector<DiagnosticEntry>& all() const;
    size_t countBySeverity(Severity severity) const;

    nlohmann::json toJson(const DiagnosticEntry& entry) const;

private:
    std::vector<DiagnosticEntry> entries_;
};

} // namespace efl
