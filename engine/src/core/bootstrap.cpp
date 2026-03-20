#include "efl/core/bootstrap.h"

#include <filesystem>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace efl {

namespace {

std::string eflVersionString() {
    std::ostringstream ss;
    ss << EFL_VERSION_MAJOR << "." << EFL_VERSION_MINOR << "." << EFL_VERSION_PATCH;
    return ss.str();
}

std::string buildPipeName() {
#ifdef _WIN32
    DWORD pid = GetCurrentProcessId();
#else
    int pid = 0;
#endif
    return "\\\\.\\pipe\\efl-" + std::to_string(pid);
}

} // anonymous namespace

EflBootstrap::EflBootstrap() = default;

EflBootstrap::~EflBootstrap() {
    shutdown();
}

bool EflBootstrap::initialize(const std::string& contentDir) {
    // Create the named pipe for TUI communication
    pipe_ = std::make_unique<PipeWriter>(buildPipeName());
    pipe_->create(); // Best-effort; bootstrap continues even if pipe fails

    log_.info("BOOT", "EFL v" + eflVersionString() + " initializing");

    if (!stepVersionCheck())
        return false;

    if (!stepDiscoverManifests(contentDir))
        return false;

    if (!stepValidateManifests())
        return false;

    emitBootStatus("init", "complete",
                   std::to_string(manifests_.size()) + " manifest(s) loaded");
    log_.info("BOOT", "Bootstrap complete — " +
              std::to_string(manifests_.size()) + " manifest(s) loaded");
    return true;
}

void EflBootstrap::shutdown() {
    if (pipe_) {
        pipe_->close();
    }
    log_.info("BOOT", "EFL shutdown");
}

LogService& EflBootstrap::log() { return log_; }
DiagnosticEmitter& EflBootstrap::diagnostics() { return diagnostics_; }
PipeWriter& EflBootstrap::pipe() { return *pipe_; }
RegistryService& EflBootstrap::registries() { return registries_; }
const std::vector<Manifest>& EflBootstrap::manifests() const { return manifests_; }

bool EflBootstrap::stepVersionCheck() {
    emitBootStatus("version_check", "running");
    std::string ver = eflVersionString();
    log_.info("BOOT", "Version check: EFL v" + ver);
    emitBootStatus("version_check", "pass", "EFL v" + ver);
    return true;
}

bool EflBootstrap::stepDiscoverManifests(const std::string& contentDir) {
    emitBootStatus("discover", "running");

    namespace fs = std::filesystem;

    if (!fs::exists(contentDir) || !fs::is_directory(contentDir)) {
        log_.warn("BOOT", "Content directory not found: " + contentDir);
        diagnostics_.emit("BOOT-W001", Severity::Warning, "BOOT",
                          "Content directory does not exist: " + contentDir,
                          "Create the directory and place .efl manifest files in it");
        emitBootStatus("discover", "warn", "content directory not found");
        // Not a fatal error — zero manifests is valid
        return true;
    }

    int found = 0;
    for (const auto& entry : fs::directory_iterator(contentDir)) {
        if (!entry.is_regular_file())
            continue;

        if (entry.path().extension() != ".efl")
            continue;

        auto manifest = ManifestParser::parseFile(entry.path().string());
        if (manifest) {
            log_.info("BOOT", "Loaded manifest: " + manifest->modId +
                      " v" + manifest->version);
            manifests_.push_back(std::move(*manifest));
            ++found;
        } else {
            log_.error("BOOT", "Failed to parse manifest: " + entry.path().string());
            diagnostics_.emit("MANIFEST-E001", Severity::Error, "MANIFEST",
                              "Failed to parse manifest: " + entry.path().filename().string(),
                              "Check JSON syntax and required fields");
        }
    }

    emitBootStatus("discover", "pass", std::to_string(found) + " manifest(s) found");
    return true;
}

bool EflBootstrap::stepValidateManifests() {
    emitBootStatus("validate", "running");

    std::string eflVer = eflVersionString();
    bool allOk = true;

    for (const auto& m : manifests_) {
        if (!CompatibilityService::isCompatible(eflVer, m.eflVersion)) {
            log_.error("BOOT", "Manifest '" + m.modId +
                       "' requires EFL v" + m.eflVersion +
                       " but running v" + eflVer);
            diagnostics_.emit("MANIFEST-E002", Severity::Error, "MANIFEST",
                              "Mod '" + m.modId + "' requires EFL v" + m.eflVersion +
                              " (running v" + eflVer + ")",
                              "Update EFL or use a compatible mod version");
            allOk = false;
        }
    }

    if (allOk) {
        emitBootStatus("validate", "pass");
    } else {
        emitBootStatus("validate", "fail", "version incompatibilities found");
    }

    return allOk;
}

void EflBootstrap::emitBootStatus(const std::string& step, const std::string& status,
                                   const std::string& detail) {
    if (!pipe_)
        return;

    nlohmann::json payload;
    payload["step"] = step;
    payload["status"] = status;
    if (!detail.empty())
        payload["detail"] = detail;

    pipe_->write("boot.status", payload);
}

} // namespace efl
