#pragma once

// Layer B (PRIVATE): Room and context tracking

#include <string>
#include <functional>
#include <vector>
#include <yytk/yytk.h>

namespace efl::bridge {

class RoomTracker {
public:
    explicit RoomTracker(YYTK::YYTKInterface* yytk);

    using RoomChangeCallback = std::function<void(YYTK::RoomId oldRoom, YYTK::RoomId newRoom)>;

    // Call each frame to check for room changes
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
