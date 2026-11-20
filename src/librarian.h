// librarian.h - Mr. Malloc, the procedurally animated librarian
#pragma once
#include "common.h"

struct Pose {
    float pelvisY = 0.80f;     // pelvis height above the floor
    float spine = 0.f;         // forward lean (rad)
    float neck = 0.f;
    float headYaw = 0.f, headPitch = 0.f, headRoll = 0.f;   // pitch > 0 looks down
    float shPitch[2] = { 0.f, 0.f };   // [0] right arm, [1] left arm. > 0 swings forward
    float shRoll[2] = { 0.f, 0.f };    // > 0 swings outward
    float shTwist[2] = { 0.f, 0.f };   // > 0 turns the arm outward about its own length: a raised arm then bends its forearm upward
    float elbow[2] = { 0.f, 0.f };     // > 0 bends the forearm forward / up
    float hip[2] = { 0.f, 0.f };       // > 0 lifts the thigh forward
    float knee[2] = { 0.f, 0.f };      // > 0 bends the shin backward
};
Pose LerpPose(const Pose& a, const Pose& b, float t);

namespace poses {
Pose Sit();
Pose Stand();
Pose Carry();
Pose PlaceOnDesk();
Pose Crouch();
Pose DrawerReach();
}

class Librarian {
public:
    Vector3 pos{ 0.f, 0.f, -0.30f };
    float yaw = 0.f;                 // 0 = facing +Z (towards the visitor)
    Pose base;                       // pose set by the director every frame
    float walk = 0.f;                // walk-cycle blend 0..1
    float walkPhase = 0.f;
    bool carrying = false;           // suppress arm swing while holding a book
    bool seated = true;
    float talk = 0.f;                // 0..1 mouth target from the dialogue
    float excitement = 0.f;          // eyebrow raise
    Vector3 lookAt{ 0.f, 1.3f, 1.9f };
    float lookWeight = 1.f;
    float gestureAmount = 1.f;       // 0 disables talking gestures
    std::string look = "malloc";     // who is on duty: an id from unlocks::Roster()
    float clock = 0.f;               // animation time for the robot's LEDs
    float debugMouth = -1.f, debugBlink = -1.f;   // autopilot: hold the mouth open / the eyes shut for a screenshot (-1 = off)
    std::function<void()> onFootstep;

    void Init();
    void Update(float dt, float time);
    void Draw() const;
    void DrawShadow() const;
    static void FreeMeshes();          // releases the sculpted heads; call before the window closes

    Matrix Hand(int side) const { return mHand[side]; }
    Vector3 HeadPos() const;
    Matrix CarryMatrix() const;            // closed book held in both hands, cover up
    Matrix ShoulderMatrix(int side) const { return mShoulder[side]; }
    Pose ReachPose(Vector3 target, int side, const Pose& from) const;   // arm pointing at a world target
    bool HitTest(Ray r) const;             // is the mouse over the librarian?

private:
    Pose cur;
    float blinkTimer = 3.f, blink = 0.f;
    float mouth = 0.f, brow = 0.f;
    float gestureTimer = 2.f, gestureT = 0.f;
    int gestureKind = 0;
    float gestureBlend = 0.f;
    float prevStepSin = 0.f;
    float headYawS = 0.f, headPitchS = 0.f;
    Matrix mRoot, mPelvis, mSpine, mNeck, mHead;
    Matrix mShoulder[2], mElbow[2], mHand[2], mHip[2], mKnee[2], mFoot[2];
    void Skeleton();
};
