// puppet.cpp - Mr. Malloc, drawn as layered vector shapes and animated with poses + two-bone arms
#include "puppet.h"
#include "stage.h"
#include "art.h"
#include "rlgl.h"
#include <algorithm>

using namespace util;

namespace {
constexpr float HIP_Y = -200.f, SH_Y = -335.f, SH_X = 76.f, HEAD_Y = -408.f;
constexpr float L1 = 104.f, L2 = 98.f;
const Color kJacket{ 90, 58, 40, 255 }, kJacketDark{ 58, 36, 26, 255 }, kVest{ 172, 124, 72, 255 };
const Color kShirt{ 242, 236, 224, 255 }, kTie{ 170, 34, 42, 255 };
const Color kSkin{ 238, 192, 152, 255 }, kSkinShade{ 206, 154, 118, 255 };
const Color kHair{ 244, 242, 236, 255 }, kTrousers{ 60, 57, 70, 255 }, kShoes{ 28, 23, 25, 255 }, kGold{ 218, 178, 96, 255 };
}

Pose LerpPose(const Pose& a, const Pose& b, float t) {
    Pose p;
    p.drop = Lerp(a.drop, b.drop, t);
    p.bend = Lerp(a.bend, b.bend, t);
    p.tilt = Lerp(a.tilt, b.tilt, t);
    for (int i = 0; i < 2; i++) p.hand[i] = Vector2Lerp(a.hand[i], b.hand[i], t);
    return p;
}

namespace poses {
Pose Sit() { Pose p; p.drop = 120.f; p.hand[0] = { -118.f, -38.f }; p.hand[1] = { 118.f, -38.f }; return p; }
Pose Stand() { Pose p; p.hand[0] = { -84.f, -140.f }; p.hand[1] = { 84.f, -140.f }; return p; }
Pose Carry() { Pose p; p.hand[0] = { -48.f, -246.f }; p.hand[1] = { 48.f, -246.f }; return p; }
Pose Place() { Pose p; p.bend = 0.85f; p.hand[0] = { -52.f, -70.f }; p.hand[1] = { 52.f, -70.f }; return p; }
Pose Reach(Vector2 t, float bend) { Pose p; p.bend = bend; p.hand[0] = { t.x - 13.f, t.y }; p.hand[1] = { t.x + 13.f, t.y }; return p; }
Pose Drawer() { Pose p; p.drop = 64.f; p.bend = 1.f; p.hand[0] = { -40.f, -60.f }; p.hand[1] = { 40.f, -60.f }; return p; }
}

float Puppet::Scale() const { return world::ScaleAtY(feet.y); }
Vector2 Puppet::ToScene(Vector2 l) const { float s = Scale(); return Vector2{ feet.x + l.x * s * turn, feet.y + l.y * s }; }
Vector2 Puppet::FromScene(Vector2 p) const { float s = Scale(); return Vector2{ (p.x - feet.x) / (s * std::max(0.05f, turn)), (p.y - feet.y) / s }; }
Vector2 Puppet::HeadPos() const { return ToScene(rig.head); }
Vector2 Puppet::HandMid() const { return ToScene(Vector2{ (rig.hand[0].x + rig.hand[1].x) * 0.5f, (rig.hand[0].y + rig.hand[1].y) * 0.5f }); }
Vector2 Puppet::CarryCenter() const { Vector2 m = HandMid(); return Vector2{ m.x, m.y - 4.f * Scale() }; }

bool Puppet::HitTest(Vector2 p) const {
    Vector2 h = HeadPos();
    float s = Scale();
    if (Vector2Distance(p, Vector2{ h.x, h.y + 6 * s }) < 70.f * s) return true;
    Vector2 l = FromScene(p);
    return fabsf(l.x) < 84.f && l.y > SH_Y + rig.dy - 20.f && l.y < HIP_Y + rig.dy + 30.f;
}

void Puppet::Update(float dt, float time) {
    if (!started) { cur = base; started = true; }
    else cur = LerpPose(cur, base, 1.f - expf(-26.f * dt));
    Pose p = cur;

    // gestures while talking, excitement when poked
    for (int i = 0; i < 2; i++) {
        float side = i == 0 ? -1.f : 1.f;
        if (seated && !carrying) {
            p.hand[i].y -= talk * (6.f + 12.f * (0.5f + 0.5f * sinf(time * 5.1f + i * 2.3f)));
            p.hand[i].x += side * talk * 6.f * sinf(time * 3.3f + i);
            p.hand[i].y -= excitement * 80.f;
        }
    }
    if (!seated && !carrying && walk > 0.05f) {
        p.hand[0].y += sinf(walkPhase) * 12.f * walk;
        p.hand[1].y -= sinf(walkPhase) * 12.f * walk;
    }
    float bob = fabsf(sinf(walkPhase)) * 5.f * walk;
    rig.dy = p.drop + p.bend * 34.f + sinf(time * 1.8f) * 1.5f - bob;
    rig.lift[0] = std::max(0.f, sinf(walkPhase)) * 16.f * walk;
    rig.lift[1] = std::max(0.f, -sinf(walkPhase)) * 16.f * walk;
    float s = sinf(walkPhase);
    if (walk > 0.3f && ((s > 0.f) != (prevStep > 0.f)) && onFootstep) onFootstep();
    prevStep = s;

    float sy = SH_Y + rig.dy;
    for (int i = 0; i < 2; i++) {
        float side = i == 0 ? -1.f : 1.f;
        rig.shoulder[i] = Vector2{ side * (SH_X - 8.f), sy + 14.f };
        rig.hand[i] = art::ClampReach(rig.shoulder[i], p.hand[i], L1, L2);
        rig.elbow[i] = art::Elbow(rig.shoulder[i], rig.hand[i], L1, L2, side);
    }
    rig.head = Vector2{ 0.f, HEAD_Y + rig.dy + p.bend * 28.f };

    // eyes and head follow the look-at point
    Vector2 hs = HeadPos();
    float dx = lookAt.x - hs.x, dyy = lookAt.y - hs.y;
    Vector2 want{ Clamp(dx / 260.f, -1.f, 1.f) * 5.5f, Clamp(dyy / 260.f, -1.f, 1.f) * 4.f };
    float k = 1.f - expf(-12.f * dt);
    rig.eye = Vector2Lerp(rig.eye, want, k);
    float wantTilt = p.tilt + Clamp(dx / 900.f, -1.f, 1.f) * 0.09f;
    rig.tilt = Lerp(rig.tilt, wantTilt, k);
    rig.mouth = talk;

    blinkTimer -= dt;
    if (blinkTimer <= 0.f && blink <= 0.f) { blink = 0.0001f; }
    if (blink > 0.f) {
        blink += dt;
        if (blink > 0.17f) { blink = 0.f; blinkTimer = 2.4f + fmodf(time * 7.31f, 3.f); }
    }
}

// ---------------------------------------------------------------------------
// drawing (local figure space, transform set by Draw())
// ---------------------------------------------------------------------------
void Puppet::DrawLegs() const {
    float hipY = HIP_Y + cur.drop;
    for (int i = 0; i < 2; i++) {
        float sx = i == 0 ? -1.f : 1.f;
        Vector2 hip{ sx * 26.f, hipY }, ankle{ sx * 31.f, -20.f - rig.lift[i] };
        art::Cap(hip, ankle, 48.f, kTrousers);
        DrawLineEx(Vector2{ hip.x + sx * 8.f, hip.y + 10.f }, Vector2{ ankle.x + sx * 8.f, ankle.y - 6.f }, 3.f, WithAlpha(BLACK, 0.18f));
        art::EllShaded(Vector2{ sx * 36.f, -9.f - rig.lift[i] }, 26.f, 13.f, kShoes);
    }
}

void Puppet::DrawTorso() const {
    float sy = SH_Y + rig.dy, wy = HIP_Y + rig.dy + 22.f;
    float mid = (sy + wy) * 0.5f + 12.f;
    Vector2 body[10] = { { -26, sy - 14 }, { 26, sy - 14 }, { 82, sy + 8 }, { 80, sy + 44 }, { 80, mid }, { 72, wy }, { -72, wy }, { -80, mid }, { -80, sy + 44 }, { -82, sy + 8 } };
    art::Poly(body, 10, kJacket);
    art::Ell(Vector2{ -76.f, sy + 16.f }, 25.f, 25.f, kJacket);
    art::Ell(Vector2{ 76.f, sy + 16.f }, 25.f, 25.f, kJacket);
    art::Ell(Vector2{ 0.f, mid - 6.f }, 82.f, 66.f, kJacket);
    DrawRectangleGradientH(-80, (int)(sy + 30), 160, (int)(wy - sy - 30), WithAlpha(WHITE, 0.07f), WithAlpha(BLACK, 0.2f));
    // neck
    art::Ell(Vector2{ 0.f, sy - 12.f }, 21.f, 17.f, kSkinShade);
    if (!back) {
        Vector2 vest[4] = { { -36, sy + 2 }, { 36, sy + 2 }, { 42, wy - 8 }, { -42, wy - 8 } };
        art::Poly(vest, 4, kVest);
        art::Ell(Vector2{ 0.f, mid + 4.f }, 46.f, 52.f, kVest);
        Vector2 shirt[3] = { { -24, sy - 12 }, { 24, sy - 12 }, { 0, sy + 48 } };
        art::Poly(shirt, 3, kShirt);
        Vector2 lapL[3] = { { -32, sy - 8 }, { -10, sy + 10 }, { -46, sy + 74 } };
        Vector2 lapR[3] = { { 32, sy - 8 }, { 10, sy + 10 }, { 46, sy + 74 } };
        art::Poly(lapL, 3, kJacketDark); art::Poly(lapR, 3, kJacketDark);
        for (int i = 0; i < 3; i++) { Vector2 b{ 0.f, sy + 66.f + i * 32.f }; art::Ell(b, 5.f, 5.f, kGold); art::Ell(Vector2{ b.x - 1, b.y - 1 }, 2.f, 2.f, Color{ 255, 240, 190, 255 }); }
        // bow tie
        Vector2 bl[3] = { { -4, sy + 6 }, { -32, sy - 8 }, { -32, sy + 20 } };
        Vector2 br[3] = { { 4, sy + 6 }, { 32, sy + 20 }, { 32, sy - 8 } };
        art::Poly(bl, 3, kTie); art::Poly(br, 3, kTie);
        art::Ell(Vector2{ 0.f, sy + 6.f }, 8.f, 9.f, Shade(kTie, 0.8f));
        // watch chain
        DrawSplineSegmentBezierQuadratic(Vector2{ -34, sy + 110 }, Vector2{ -4, sy + 150 }, Vector2{ 30, sy + 108 }, 2.6f, kGold);
        art::Ell(Vector2{ 30.f, sy + 112.f }, 6.f, 6.f, kGold);
    } else {
        DrawLineEx(Vector2{ 0, sy - 6 }, Vector2{ 0, wy }, 2.5f, WithAlpha(kJacketDark, 0.8f));
        DrawLineEx(Vector2{ 0, wy - 54 }, Vector2{ -16, wy }, 2.f, WithAlpha(kJacketDark, 0.7f));
        DrawLineEx(Vector2{ 0, wy - 54 }, Vector2{ 16, wy }, 2.f, WithAlpha(kJacketDark, 0.7f));
        Vector2 col[4] = { { -24, sy - 14 }, { 24, sy - 14 }, { 20, sy + 4 }, { -20, sy + 4 } };
        art::Poly(col, 4, kShirt);
    }
}

void Puppet::DrawArms() const {
    for (int i = 0; i < 2; i++) {
        art::Cap(rig.shoulder[i], rig.elbow[i], 42.f, kJacket);
        art::Cap(rig.elbow[i], rig.hand[i], 36.f, Shade(kJacket, 1.06f));
        Vector2 dir = Vector2Normalize(Vector2Subtract(rig.hand[i], rig.elbow[i]));
        Vector2 w0 = Vector2Subtract(rig.hand[i], Vector2Scale(dir, 22.f)), w1 = Vector2Subtract(rig.hand[i], Vector2Scale(dir, 14.f));
        art::Cap(w0, w1, 33.f, kShirt);                                                    // cuff
        art::Cap(rig.elbow[i], Vector2Lerp(rig.elbow[i], rig.hand[i], 0.1f), 36.f, Shade(kJacket, 0.92f));
    }
}

void Puppet::DrawCarried() const {
    if (!carrying || !carriedBook || back) return;
    Vector2 m{ (rig.hand[0].x + rig.hand[1].x) * 0.5f, (rig.hand[0].y + rig.hand[1].y) * 0.5f - 4.f };
    carriedBook->DrawClosed(m, 82.f, 1.f, 0.f, false);
}

void Puppet::DrawHead() const {
    Vector2 hc = rig.head;
    float sy = SH_Y + rig.dy;
    rlPushMatrix();
    rlTranslatef(0, sy - 14.f, 0);
    rlRotatef(rig.tilt * RAD2DEG, 0, 0, 1);
    rlTranslatef(0, -(sy - 14.f), 0);
    for (int s = -1; s <= 1; s += 2) art::Ell(Vector2{ hc.x + s * 57.f, hc.y + 8.f }, 11.f, 15.f, kSkinShade);   // ears
    art::EllShaded(hc, 58.f, 65.f, kSkin);
    if (!back) {
        // hair: fluffy white sides, a few strands across the bald top
        for (int s = -1; s <= 1; s += 2) {
            art::Ell(Vector2{ hc.x + s * 50.f, hc.y - 6.f }, 21.f, 30.f, kHair);
            art::Ell(Vector2{ hc.x + s * 55.f, hc.y - 26.f }, 14.f, 15.f, kHair);
            art::Ell(Vector2{ hc.x + s * 53.f, hc.y + 22.f }, 13.f, 16.f, Shade(kHair, 0.94f));
            art::Ell(Vector2{ hc.x + s * 43.f, hc.y - 38.f }, 12.f, 12.f, Shade(kHair, 0.97f));
        }
        DrawSplineSegmentBezierQuadratic(Vector2{ hc.x - 34, hc.y - 56 }, Vector2{ hc.x, hc.y - 74 }, Vector2{ hc.x + 34, hc.y - 56 }, 3.f, WithAlpha(kHair, 0.9f));
        DrawSplineSegmentBezierQuadratic(Vector2{ hc.x - 22, hc.y - 60 }, Vector2{ hc.x + 6, hc.y - 70 }, Vector2{ hc.x + 24, hc.y - 60 }, 2.f, WithAlpha(kHair, 0.7f));
        for (int s = -1; s <= 1; s += 2) {
            art::Ell(Vector2{ hc.x + s * 34.f, hc.y + 26.f }, 14.f, 9.f, Color{ 238, 140, 130, 80 });         // blush
            art::Ell(Vector2{ hc.x + s * 27.f, hc.y - 31.f }, 22.f, 7.f, kHair, s * -9.f);                    // eyebrows
        }
        // glasses and eyes
        float closed = blink > 0.f ? sinf(PI * Clamp(blink / 0.17f, 0.f, 1.f)) : 0.f;
        for (int s = -1; s <= 1; s += 2) {
            Vector2 lc{ hc.x + s * 27.f, hc.y - 6.f };
            art::Ell(lc, 21.f, 21.f, Color{ 250, 252, 255, 255 });
            art::Cap(Vector2{ lc.x + rig.eye.x, lc.y + rig.eye.y }, Vector2{ lc.x + rig.eye.x, lc.y + rig.eye.y }, 11.f, Color{ 40, 30, 28, 255 });
            art::Ell(Vector2{ lc.x + rig.eye.x - 2.f, lc.y + rig.eye.y - 2.5f }, 2.4f, 2.4f, WHITE);
            if (closed > 0.02f) art::Ell(Vector2{ lc.x, lc.y - 20.f * (1.f - closed) + 1.f }, 22.f, 21.f * closed + 0.5f, kSkin);
            if (closed > 0.5f) DrawLineEx(Vector2{ lc.x - 12, lc.y + 4 }, Vector2{ lc.x + 12, lc.y + 4 }, 2.f, kJacketDark);
            DrawRing(lc, 18.5f, 22.5f, 0, 360, 40, Color{ 52, 34, 24, 255 });
            art::Ell(lc, 18.f, 18.f, Color{ 190, 220, 240, 36 });
        }
        DrawLineEx(Vector2{ hc.x - 6, hc.y - 10 }, Vector2{ hc.x + 6, hc.y - 10 }, 3.2f, Color{ 52, 34, 24, 255 });
        DrawLineEx(Vector2{ hc.x - 49, hc.y - 10 }, Vector2{ hc.x - 58, hc.y - 4 }, 2.6f, Color{ 52, 34, 24, 255 });
        DrawLineEx(Vector2{ hc.x + 49, hc.y - 10 }, Vector2{ hc.x + 58, hc.y - 4 }, 2.6f, Color{ 52, 34, 24, 255 });
        // nose, mouth, moustache
        art::EllShaded(Vector2{ hc.x, hc.y + 20.f }, 12.f, 14.f, Color{ 238, 168, 140, 255 });
        float open = 3.f + rig.mouth * 11.f;
        art::Ell(Vector2{ hc.x, hc.y + 49.f + open * 0.35f }, 13.f, open, Color{ 92, 30, 38, 255 });
        if (rig.mouth > 0.45f) art::Ell(Vector2{ hc.x, hc.y + 50.f + open * 0.7f }, 8.f, open * 0.45f, Color{ 214, 100, 110, 255 });
        art::Ell(Vector2{ hc.x - 21.f, hc.y + 38.f }, 27.f, 11.f, kHair, 12.f);
        art::Ell(Vector2{ hc.x + 21.f, hc.y + 38.f }, 27.f, 11.f, kHair, -12.f);
        DrawSplineSegmentBezierQuadratic(Vector2{ hc.x - 40, hc.y + 38 }, Vector2{ hc.x - 56, hc.y + 40 }, Vector2{ hc.x - 52, hc.y + 26 }, 4.f, kHair);
        DrawSplineSegmentBezierQuadratic(Vector2{ hc.x + 40, hc.y + 38 }, Vector2{ hc.x + 56, hc.y + 40 }, Vector2{ hc.x + 52, hc.y + 26 }, 4.f, kHair);
    } else {
        art::Ell(Vector2{ hc.x, hc.y + 16.f }, 54.f, 48.f, kHair);
        for (int s = -1; s <= 1; s += 2) { art::Ell(Vector2{ hc.x + s * 50.f, hc.y - 10.f }, 16.f, 22.f, kHair); art::Ell(Vector2{ hc.x + s * 40.f, hc.y - 38.f }, 12.f, 12.f, Shade(kHair, 0.96f)); }
        DrawSplineSegmentBezierQuadratic(Vector2{ hc.x - 30, hc.y - 52 }, Vector2{ hc.x, hc.y - 68 }, Vector2{ hc.x + 30, hc.y - 52 }, 3.f, WithAlpha(kHair, 0.8f));
    }
    rlPopMatrix();
}

void Puppet::Draw() const {
    if (turn < 0.02f) return;
    float s = Scale();
    rlPushMatrix();
    rlTranslatef(feet.x, feet.y, 0);
    rlScalef(s * turn, s, 1.f);
    art::Shadow(Vector2{ 0.f, 2.f }, 92.f, 15.f, 0.5f);
    DrawLegs();
    DrawTorso();
    DrawHead();
    DrawArms();
    DrawCarried();
    for (int i = 0; i < 2; i++) {
        art::EllShaded(rig.hand[i], 18.f, 19.f, kSkin);
        art::Ell(Vector2{ rig.hand[i].x + (i == 0 ? 11.f : -11.f), rig.hand[i].y - 6.f }, 6.f, 9.f, kSkinShade, i == 0 ? -20.f : 20.f);
    }
    rlPopMatrix();
}

void Puppet::DrawHandsOverDesk() const {
    if (turn < 0.02f) return;
    float s = Scale();
    rlPushMatrix();
    rlTranslatef(feet.x, feet.y, 0);
    rlScalef(s * turn, s, 1.f);
    DrawArms();
    DrawCarried();
    for (int i = 0; i < 2; i++) {
        art::EllShaded(rig.hand[i], 18.f, 19.f, kSkin);
        art::Ell(Vector2{ rig.hand[i].x + (i == 0 ? 11.f : -11.f), rig.hand[i].y - 6.f }, 6.f, 9.f, kSkinShade, i == 0 ? -20.f : 20.f);
    }
    rlPopMatrix();
}
