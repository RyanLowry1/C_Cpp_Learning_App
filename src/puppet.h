// puppet.h - Mr. Malloc as a 2D cut-out puppet: poses, two-bone arms, walk cycle, face
#pragma once
#include "common.h"
#include "book2d.h"

// A pose in "figure space": origin between the feet, y up is negative, units are pixels at scale 1.
struct Pose {
    float drop = 0.f;       // body lowered towards the seat (seated = 120)
    float bend = 0.f;       // lean forward, 0..1
    Vector2 hand[2] = { { -92.f, -196.f }, { 92.f, -196.f } };   // hand targets: [0] = screen left, [1] = screen right
    float tilt = 0.f;       // head tilt (radians)
};

Pose LerpPose(const Pose& a, const Pose& b, float t);

namespace poses {
Pose Sit();
Pose Stand();
Pose Carry();
Pose Place();                           // leaning over the desk, arms out
Pose Reach(Vector2 target, float bend); // both hands on a spot in figure space
Pose Drawer();                          // ducking down behind the desk
}

class Puppet {
public:
    // placement
    Vector2 feet{ 800.f, 732.f };
    float turn = 1.f;               // horizontal scale, used for the turn-around animation
    bool back = false;              // facing away from the viewer
    bool seated = true;
    bool carrying = false;          // holds the (cover-out) book against his chest
    bool behindDesk = true;         // arms and book are redrawn in front of the desk edge
    float walk = 0.f, walkPhase = 0.f;
    float talk = 0.f, excitement = 0.f;
    Vector2 lookAt{ 800.f, 800.f }; // scene-space point he looks at
    Pose base = poses::Sit();
    const book::Book2D* carriedBook = nullptr;
    std::function<void()> onFootstep;

    void Update(float dt, float time);
    void Draw() const;                  // the whole figure
    void DrawHandsOverDesk() const;     // arms, hands and book again (the caller clips to the desk top)

    float Scale() const;
    Vector2 ToScene(Vector2 local) const;
    Vector2 FromScene(Vector2 scene) const;
    Vector2 HeadPos() const;            // scene space
    Vector2 CarryCenter() const;        // scene space, between the hands
    Vector2 HandMid() const;
    bool HitTest(Vector2 p) const;

private:
    Pose cur;
    bool started = false;
    float blinkTimer = 2.f, blink = 0.f, prevStep = 0.f;
    struct Rig {
        Vector2 shoulder[2], elbow[2], hand[2];
        Vector2 head{ 0, -405 };
        float dy = 0.f, tilt = 0.f, lift[2] = { 0, 0 };
        Vector2 eye{ 0, 0 };
        float mouth = 0.f;
    } rig;

    void DrawLegs() const;
    void DrawTorso() const;
    void DrawArms() const;
    void DrawHead() const;
    void DrawCarried() const;
};
