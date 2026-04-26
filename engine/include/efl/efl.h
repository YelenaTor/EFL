#pragma once

// EFL - Expansion Framework Library
// Runtime expansion framework for Fields of Mistria

#define EFL_VERSION_STRING "1.0.0"

#ifndef EFL_VERSION_PRERELEASE
#define EFL_VERSION_PRERELEASE ""
#endif

#ifndef EFL_VERSION_MAJOR
#define EFL_VERSION_MAJOR 1
#endif
#ifndef EFL_VERSION_MINOR
#define EFL_VERSION_MINOR 0
#endif
#ifndef EFL_VERSION_PATCH
#define EFL_VERSION_PATCH 0
#endif

#ifdef EFL_BUILDING_DLL
#define EFL_API __declspec(dllexport)
#else
#define EFL_API __declspec(dllimport)
#endif

// Core (Layer A + C)
#include "core/bootstrap.h"
#include "core/manifest.h"
#include "core/event_bus.h"
#include "core/save_service.h"
#include "core/trigger_service.h"
#include "core/config_service.h"
#include "core/log_service.h"
#include "core/registry_service.h"
#include "core/compatibility_service.h"
#include "core/diagnostics.h"

// Registries (Layer D) - Public API
#include "registries/area_registry.h"
#include "registries/warp_service.h"
#include "registries/resource_registry.h"
#include "registries/crafting_registry.h"
#include "registries/npc_registry.h"
#include "registries/quest_registry.h"
#include "registries/dialogue_service.h"
#include "registries/story_bridge.h"
#include "registries/world_state_service.h"

// IPC (Layer F)
#include "ipc/channel_broker.h"
