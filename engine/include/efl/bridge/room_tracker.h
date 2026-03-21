#pragma once

// Layer B (PRIVATE): Room and context tracking

#include <string>
#include <functional>
#include <vector>

#ifdef EFL_STUB_SDK
#include "efl/bridge/sdk_compat.h"

namespace efl::bridge {

class RoomTracker {
public:
    explicit RoomTracker(YYTK::YYTKInterface* yytk);

    using RoomChangeCallback = std::function<void(YYTK::RoomId oldRoom, YYTK::RoomId newRoom)>;

    void update();

    YYTK::RoomId currentRoom() const;
    std::string currentRoomName() const;
    void onRoomChange(RoomChangeCallback callback);

private:
    YYTK::YYTKInterface* yytk_;
    YYTK::RoomId currentRoom_ = YYTK::ROOM_INVALID;
    std::vector<RoomChangeCallback> callbacks_;
};

} // namespace efl::bridge

#else // Real SDK

#include <Aurie/shared.hpp>
#include <YYToolkit/YYTK_Shared.hpp>

namespace efl::bridge {

class RoomTracker {
public:
    RoomTracker(YYTK::YYTKInterface* yytk);

    using RoomChangeCallback = std::function<void(const std::string& oldRoom, const std::string& newRoom)>;

    // Call from frame callback to detect room changes via YYTK.
    void update();

    // Notify directly from a room transition hook.
    void onRoomTransition(const std::string& newRoomName);

    const std::string& currentRoomName() const;
    void onRoomChange(RoomChangeCallback callback);

private:
    YYTK::YYTKInterface* yytk_;
    std::string currentRoom_;
    std::vector<RoomChangeCallback> callbacks_;

    void fireCallbacks(const std::string& oldRoom, const std::string& newRoom);
};

} // namespace efl::bridge

#endif // EFL_STUB_SDK
