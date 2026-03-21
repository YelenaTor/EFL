#pragma once

// Layer A: EFL bootstrap, init/shutdown, version checks

#include <string>
#include <vector>
#include <memory>
#include "efl/core/log_service.h"
#include "efl/core/diagnostics.h"
#include "efl/core/manifest.h"
#include "efl/core/config_service.h"
#include "efl/core/compatibility_service.h"
#include "efl/core/registry_service.h"
#include "efl/ipc/pipe_writer.h"

namespace efl {

class EflBootstrap {
public:
    EflBootstrap();
    ~EflBootstrap();

    bool initialize(const std::string& contentDir);
    void shutdown();

    LogService& log();
    DiagnosticEmitter& diagnostics();
    PipeWriter& pipe();
    RegistryService& registries();
    const std::vector<Manifest>& manifests() const;

private:
    LogService log_;
    DiagnosticEmitter diagnostics_;
    std::unique_ptr<PipeWriter> pipe_;
    RegistryService registries_;
    ConfigService config_;
    std::vector<Manifest> manifests_;

    bool stepVersionCheck();
    bool stepDiscoverManifests(const std::string& contentDir);
    bool stepValidateManifests();
    void stepLoadContent(const std::string& contentDir);
    void emitBootStatus(const std::string& step, const std::string& status,
                        const std::string& detail = "");
};

} // namespace efl
