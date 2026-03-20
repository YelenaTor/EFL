#include "efl/bridge/room_tracker.h"

namespace efl::bridge {

RoomTracker::RoomTracker(YYTK::YYTKInterface* yytk)
    : yytk_(yytk), currentRoom_(YYTK::ROOM_INVALID) {}

void RoomTracker::update() {
    if (!yytk_) return;

    YYTK::RoomId newRoom = yytk_->GetCurrentRoom();
    if (newRoom != currentRoom_) {
        YYTK::RoomId oldRoom = currentRoom_;
        currentRoom_ = newRoom;
        for (auto& cb : callbacks_) {
            cb(oldRoom, newRoom);
        }
    }
}

YYTK::RoomId RoomTracker::currentRoom() const {
    return currentRoom_;
}

std::string RoomTracker::currentRoomName() const {
    if (!yytk_ || currentRoom_ == YYTK::ROOM_INVALID) return "";
    return yytk_->GetRoomName(currentRoom_);
}

void RoomTracker::onRoomChange(RoomChangeCallback callback) {
    callbacks_.push_back(std::move(callback));
}

} // namespace efl::bridge
