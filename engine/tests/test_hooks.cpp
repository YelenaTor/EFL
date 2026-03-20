#include <gtest/gtest.h>
#include "efl/bridge/hooks.h"
#include "efl/bridge/room_tracker.h"
#include "efl/bridge/routine_invoker.h"
#include "efl/bridge/instance_walker.h"

// ---------------------------------------------------------------------------
// HookRegistry tests
// ---------------------------------------------------------------------------

TEST(HookRegistry, RegisterAndTrack) {
    YYTK::YYTKInterface yytk;
    efl::bridge::HookRegistry reg(&yytk);
    bool ok = reg.registerHook("room_trans",
                               "gml_Object_obj_roomtransition_Create_0",
                               [](){});
    EXPECT_TRUE(ok);
    EXPECT_TRUE(reg.isRegistered("room_trans"));
    EXPECT_EQ(reg.count(), 1u);
}

TEST(HookRegistry, RemoveHook) {
    YYTK::YYTKInterface yytk;
    efl::bridge::HookRegistry reg(&yytk);
    reg.registerHook("test", "gml_target", [](){});
    reg.removeHook("test");
    EXPECT_FALSE(reg.isRegistered("test"));
    EXPECT_EQ(reg.count(), 0u);
}

TEST(HookRegistry, RemoveAll) {
    YYTK::YYTKInterface yytk;
    efl::bridge::HookRegistry reg(&yytk);
    reg.registerHook("a", "target_a", [](){});
    reg.registerHook("b", "target_b", [](){});
    reg.removeAll();
    EXPECT_EQ(reg.count(), 0u);
}

TEST(HookRegistry, DuplicateNameRejected) {
    YYTK::YYTKInterface yytk;
    efl::bridge::HookRegistry reg(&yytk);
    EXPECT_TRUE(reg.registerHook("dup", "target_x", [](){}));
    EXPECT_FALSE(reg.registerHook("dup", "target_y", [](){}));
    EXPECT_EQ(reg.count(), 1u);
}

// ---------------------------------------------------------------------------
// RoomTracker tests
// ---------------------------------------------------------------------------

// Subclass that lets us control GetCurrentRoom return value
class FakeYYTK : public YYTK::YYTKInterface {
public:
    YYTK::RoomId fakeRoom = YYTK::ROOM_INVALID;
    std::string fakeRoomName;

    YYTK::RoomId GetCurrentRoom() override { return fakeRoom; }
    std::string GetRoomName(YYTK::RoomId /*id*/) override { return fakeRoomName; }

    std::vector<YYTK::InstanceId> GetInstances(const std::string& /*name*/) override {
        return {10, 20, 30};
    }

    YYTK::RValue GetInstanceVariable(YYTK::InstanceId /*id*/,
                                     const std::string& /*varName*/) override {
        return YYTK::RValue(42.0);
    }
};

TEST(RoomTracker, InitialRoomIsInvalid) {
    FakeYYTK yytk;
    efl::bridge::RoomTracker tracker(&yytk);
    EXPECT_EQ(tracker.currentRoom(), YYTK::ROOM_INVALID);
}

TEST(RoomTracker, FiresCallbackOnChange) {
    FakeYYTK yytk;
    efl::bridge::RoomTracker tracker(&yytk);

    YYTK::RoomId capturedOld = -99;
    YYTK::RoomId capturedNew = -99;
    tracker.onRoomChange([&](YYTK::RoomId oldR, YYTK::RoomId newR) {
        capturedOld = oldR;
        capturedNew = newR;
    });

    yytk.fakeRoom = 5;
    tracker.update();

    EXPECT_EQ(capturedOld, YYTK::ROOM_INVALID);
    EXPECT_EQ(capturedNew, 5);
    EXPECT_EQ(tracker.currentRoom(), 5);
}

TEST(RoomTracker, NoCallbackWhenRoomUnchanged) {
    FakeYYTK yytk;
    yytk.fakeRoom = 3;
    efl::bridge::RoomTracker tracker(&yytk);

    tracker.update(); // transitions from INVALID -> 3
    int callCount = 0;
    tracker.onRoomChange([&](YYTK::RoomId, YYTK::RoomId) { ++callCount; });

    tracker.update(); // still 3, no change
    EXPECT_EQ(callCount, 0);
}

TEST(RoomTracker, RoomName) {
    FakeYYTK yytk;
    yytk.fakeRoom = 7;
    yytk.fakeRoomName = "rm_farm";
    efl::bridge::RoomTracker tracker(&yytk);
    tracker.update();
    EXPECT_EQ(tracker.currentRoomName(), "rm_farm");
}

// ---------------------------------------------------------------------------
// RoutineInvoker tests
// ---------------------------------------------------------------------------

TEST(RoutineInvoker, HasRoutineReturnsFalseForNullStub) {
    YYTK::YYTKInterface yytk; // stub always returns nullptr
    efl::bridge::RoutineInvoker invoker(&yytk);
    EXPECT_FALSE(invoker.hasRoutine("gml_Script_some_func"));
}

TEST(RoutineInvoker, InvokeReturnsNulloptForMissingRoutine) {
    YYTK::YYTKInterface yytk;
    efl::bridge::RoutineInvoker invoker(&yytk);
    auto result = invoker.invoke("gml_Script_missing");
    EXPECT_FALSE(result.has_value());
}

TEST(RoutineInvoker, ClearCacheDoesNotCrash) {
    YYTK::YYTKInterface yytk;
    efl::bridge::RoutineInvoker invoker(&yytk);
    invoker.hasRoutine("gml_Script_test");
    invoker.clearCache();
    EXPECT_FALSE(invoker.hasRoutine("gml_Script_test"));
}

// ---------------------------------------------------------------------------
// InstanceWalker tests
// ---------------------------------------------------------------------------

TEST(InstanceWalker, GetAllDelegatestoYYTK) {
    FakeYYTK yytk;
    efl::bridge::InstanceWalker walker(&yytk);
    auto ids = walker.getAll("par_NPC");
    ASSERT_EQ(ids.size(), 3u);
    EXPECT_EQ(ids[0], 10);
    EXPECT_EQ(ids[1], 20);
    EXPECT_EQ(ids[2], 30);
}

TEST(InstanceWalker, FilterAppliesPredicate) {
    FakeYYTK yytk;
    efl::bridge::InstanceWalker walker(&yytk);
    auto ids = walker.filter("par_NPC", [](YYTK::InstanceId id) { return id > 15; });
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], 20);
    EXPECT_EQ(ids[1], 30);
}

TEST(InstanceWalker, GetVariableDelegates) {
    FakeYYTK yytk;
    efl::bridge::InstanceWalker walker(&yytk);
    auto val = walker.getVariable(10, "x");
    EXPECT_EQ(val.kind, YYTK::RValue::REAL);
    EXPECT_DOUBLE_EQ(val.real, 42.0);
}

TEST(InstanceWalker, SetVariableDoesNotCrash) {
    FakeYYTK yytk;
    efl::bridge::InstanceWalker walker(&yytk);
    YYTK::RValue v(99.0);
    EXPECT_NO_THROW(walker.setVariable(10, "hp", v));
}
