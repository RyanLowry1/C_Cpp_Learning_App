// librarian.cpp - skeleton, procedural animation layers and rendering of Mr. Malloc
#include "librarian.h"
#include "librarian_look.h"
#include "gfx.h"
#include "book3d.h"

using util::RGBA;

namespace {
constexpr float TORSO = 0.56f, NECK = 0.035f, UPPER = 0.28f, FORE = 0.26f;
constexpr float THIGH = 0.37f, SHIN = 0.37f;
constexpr float SHOULDER_Y = TORSO - 0.06f, HIP_X = 0.072f;

Matrix T(float x, float y, float z) { return MatrixTranslate(x, y, z); }
Matrix M(const Matrix& a, const Matrix& b) { return MatrixMultiply(a, b); }
// sphere scaled to `s`, centred at `c` in the given frame
} // namespace

Pose LerpPose(const Pose& a, const Pose& b, float t) {
    Pose p;
    auto L = [&](float x, float y) { return x + (y - x) * t; };
    p.pelvisY = L(a.pelvisY, b.pelvisY);
    p.spine = L(a.spine, b.spine);
    p.neck = L(a.neck, b.neck);
    p.headYaw = L(a.headYaw, b.headYaw);
    p.headPitch = L(a.headPitch, b.headPitch);
    p.headRoll = L(a.headRoll, b.headRoll);
    for (int i = 0; i < 2; i++) {
        p.shPitch[i] = L(a.shPitch[i], b.shPitch[i]);
        p.shRoll[i] = L(a.shRoll[i], b.shRoll[i]);
        p.shTwist[i] = L(a.shTwist[i], b.shTwist[i]);
        p.elbow[i] = L(a.elbow[i], b.elbow[i]);
        p.hip[i] = L(a.hip[i], b.hip[i]);
        p.knee[i] = L(a.knee[i], b.knee[i]);
    }
    return p;
}

namespace poses {
Pose Sit() {
    Pose p;
    p.pelvisY = 0.53f; p.spine = 0.14f; p.neck = -0.04f;
    for (int i = 0; i < 2; i++) { p.shPitch[i] = 0.55f; p.shRoll[i] = 0.10f; p.elbow[i] = 1.0f; p.hip[i] = 1.57f; p.knee[i] = 1.57f; }
    return p;
}
Pose Stand() {
    Pose p;
    p.pelvisY = 0.80f; p.spine = 0.04f;
    for (int i = 0; i < 2; i++) { p.shPitch[i] = 0.06f; p.shRoll[i] = 0.08f; p.elbow[i] = 0.18f; }
    return p;
}
Pose Carry() {
    Pose p = Stand();
    for (int i = 0; i < 2; i++) { p.shPitch[i] = 0.50f; p.shRoll[i] = -0.16f; p.elbow[i] = 1.2f; }
    p.headPitch = 0.15f;
    return p;
}
Pose PlaceOnDesk() {
    Pose p = Stand();
    p.pelvisY = 0.78f; p.spine = 0.56f;
    for (int i = 0; i < 2; i++) { p.shPitch[i] = 0.95f; p.shRoll[i] = -0.12f; p.elbow[i] = 0.35f; p.knee[i] = 0.1f; p.hip[i] = 0.05f; }
    p.headPitch = 0.1f;
    return p;
}
Pose Crouch() {
    Pose p = Stand();
    p.pelvisY = 0.74f; p.spine = 0.48f;
    for (int i = 0; i < 2; i++) { p.hip[i] = 0.55f; p.knee[i] = 0.75f; }
    return p;
}
Pose DrawerReach() {
    Pose p = Sit();
    p.spine = 0.38f;
    p.shPitch[1] = 0.25f; p.shRoll[1] = 0.55f; p.elbow[1] = 0.25f;
    p.headPitch = 0.35f; p.headYaw = 0.55f;
    return p;
}
} // namespace poses

void Librarian::Init() {
    base = poses::Sit();
    cur = base;
    Skeleton();
}

void Librarian::Skeleton() {
    mRoot = M(MatrixRotateY(yaw), T(pos.x, 0, pos.z));
    mPelvis = M(T(0, cur.pelvisY, 0), mRoot);
    mSpine = M(MatrixRotateX(cur.spine), mPelvis);
    mNeck = M(MatrixRotateX(cur.neck), M(T(0, TORSO, 0), mSpine));
    Matrix headRot = M(M(MatrixRotateZ(cur.headRoll), MatrixRotateX(cur.headPitch)), MatrixRotateY(cur.headYaw));
    mHead = M(headRot, M(T(0, NECK, 0), mNeck));
    for (int i = 0; i < 2; i++) {
        float s = i == 0 ? -1.f : 1.f;
        Matrix rot = M(MatrixRotateY(s * cur.shTwist[i]), M(MatrixRotateZ(s * cur.shRoll[i]), MatrixRotateX(-cur.shPitch[i])));
        mShoulder[i] = M(rot, M(T(s * look::ShoulderX(look), SHOULDER_Y, 0), mSpine));
        mElbow[i] = M(MatrixRotateX(-cur.elbow[i]), M(T(0, -UPPER, 0), mShoulder[i]));
        mHand[i] = M(T(0, -FORE, 0), mElbow[i]);
        mHip[i] = M(MatrixRotateX(-cur.hip[i]), M(T(s * HIP_X, 0, 0), mPelvis));
        mKnee[i] = M(MatrixRotateX(cur.knee[i]), M(T(0, -THIGH, 0), mHip[i]));
        mFoot[i] = M(T(0, -SHIN, 0), mKnee[i]);
    }
}

Vector3 Librarian::HeadPos() const { return Vector3Transform(Vector3{ 0, 0.115f, 0 }, mHead); }

Matrix Librarian::CarryMatrix() const {
    Vector3 a = gfx::Origin(mHand[0]), b = gfx::Origin(mHand[1]);
    Vector3 mid = Vector3Scale(Vector3Add(a, b), 0.5f);
    return M(M(T(-book::W * 0.5f, 0, 0), MatrixRotateY(yaw)), T(mid.x, mid.y - 0.035f, mid.z));
}

Pose Librarian::ReachPose(Vector3 target, int side, const Pose& from) const {
    Pose p = from;
    Vector3 sh = gfx::Origin(mShoulder[side]);
    Vector3 d = Vector3Normalize(Vector3Subtract(target, sh));
    Matrix rot = mSpine;
    rot.m12 = rot.m13 = rot.m14 = 0;
    Vector3 local = Vector3Transform(d, MatrixTranspose(rot));
    float s = side == 0 ? -1.f : 1.f;
    p.shPitch[side] = atan2f(local.z, -local.y);
    p.shRoll[side] = asinf(Clamp(s * local.x, -0.9f, 0.9f));
    p.elbow[side] = 0.12f;
    return p;
}

bool Librarian::HitTest(Ray r) const {
    Vector3 head = HeadPos();
    Vector3 chest = Vector3Transform(Vector3{ 0, 0.35f, 0.02f }, mSpine);
    return GetRayCollisionSphere(r, head, 0.15f).hit || GetRayCollisionSphere(r, chest, 0.27f).hit;
}

void Librarian::Update(float dt, float time) {
    clock = time;
    // blinking
    blinkTimer -= dt;
    if (blinkTimer <= 0) { blink = 0.16f; blinkTimer = 2.2f + fmodf(time * 7.31f, 3.5f); }
    if (blink > 0) blink -= dt;

    muscle = util::Damp(muscle, muscleTarget, 0.8f, dt);   // slow, so a change in form is seen gradually
    mouth = util::Damp(mouth, talk, 22.f, dt);
    brow = util::Damp(brow, Clamp(talk * 0.6f + excitement, 0.f, 1.f), 6.f, dt);
    if (debugMouth >= 0.f) mouth = debugMouth;
    if (debugBlink >= 0.f) blink = debugBlink;

    cur = base;
    // breathing
    float br = sinf(time * 1.7f);
    cur.spine += 0.012f * br;
    for (int i = 0; i < 2; i++) cur.shPitch[i] += 0.012f * br;
    // talking: head bob & gestures
    bool talking = talk > 0.02f;
    if (talking) {
        cur.headPitch += 0.028f * sinf(time * 7.3f);
        cur.headRoll += 0.03f * sinf(time * 2.9f);
    }
    if (seated && gestureAmount > 0.01f) {
        if (talking) {
            gestureTimer -= dt;
            if (gestureTimer <= 0 && gestureT <= 0) {
                gestureKind = 1 + (int)fmodf(time * 13.7f, 4.f);
                gestureT = 1.7f;
                gestureTimer = 2.5f + fmodf(time * 3.3f, 2.5f);
            }
        }
        if (gestureT > 0) {
            gestureT -= dt;
            float g = sinf(util::Clamp01(1.f - gestureT / 1.7f) * PI);
            gestureBlend = g * gestureAmount;
        } else gestureBlend = util::Damp(gestureBlend, 0.f, 8.f, dt);
        float b = gestureBlend;
        switch (gestureKind) {
            case 1: cur.shPitch[0] += 0.55f * b; cur.shRoll[0] += 0.25f * b; cur.elbow[0] += 0.95f * b; cur.headRoll += 0.08f * b; break;
            case 2: for (int i = 0; i < 2; i++) { cur.shRoll[i] += 0.45f * b; cur.elbow[i] += 0.25f * b; cur.shPitch[i] += 0.15f * b; } break;
            case 3: cur.shPitch[0] += 0.72f * b; cur.elbow[0] += 1.45f * b; cur.shRoll[0] -= 0.2f * b; break;
            case 4: cur.shPitch[1] += 0.6f * b; cur.elbow[1] += 0.7f * b; cur.shRoll[1] += 0.3f * b; break;
            default: break;
        }
    }
    // walk cycle
    if (walk > 0.001f) {
        float s = sinf(walkPhase), c = cosf(walkPhase);
        float w = walk;
        auto L = [&](float a, float b) { return a + (b - a) * w; };
        cur.hip[0] = L(cur.hip[0], 0.40f * s);
        cur.hip[1] = L(cur.hip[1], -0.40f * s);
        cur.knee[0] = L(cur.knee[0], 0.08f + 0.6f * std::max(0.f, c));
        cur.knee[1] = L(cur.knee[1], 0.08f + 0.6f * std::max(0.f, -c));
        float armK = carrying ? 0.08f : 1.f;
        cur.shPitch[0] += w * -0.32f * s * armK;
        cur.shPitch[1] += w * 0.32f * s * armK;
        cur.pelvisY += w * (0.022f * fabsf(c) - 0.016f);
        cur.spine += 0.05f * w;
        if (onFootstep && walk > 0.3f && ((s >= 0) != (prevStepSin >= 0))) onFootstep();
        prevStepSin = s;
    }
    // head look-at, expressed in the body's yaw frame
    {
        Vector3 head = gfx::Origin(mNeck);
        Vector3 d = Vector3Subtract(lookAt, head);
        float cy = cosf(-yaw), sy = sinf(-yaw);
        Vector3 local{ d.x * cy + d.z * sy, d.y, -d.x * sy + d.z * cy };
        float horiz = sqrtf(local.x * local.x + local.z * local.z);
        float wantYaw = Clamp(atan2f(local.x, local.z), -1.0f, 1.0f);
        float wantPitch = Clamp(atan2f(-local.y, std::max(horiz, 0.05f)) - cur.spine - cur.neck, -0.5f, 0.6f);
        if (local.z < -0.2f) wantYaw = 0;   // never twist the head round backwards
        headYawS = util::Damp(headYawS, wantYaw * lookWeight, 5.f, dt);
        headPitchS = util::Damp(headPitchS, wantPitch * lookWeight, 5.f, dt);
        cur.headYaw += headYawS;
        cur.headPitch += headPitchS;
    }
    for (int i = 0; i < 2; i++) cur.elbow[i] = std::max(0.f, cur.elbow[i]);
    Skeleton();
}

void Librarian::DrawShadow() const {
    Vector3 p = gfx::Origin(mPelvis);
    gfx::BlobShadow(Vector3{ p.x, 0.008f, p.z + 0.05f }, 0.75f, 0.6f, 0.6f, yaw);
}

