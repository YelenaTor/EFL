#include "efl/bridge/room_tracker.h"

namespace efl::bridge {

#ifdef EFL_STUB_SDK

// ── Stub implementation (for tests) ─────────────────────────────────────────

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

#else // Real SDK

// ── Real implementation ─────────────────────────────────────────────────────

RoomTracker::RoomTracker(YYTK::YYTKInterface* yytk)
    : yytk_(yytk) {}

void RoomTracker::update() {
    if (!yytk_) return;

    // Get global instance for builtin access
    YYTK::CInstance* global = nullptr;
    if (!Aurie::AurieSuccess(yytk_->GetGlobalInstance(&global)) || !global)
        return;

    // Read the 'room' builtin variable (current room index)
    YYTK::RValue roomIdx;
    if (!Aurie::AurieSuccess(yytk_->GetBuiltin("room", global, NULL_INDEX, roomIdx)))
        return;

    // Convert room index to name via room_get_name()
    YYTK::RValue nameVal = yytk_->CallBuiltin("room_get_name", {roomIdx});

    std::string newRoom;
    std::string converted;
    if (Aurie::AurieSuccess(yytk_->RValueToString(nameVal, converted))) {
        newRoom = std::move(converted);
    }
    if (newRoom != currentRoom_) {
        std::string oldRoom = currentRoom_;
        currentRoom_ = newRoom;
        fireCallbacks(oldRoom, newRoom);
    }
}

void RoomTracker::onRoomTransition(const std::string& newRoomName) {
    if (newRoomName == currentRoom_) return;

    std::string oldRoom = currentRoom_;
    currentRoom_ = newRoomName;
    fireCallbacks(oldRoom, newRoomName);
}

const std::string& RoomTracker::currentRoomName() const {
    return currentRoom_;
}

void RoomTracker::onRoomChange(RoomChangeCallback callback) {
    callbacks_.push_back(std::move(callback));
}

void RoomTracker::fireCallbacks(const std::string& oldRoom, const std::string& newRoom) {
    for (auto& cb : callbacks_) {
        cb(oldRoom, newRoom);
    }
}

#endif // EFL_STUB_SDK

} // namespace efl::bridge
