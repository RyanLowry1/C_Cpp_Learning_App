// librarian_draw.cpp - drawing the librarians every frame.
// Bodies are smooth swept surfaces (gfx::Lathe) and tubes (gfx::TubeSet) that follow the skeleton; heads are
// sculpted meshes from librarian_sculpt.cpp with the moving features (eyes, brows, mouth, glasses) laid on top.
#include "librarian.h"
#include "librarian_look.h"
#include "gfx.h"
#include <algorithm>
#include <initializer_list>

using util::RGBA;
using util::Shade;

namespace {
constexpr float TORSO = 0.56f, NECK = 0.035f, UPPER = 0.28f, FORE = 0.26f;
constexpr float THIGH = 0.37f, SHIN = 0.37f;
constexpr float SHOULDER_X = 0.20f, HIP_X = 0.072f;

// ---------------------------------------------------------------------------
// the pose of every joint, copied out of the Librarian for drawing
// ---------------------------------------------------------------------------
struct Rig {
    Matrix pelvis, spine, neck, head, shoulder[2], elbow[2], hand[2], hip[2], knee[2], foot[2];
    float muscle = 0.f, mouth = 0.f, brow = 0.f, blink = 0.f, clock = 0.f;
    float sculpt = 0.f;              // the fitness the torso is sculpted for: the target, so a change rebuilds it once, not at every step on the way
    bool dumbbells = false, fists = false;
    bool legs = true;                // legs are skipped while seated (the desk hides them)
};

// ---------------------------------------------------------------------------
// primitives
// ---------------------------------------------------------------------------
Matrix T(float x, float y, float z) { return MatrixTranslate(x, y, z); }
Matrix M(const Matrix& a, const Matrix& b) { return MatrixMultiply(a, b); }
float Hash(int i) { float s = sinf((float)i * 12.9898f + 3.1f) * 43758.5453f; return s - floorf(s); }
float Lf(float a, float b, float t) { return a + (b - a) * t; }

// ellipsoid (radii rx,ry,rz) centred at (x,y,z) in frame f, optionally rolled about Z
void E(const Matrix& f, float x, float y, float z, float rx, float ry, float rz, Color c, float rotZ = 0.f, float em = 0.f) {
    gfx::Sphere(M(M(M(MatrixScale(rx, ry, rz), MatrixRotateZ(rotZ)), T(x, y, z)), f), c, em);
}
void S(const Matrix& f, float x, float y, float z, float r, Color c, float em = 0.f) { E(f, x, y, z, r, r, r, c, 0.f, em); }
// cylinder of radius r and height h standing on (x,y,z), tilted by rotX/rotZ
void Cy(const Matrix& f, float x, float y, float z, float r, float h, Color c, float rotX = 0.f, float rotZ = 0.f, float em = 0.f) {
    gfx::Cylinder(M(M(M(M(MatrixScale(r, h, r), MatrixRotateX(rotX)), MatrixRotateZ(rotZ)), T(x, y, z)), f), c, em);
}

template <class RF>
void TubeC(const Matrix& f, Vector3 a, Vector3 b, RF rad, Color c, float em = 0.f) {
    // a smooth rod
    gfx::TubeSet ts;
    std::vector<Vector3> p;
    std::vector<float> r;
    for (int i = 0; i <= 6; i++) { float t = i / 6.f; p.push_back(Vector3Lerp(a, b, t)); r.push_back(rad(t)); }
    ts.Add(p, r, c);
    ts.Flush(7, f);
    (void)em;
}
// a smooth hoop around the Y axis at height y (zscale squashes its depth)
void Ring(const Matrix& f, float y, float radius, int n, float r, Color c, float zscale = 1.f, float cx = 0.f, float cz = 0.f, float em = 0.f) {
    (void)em;
    int pts = std::max(20, std::min(40, n * 2));
    std::vector<Vector3> loop;
    for (int k = 0; k < pts; k++) {
        float a = 2.f * PI * k / pts;
        loop.push_back(Vector3{ cx + cosf(a) * radius, y, cz + sinf(a) * radius * zscale });
    }
    gfx::TubeSet ts;
    ts.Add(loop, std::vector<float>(loop.size(), r), c, true);
    ts.Flush(6, f);
}

using look::Spec;
using look::MakeSpec;
const Color kGold = RGBA(244, 204, 104);

// ---------------------------------------------------------------------------
// torso: a lathe of thin ellipsoid discs; training broadens the chest and slims the paunch
// ---------------------------------------------------------------------------
struct TP { float hw, hd, zc; };
TP TorsoProfile(float y, float m, float belly) {
    static const float Y[9]  = { -0.03f, 0.05f, 0.13f, 0.22f, 0.31f, 0.40f, 0.47f, 0.52f, 0.555f };
    static const float HW[9] = { 0.165f, 0.170f, 0.165f, 0.178f, 0.200f, 0.222f, 0.215f, 0.170f, 0.085f };
    static const float HD[9] = { 0.115f, 0.122f, 0.125f, 0.128f, 0.128f, 0.126f, 0.112f, 0.095f, 0.075f };
    static const float ZC[9] = { 0, 0, 0.01f, 0.02f, 0.015f, 0.01f, -0.005f, -0.01f, -0.01f };
    static const float MW[9] = { 0, 0, -0.012f, 0, 0.03f, 0.07f, 0.075f, 0.05f, 0 };
    static const float MD[9] = { 0, 0, 0, 0.005f, 0.012f, 0.02f, 0.012f, 0, 0 };
    static const float BD[9] = { 0, 0.012f, 0.035f, 0.04f, 0.012f, 0, 0, 0, 0 };
    int i = 0;
    while (i < 7 && y > Y[i + 1]) i++;
    float t = Clamp((y - Y[i]) / (Y[i + 1] - Y[i]), 0.f, 1.f);
    auto L = [&](const float* a) { return a[i] + (a[i + 1] - a[i]) * t; };
    TP p;
    p.hw = L(HW) + m * L(MW);
    p.hd = L(HD) + m * L(MD) + belly * L(BD);
    p.zc = L(ZC) + belly * L(BD) * 0.5f;
    return p;
}

// fingers: each is three spheres bending towards +Z (the way a fist closes). curl 0 = relaxed, 1 = gripping a bar
void Fingers(gfx::TubeSet& ts, int count, float spread, float curl, Color c, float r0) {
    for (int k = 0; k < count; k++) {
        float u = count == 1 ? 0.f : (float)k / (count - 1) - 0.5f;
        float x = u * spread, len = 0.021f - 0.0035f * fabsf(u) * 2.f + (count == 3 ? 0.004f : 0.f);
        float a = 0.18f + 0.22f * Hash(k + 7), y = -0.066f, z = 0.0f;
        std::vector<Vector3> pts{ Vector3{ x, y, z } };
        for (int j = 0; j < 3; j++) {
            a += (0.3f + 0.45f * curl) * (j == 0 ? 0.7f : 1.f);
            float l = len * (j == 2 ? 0.8f : 1.f);
            y -= cosf(a) * l;
            z += sinf(a) * l;
            pts.push_back(Vector3{ x, y, z });
        }
        ts.Taper(pts, r0, r0 * 0.95f, r0 * 0.7f, c);   // one smooth finger
    }
}
void Thumb(gfx::TubeSet& ts, float side, float curl, Color c, float r0) {
    float a = 0.9f + 0.25f * curl, x = side * 0.03f, y = -0.032f, z = 0.012f;
    std::vector<Vector3> pts{ Vector3{ x, y, z } };
    for (int j = 0; j < 3; j++) {
        a += 0.18f + 0.18f * curl;
        x -= side * 0.004f;
        y -= cosf(a * 0.75f) * 0.016f;
        z += sinf(a * 0.75f) * 0.016f;
        pts.push_back(Vector3{ x, y, z });
    }
    ts.Taper(pts, r0 * 1.05f, r0, r0 * 0.7f, c);
}
// a dumbbell across the fist (axis along the hand's X)
void Dumbbell(const Matrix& f) {
    Color iron = RGBA(74, 76, 90), steel = RGBA(170, 172, 182), plate = RGBA(110, 112, 126);
    float by = -0.078f, bz = 0.034f;   // where the closed fist holds it (see SculptHand)
    Cy(f, 0.11f, by, bz, 0.0105f, 0.22f, steel, 0.f, PI * 0.5f);   // bar
    for (int e = -1; e <= 1; e += 2) {
        float x = e * 0.105f;
        Cy(f, x + 0.011f, by, bz, 0.062f, 0.022f, iron, 0.f, PI * 0.5f);
        Cy(f, x + e * 0.024f + 0.008f, by, bz, 0.048f, 0.016f, plate, 0.f, PI * 0.5f);
        S(f, e * 0.138f, by, bz, 0.016f, steel);
        for (int k = 0; k < 6; k++) {   // bolts around the plate
            float a = 2.f * PI * k / 6;
            S(f, x + e * 0.001f, by + cosf(a) * 0.05f, bz + sinf(a) * 0.05f, 0.0045f, steel);
        }
    }
}

// ---------------------------------------------------------------------------
// smooth swept surfaces: limbs, torso, neck are single lathe meshes (see gfx::Lathe)
// ---------------------------------------------------------------------------
using gfx::LatheRing;
enum Slot { kTorso = 0, kNeck = 1, kUpperArm = 2, kForearm = 4, kThigh = 6, kShin = 8, kVest = 10, kLapel = 11, kPelvis = 13, kExtra = 14 };

// rings along the local Y axis from y0 to y1; rx/rz are radii, cx/cz the centre offset, col the colour.
// `seams` are parameters (0..1) where the colour changes abruptly.
template <class RX, class RZ, class CX, class CZ, class CF>
void Sweep(int slot, const Matrix& f, float y0, float y1, int n, RX rx, RZ rz, CX cx, CZ cz, CF col, std::initializer_list<float> seams = {}) {
    std::vector<float> ts;
    for (int i = 0; i <= n; i++) ts.push_back((float)i / n);
    for (float s : seams) { ts.push_back(s); ts.push_back(s + 0.0015f); }
    std::sort(ts.begin(), ts.end());
    ts.erase(std::unique(ts.begin(), ts.end()), ts.end());
    std::vector<LatheRing> rg;
    for (float t : ts) rg.push_back(LatheRing{ Lf(y0, y1, t), cx(t), cz(t), rx(t), rz(t), col(t) });
    gfx::Lathe(slot, f, rg, 28);
}
inline float Zero(float) { return 0.f; }

// ---------------------------------------------------------------------------
// the human body
// ---------------------------------------------------------------------------
void DrawHumanBody(const Rig& R, const Spec& L, const std::string& id) {
    const float m = R.muscle;
    const bool robe = id == "pointer", coat = L.labCoat, hoodie = L.hood;

    // ---- legs: straight trouser legs with a turn-up, socks and round-toed shoes ----
    for (int i = 0; i < 2 && R.legs; i++) {
        auto thighR = [&](float t) { return 0.072f - 0.008f * t + 0.006f * m * (1.f - t); };
        Sweep(kThigh + i, R.hip[i], 0.f, -THIGH, 8, thighR, thighR, Zero, Zero, [&](float) { return L.trousers; });
        S(R.knee[i], 0, 0, 0, 0.066f, L.trousers);
        auto shinR = [&](float t) { return 0.066f - 0.01f * t; };
        Sweep(kShin + i, R.knee[i], 0.f, -SHIN + 0.035f, 8, shinR, shinR, Zero, Zero, [&](float) { return L.trousers; });
        Ring(R.knee[i], -SHIN + 0.04f, 0.057f, 14, 0.0075f, Shade(L.trousers, 0.88f));
        E(R.foot[i], 0, 0.0f, -0.004f, 0.032f, 0.046f, 0.036f, Shade(L.accent, 0.92f));          // sock
        E(R.foot[i], 0, -0.06f, 0.04f, 0.05f, 0.012f, 0.112f, Shade(L.shoes, 0.55f));            // sole
        E(R.foot[i], 0, -0.036f, 0.03f, 0.047f, 0.032f, 0.098f, L.shoes);
        S(R.foot[i], 0, -0.038f, 0.108f, 0.042f, L.shoes);                                       // round toe
    }
    // ---- pelvis ----
    E(R.pelvis, 0, -0.03f, 0, 0.128f, 0.092f, 0.1f, L.trousers);
    for (int s = -1; s <= 1; s += 2) S(R.pelvis, s * HIP_X, 0, 0, 0.072f + 0.006f * m, L.trousers);

    // ---- torso: one sculpt (jacket, waistcoat, lapels, collar, shoulders); thin details are laid on its surface ----
    const look::Sculpt& torso = look::GetTorso(id, R.sculpt);
    for (const Mesh& mesh : torso.meshes) gfx::DrawMeshLit(mesh, R.spine, WHITE);
    auto zf = [&](float x, float y) { return torso.Z(x, y); };
    gfx::TubeSet bt;
    if (hoodie) {   // zip and drawstrings
        std::vector<Vector3> zp;
        for (int k = 0; k < 12; k++) { float y = 0.06f + k * 0.04f; zp.push_back(Vector3{ 0.f, y, zf(0, y) + 0.001f }); }
        bt.Add(zp, std::vector<float>(zp.size(), 0.003f), RGBA(220, 224, 230));
        S(R.spine, 0.006f, 0.5f, zf(0, 0.5f) + 0.002f, 0.006f, RGBA(220, 224, 230));
        for (int s = -1; s <= 1; s += 2) {
            std::vector<Vector3> dp;
            for (int j = 0; j < 7; j++) { float y = 0.5f - j * 0.022f, x = s * (0.03f + 0.003f * j); dp.push_back(Vector3{ x, y, zf(x, y) + 0.003f }); }
            bt.Add(dp, std::vector<float>(dp.size(), 0.0034f), RGBA(250, 244, 226));
            S(R.spine, s * 0.049f, 0.362f, zf(s * 0.049f, 0.362f) + 0.004f, 0.007f, L.accent);
        }
    }
    if (L.chain) {   // pocket-watch chain across the waistcoat
        std::vector<Vector3> cp;
        for (int k = 0; k < 12; k++) { float u = k / 11.f, x = 0.06f - 0.1f * u, y = 0.215f - 0.03f * sinf(PI * u); cp.push_back(Vector3{ x, y, zf(x, y) + 0.003f }); }
        bt.Add(cp, std::vector<float>(cp.size(), 0.003f), kGold);
        E(R.spine, 0.062f, 0.215f, zf(0.062f, 0.215f) + 0.003f, 0.011f, 0.011f, 0.005f, kGold);
    }
    if (coat) {   // lab coat: pens in the breast pocket, an ID badge and a stethoscope
        for (int k = 0; k < 3; k++) {
            Color pc = k == 0 ? RGBA(214, 52, 52) : k == 1 ? RGBA(54, 94, 214) : RGBA(40, 40, 46);
            float x = -0.117f + 0.012f * k;
            bt.Add({ Vector3{ x, 0.345f, zf(x, 0.345f) + 0.004f }, Vector3{ x, 0.385f, zf(x, 0.385f) + 0.005f } }, { 0.0044f, 0.0044f }, pc);
        }
        // ID badge: a flat card clipped to the coat, turned to lie on the curve of the chest
        float zb = zf(0.105f, 0.3f);
        const Matrix card = M(M(MatrixRotateY(atanf((zf(0.085f, 0.3f) - zf(0.125f, 0.3f)) / 0.04f)), T(0.105f, 0.3f, zb + 0.004f)), R.spine);
        gfx::Cube(M(MatrixScale(0.04f, 0.052f, 0.004f), card), RGBA(250, 250, 246));
        gfx::Cube(M(M(MatrixScale(0.04f, 0.014f, 0.005f), T(0, 0.019f, 0)), card), L.accent);
        gfx::Cube(M(M(MatrixScale(0.012f, 0.01f, 0.007f), T(0, 0.029f, 0)), card), RGBA(60, 64, 76));
        std::vector<Vector3> sp;
        for (int k = 0; k < 24; k++) { float a = PI * k / 23.f, x = cosf(a) * 0.07f, y = 0.5f - sinf(a) * 0.13f; sp.push_back(Vector3{ x, y, zf(x, y) + 0.005f }); }
        bt.Add(sp, std::vector<float>(sp.size(), 0.0044f), RGBA(34, 36, 44));
        E(R.spine, 0.f, 0.36f, zf(0, 0.36f) + 0.006f, 0.015f, 0.015f, 0.006f, RGBA(190, 194, 204));
    }
    bt.Flush(4, R.spine);

    // ---- arms: smooth muscular sweeps; sleeves roll up, then come off ----
    // sleeves change with the torso sculpt, which is rebuilt in steps of 5% fitness (see look::GetTorso)
    const float mq = floorf(R.sculpt * 20.f + 0.5f) / 20.f;
    const bool bare = mq >= 0.8f, rolled = mq >= 0.45f && mq < 0.8f;
    for (int i = 0; i < 2; i++) {
        auto bump = [&](float t) { return (0.006f + 0.026f * m) * expf(-powf((t - 0.42f) / 0.24f, 2.f)); };
        auto upperR = [&](float t) { return 0.043f + 0.014f * m - 0.006f * t + 0.016f * m * expf(-t * t / 0.04f); };   // fuller under the shoulder
        Sweep(kUpperArm + i, R.shoulder[i], 0.f, -UPPER, 14,
              [&](float t) { return upperR(t) + 0.6f * bump(t); },                 // side to side
              [&](float t) { return upperR(t) + 1.3f * bump(t); },                 // front to back: the biceps / triceps
              Zero, [&](float t) { return 0.35f * bump(t); },
              [&](float) { return bare ? L.skin : L.jacket; });   // bare arms: the sculpted torso has bare shoulders to match
        float flare = (robe && !bare && !rolled) ? 1.f : 0.f;   // wide wizard sleeves, while he still has sleeves
        auto foreBase = [&](float t) { return 0.036f + 0.012f * m - 0.011f * t + 0.012f * m * expf(-powf((t - 0.15f) / 0.2f, 2.f)); };
        auto foreR = [&](float t) { return foreBase(t) + flare * 0.04f * t * t; };
        Sweep(kForearm + i, R.elbow[i], 0.f, -FORE, 12, foreR, [&](float t) { return foreBase(t) * 0.92f + flare * 0.04f * t * t; }, Zero, Zero,
              [&](float t) { return (!bare && !rolled) ? L.jacket : (rolled ? (t <= 0.12f ? L.jacket : L.skin) : L.skin); }, { 0.12f });
        S(R.elbow[i], 0, 0, 0, 0.038f + 0.014f * m, (bare || rolled) ? L.skin : L.jacket);
        if (rolled) Ring(R.elbow[i], -0.03f, 0.046f + 0.012f * m, 14, 0.0115f, L.jacket);       // rolled-up sleeve cuff
        if (!bare && !rolled) {
            if (robe) Ring(R.elbow[i], -FORE + 0.012f, 0.07f, 18, 0.0075f, L.accent, 0.95f);
            else Ring(R.elbow[i], -FORE + 0.008f, 0.036f + 0.004f * m, 14, 0.0082f, coat ? L.vest : L.shirt);   // shirt cuff
        }
        // hand: a sculpted cartoon hand, drawn a little large. A resting hand turns its palm towards the body
        // (thumb forward); a hand gripping a dumbbell keeps its palm forward so the bar lies across the body.
        const bool closed = R.dumbbells || R.fists;
        const float turn = closed ? 0.f : (i == 0 ? 0.5f * PI : -0.5f * PI);
        const Matrix hm = M(M(MatrixScale(1.12f, 1.12f, 1.12f), MatrixRotateY(turn)), R.hand[i]);
        for (const Mesh& mesh : look::GetHand(id, i, closed).meshes) gfx::DrawMeshLit(mesh, hm, WHITE);
        if (R.dumbbells) Dumbbell(hm);
    }
}

// ---------------------------------------------------------------------------
// the human head: one sculpted mesh (librarian_sculpt.cpp) plus the features that move, laid on top
// ---------------------------------------------------------------------------
// outline of one lens for each style of glasses, as an offset from its centre (a = 0 .. 2 PI)
Vector2 Rim(int style, float a) {
    float c = cosf(a), s = sinf(a);
    if (style == 2) return Vector2{ 0.027f * c, s < 0.f ? 0.019f * s : 0.004f * s };   // half-moon: flat on top
    if (style == 3) return Vector2{ 0.035f * copysignf(sqrtf(fabsf(c)), c), 0.028f * copysignf(sqrtf(fabsf(s)), s) };   // rounded square
    float r = style == 4 ? 0.038f : 0.0345f;
    return Vector2{ r * c, r * s };
}

void DrawHumanHead(const Rig& R, const Spec& L, const std::string& id) {
    const look::Head& A = look::GetHead(id);
    const look::Face& f = A.face;
    const Matrix H = M(M(MatrixScale(look::kHeadScale, look::kHeadScale, look::kHeadScale), T(0, -0.016f, 0)), R.head);
    for (const Mesh& mesh : A.meshes) gfx::DrawMeshLit(mesh, H, WHITE);
    gfx::TubeSet T;
    auto P = [&](float x, float y, float dz) { return Vector3{ x, y, A.Z(x, y) + dz }; };
    const Color ink = RGBA(58, 38, 32);   // lash lines and the line of the mouth

    // ---- eyes: a white ball, a rim of colour, a big dark pupil and a sparkle ----
    const float open = R.blink > 0 ? Clamp(fabsf(R.blink - 0.08f) / 0.08f, 0.f, 1.f) : 1.f;
    const float re = f.eyeR, lidR = re + 0.0026f;
    for (int s = -1; s <= 1; s += 2) {
        float ex = s * f.eyeX, ey = f.eyeY, ez = f.eyeZ;
        S(H, ex, ey, ez, re, RGBA(252, 250, 244));
        S(H, ex, ey, ez + 0.139f * re, 0.9f * re, Shade(L.iris, 0.9f));        // caps that hug the eyeball
        S(H, ex, ey, ez + 0.263f * re, 0.8f * re, RGBA(26, 20, 22));
        if (open > 0.6f) {   // sparkles (they would poke through a closing lid)
            E(H, ex - 0.005f, ey + 0.0058f, ez + 0.975f * re, 0.004f, 0.004f, 0.0016f, WHITE, 0.f, 1.f);
            E(H, ex + 0.0044f, ey - 0.0048f, ez + 0.99f * re, 0.0019f, 0.0019f, 0.0012f, WHITE, 0.f, 1.f);
        }
        // lids: two domes over the eyeball hinged at the corners of the eye, so their edges are arcs. The lower
        // one stays put; the upper one swings down to meet it in a blink, leaving the lash line as a closed eye.
        // `th` is how high the upper edge sits at the middle of the eye.
        const float low = -44.f * DEG2RAD;
        float th = Lf(low * RAD2DEG + 1.f, f.lidOpen, open) * DEG2RAD;
        const Matrix eyeAt = M(MatrixTranslate(ex, ey, ez), H);
        gfx::Hemi(M(M(MatrixScale(lidR, lidR, lidR), MatrixRotateX(-th)), eyeAt), Shade(L.skin, 0.95f));
        gfx::Hemi(M(M(MatrixScale(lidR, lidR, lidR), MatrixRotateX(PI - low)), eyeAt), Shade(L.skin, 0.98f));
        std::vector<Vector3> lash;   // lash line along the lid's edge
        for (int k = 0; k <= 10; k++) {
            float a = (18.f + 14.4f * k) * DEG2RAD;
            lash.push_back(Vector3{ ex + lidR * cosf(a), ey + lidR * sinf(a) * sinf(th), ez + lidR * sinf(a) * cosf(th) + 0.0003f });
        }
        T.Taper(lash, 0.0007f, (L.lashes ? 0.0019f : 0.0013f) * (1.7f - 0.7f * open), 0.0007f, ink);   // bolder as the eye closes
        if (L.lashes) {   // a flick at the outer corner
            Vector3 c = lash[(size_t)(s > 0 ? 1 : 9)];
            T.Taper({ c, Vector3{ c.x + s * 0.005f, c.y + 0.004f, c.z - 0.003f }, Vector3{ c.x + s * 0.009f, c.y + 0.009f, c.z - 0.007f } }, 0.0014f, 0.0012f, 0.0004f, ink);
        }
        // eyebrow: one soft tapered stroke; it lifts when he talks
        std::vector<Vector3> bp;
        for (int k = 0; k < 6; k++) {
            float u = k / 5.f, x = s * (0.015f + 0.05f * u);
            float y = f.browY + R.brow * 0.011f + 0.009f * sinf(PI * (0.15f + 0.75f * u)) + L.browSlope * 0.011f * (1.f - u);
            bp.push_back(P(x, y, 0.0045f));
        }
        T.Taper(bp, 0.004f * L.browBulk, 0.0066f * L.browBulk, 0.003f * L.browBulk, L.brow);
    }

    // ---- glasses ----
    if (L.glassesStyle) {
        const int gs = L.glassesStyle;
        float cy = gs == 2 ? f.eyeY - 0.024f : f.eyeY - 0.001f;   // half-moons sit low on the nose
        float cx = gs == 2 ? 0.034f : f.eyeX + 0.001f;
        float zg = (gs == 2 ? A.Z(0, cy) + 0.006f : f.eyeZ + f.eyeR) + 0.012f;
        float tr = gs == 3 ? 0.0033f : (gs == 4 ? 0.0023f : 0.0028f);
        auto wrap = [&](float x) { return zg - 1.8f * (x * x - cx * cx); };   // the frame follows the curve of the face
        float xo = cx + Rim(gs, 0.f).x, xi = cx + Rim(gs, PI).x, by = gs == 2 ? 0.002f : 0.006f;
        for (int s = -1; s <= 1; s += 2) {
            std::vector<Vector3> ring;
            for (int k = 0; k < 44; k++) {
                Vector2 o = Rim(gs, 2.f * PI * k / 44);
                ring.push_back(Vector3{ s * (cx + o.x), cy + o.y, wrap(cx + o.x) });
            }
            T.Add(ring, std::vector<float>(ring.size(), tr), L.glasses, true);
            T.Add({ Vector3{ s * xo, cy + 0.004f, wrap(xo) }, Vector3{ s * (f.earX - 0.004f), 0.139f, 0.03f }, Vector3{ s * (f.earX + 0.002f), 0.138f, -0.004f }, Vector3{ s * (f.earX + 0.001f), 0.124f, -0.02f } },
                  { tr, tr * 0.9f, tr * 0.9f, tr * 0.7f }, L.glasses);
        }
        std::vector<Vector3> bridge;   // a low arch over the nose
        for (int k = -3; k <= 3; k++) { float u = k / 3.f; bridge.push_back(Vector3{ u * xi, cy + by + 0.004f * (1.f - u * u), wrap(xi) + 0.002f * (1.f - u * u) }); }
        T.Add(bridge, std::vector<float>(bridge.size(), tr), L.glasses);
    }

    // ---- mouth: a line that curves into a smile; it opens on a dark mouth with a row of teeth and a tongue ----
    {
        float my = f.mouthY, mw = f.mouthW, o = std::max(R.mouth, f.restOpen), drop = 0.022f * o;
        // a curve across the mouth: v = 0 is the upper lip, 1 the lower lip
        auto edge = [&](float v, float dz, float shrink, float dy) {
            std::vector<Vector3> c;
            for (int k = -6; k <= 6; k++) {
                float u = k / 6.f * shrink;
                c.push_back(P(u * mw, my + f.smile * u * u - v * drop * (1.f - u * u) + dy, dz));
            }
            return c;
        };
        std::vector<float> thin(13);
        for (int k = 0; k < 13; k++) { float u = (k - 6) / 6.f; thin[(size_t)k] = 0.0006f + 0.0011f * (1.f - u * u); }
        if (o > 0.04f) {
            gfx::TubeSet in;   // drawn unlit: the inside of a mouth should be flat colour, not shiny ridges
            float r = drop * 0.1f + 0.0004f, face = 0.0004f + r;   // thin strips; `face` is how far they stand off the skin
            for (int j = 0; j < 8; j++) in.Add(edge((j + 0.5f) / 8.f, 0.0004f, 0.97f, 0.f), std::vector<float>(13, r), RGBA(98, 30, 36));
            float rt = std::min(0.0032f, drop * 0.2f), rg = std::min(0.004f, drop * 0.24f);
            std::vector<float> tongue(13);
            for (int k = 0; k < 13; k++) { float u = (k - 6) / 6.f; tongue[(size_t)k] = rg * (0.6f + 0.4f * (1.f - u * u)); }
            in.Add(edge(0.17f, face + 0.0006f - rt, 0.78f, 0.f), std::vector<float>(13, rt), RGBA(250, 248, 240));   // a row of teeth under the upper lip
            in.Add(edge(0.82f, face + 0.0005f - rg, 0.5f, 0.f), tongue, RGBA(228, 112, 118));                        // the tongue
            in.Flush(3, H, 1.f);
        }
        T.Add(edge(0.f, 0.0022f, 1.f, 0.f), thin, ink);
        float lr = L.lashes ? 0.0036f : 0.0026f;   // lower lip: a soft stroke just under the mouth
        T.Taper(edge(1.f, 0.0014f, 0.68f, -lr - 0.0006f), 0.0008f, lr, 0.0008f, L.lip);
    }

    // ---- Professor Pointer's hat: one smooth cone bending at the tip, gold bands and stars ----
    if (L.hat) {
        std::vector<LatheRing> rg;
        for (int k = 0; k <= 26; k++) {
            float t = k / 26.f, r = 0.118f * powf(1.f - t, 0.85f) + 0.004f;
            bool band = (t > 0.03f && t <= 0.075f) || (t > 0.1f && t <= 0.125f);
            LatheRing ring{ 0.205f + 0.34f * t, 0.02f * t * t, -0.01f - 0.1f * t * t * t - 0.012f * t, r + (band ? 0.0035f : 0.f), r * 0.99f + (band ? 0.0035f : 0.f), band ? L.accent : L.jacket };
            if (k > 0 && (ring.c.r != rg.back().c.r || ring.c.g != rg.back().c.g)) {   // a hard colour edge where a band starts or ends
                LatheRing e = ring;
                e.y -= 0.0004f; e.c = rg.back().c;
                rg.push_back(e);
            }
            rg.push_back(ring);
        }
        gfx::Lathe(kExtra + 2, H, rg, 36);
        E(H, 0, 0.206f, -0.008f, 0.145f, 0.0105f, 0.152f, L.jacketDark);   // brim
        S(H, 0.02f, 0.548f, -0.114f, 0.014f, L.accent);                    // little bell at the tip
        for (int k = 0; k < 3; k++) {
            float t = 0.22f + 0.2f * k, ang = 0.5f - 0.5f * k;
            float r = 0.118f * powf(1.f - t, 0.85f) + 0.004f;
            S(H, r * sinf(ang) + 0.02f * t * t, 0.205f + 0.34f * t, -0.01f - 0.1f * t * t * t - 0.012f * t + r * cosf(ang) + 0.005f, 0.0095f, L.accent, 0.6f);
        }
    }
    T.Flush(0, H);
}

// ---------------------------------------------------------------------------
// the robot: B.Y.T.E.
// ---------------------------------------------------------------------------
float VisorZ(float x, float y) {
    float u = x / 0.093f, v = (y - 0.116f) / 0.07f;
    return 0.074f + 0.04f * sqrtf(std::max(0.02f, 1.f - u * u - v * v));
}

void DrawRobot(const Rig& R) {
    const float m = R.muscle;
    const Color shell = RGBA(228, 232, 238), dark = RGBA(58, 66, 82), steel = RGBA(150, 160, 176);
    const Color teal = RGBA(36, 168, 182), glow = RGBA(96, 232, 255), amber = RGBA(255, 176, 56);
    const float pulse = 0.65f + 0.35f * sinf(R.clock * 3.f);
    auto zf = [&](float y) { TP p = TorsoProfile(y, m, 0.f); return p.zc + p.hd * 0.97f; };

    // ---- legs ----
    for (int i = 0; i < 2 && R.legs; i++) {
        auto thighR = [&](float t) { return 0.062f - 0.012f * t + 0.006f * m; };
        Sweep(kThigh + i, R.hip[i], 0.f, -THIGH, 10, thighR, thighR, Zero, Zero, [&](float t) { return (t > 0.18f && t <= 0.24f) || (t > 0.48f && t <= 0.54f) || (t > 0.78f && t <= 0.84f) ? steel : dark; },
              { 0.18f, 0.24f, 0.48f, 0.54f, 0.78f, 0.84f });
        S(R.knee[i], 0, 0, 0, 0.056f, steel);
        S(R.knee[i], 0, 0, 0.012f, 0.04f, dark);
        auto shinR = [&](float t) { return 0.05f - 0.012f * t + 0.016f * sinf(PI * std::min(1.f, t * 1.3f)); };
        Sweep(kShin + i, R.knee[i], 0.f, -SHIN, 10, shinR, shinR, Zero, Zero, [&](float) { return shell; });
        TubeC(R.knee[i], Vector3{ 0, -0.04f, 0.056f }, Vector3{ 0, -0.34f, 0.044f }, [](float) { return 0.0075f; }, steel);   // shin piston
        E(R.foot[i], 0, -0.058f, 0.04f, 0.05f, 0.013f, 0.112f, dark);
        E(R.foot[i], 0, -0.03f, 0.035f, 0.048f, 0.034f, 0.104f, shell);
        S(R.foot[i], 0, -0.038f, 0.12f, 0.04f, teal);
        for (int k = 0; k < 3; k++) E(R.foot[i], 0, -0.02f - 0.004f * k, 0.0f + 0.04f * k, 0.036f, 0.005f, 0.008f, dark);
    }
    // ---- pelvis: ribbed ----
    {
        std::vector<LatheRing> rg;
        for (int k = 0; k <= 12; k++) {
            float t = k / 12.f;
            rg.push_back(LatheRing{ Lf(-0.11f, 0.08f, t), 0.f, 0.f, (0.152f + 0.012f * sinf(PI * t)) * (k % 2 ? 0.965f : 1.f), (0.106f + 0.01f * sinf(PI * t)) * (k % 2 ? 0.965f : 1.f), k % 2 ? dark : steel });
        }
        gfx::Lathe(kPelvis, R.pelvis, rg, 28);
    }
    for (int s = -1; s <= 1; s += 2) { S(R.pelvis, s * HIP_X, 0, 0, 0.075f, steel); S(R.pelvis, s * HIP_X, 0, 0.0, 0.052f, dark); }

    // ---- torso: smooth shell with a ribbed accordion at the waist ----
    {
        std::vector<LatheRing> rg;
        for (int k = 0; k < 44; k++) {
            float y = Lf(-0.05f, 0.56f, k / 43.f);
            TP p = TorsoProfile(y, m, 0.f);
            bool rib = y < 0.17f;
            float f = rib ? (k % 2 ? 0.955f : 1.f) : 1.f;
            Color c = rib ? (k % 2 ? dark : steel) : (y > 0.52f ? dark : shell);
            if (!rib && k > 0 && Lf(-0.05f, 0.56f, (k - 1) / 43.f) < 0.17f) c = shell;
            rg.push_back(LatheRing{ y, 0.f, p.zc, p.hw * 0.97f * f, p.hd * 0.97f * f, c });
        }
        rg.push_back(LatheRing{ 0.585f, 0.f, -0.01f, 0.05f, 0.045f, dark });
        gfx::Lathe(kTorso, R.spine, rg, 32);
    }
    // centre seam of tiny dark beads
    {
        gfx::TubeSet sm;
        std::vector<Vector3> seam;
        for (int k = 0; k < 14; k++) { float y = 0.19f + k * 0.029f; seam.push_back(Vector3{ 0.f, y, zf(y) + 0.0015f }); }
        sm.Add(seam, std::vector<float>(seam.size(), 0.0026f), dark);
        sm.Flush(5, R.spine);
    }
    // reactor core: glowing disc ringed with bolts
    float cy = 0.36f, cz = zf(cy);
    E(R.spine, 0, cy, cz + 0.002f, 0.056f, 0.056f, 0.012f, dark);
    E(R.spine, 0, cy, cz + 0.008f, 0.04f, 0.04f, 0.008f, glow, 0.f, 0.55f + 0.45f * pulse);
    S(R.spine, 0, cy, cz + 0.013f, 0.014f, RGBA(255, 255, 255), 1.f);
    {
        gfx::TubeSet ct;
        std::vector<Vector3> ring;
        for (int k = 0; k < 32; k++) { float a = 2.f * PI * k / 32; ring.push_back(Vector3{ cosf(a) * 0.058f, cy + sinf(a) * 0.058f, cz + 0.004f }); }
        ct.Add(ring, std::vector<float>(ring.size(), 0.0052f), steel, true);
        ct.Flush(3, R.spine);
    }
    for (int k = 0; k < 8; k++) {
        float a = 2.f * PI * k / 8 + 0.2f;
        S(R.spine, cosf(a) * 0.047f, cy + sinf(a) * 0.047f, cz + 0.011f, 0.003f, teal, 0.5f);
    }
    // status display: equaliser bars
    for (int k = 0; k < 9; k++) {
        float h = 0.006f + 0.016f * (0.5f + 0.5f * sinf(R.clock * 4.f + k * 1.1f)) * (0.8f + 0.5f * R.mouth);
        E(R.spine, (k - 4) * 0.0145f, 0.255f + h * 0.5f, zf(0.255f) + 0.003f, 0.0052f, h, 0.005f, k % 3 == 0 ? amber : glow, 0.f, 0.8f);
    }
    for (int s = -1; s <= 1; s += 2) for (int k = 0; k < 4; k++)   // side vents
        E(R.spine, s * (0.2f + 0.035f * m + 0.012f), 0.28f + k * 0.026f, 0.0, 0.006f, 0.0048f, 0.07f, dark);
    // neck: stacked rings and cables
    for (int k = 0; k < 4; k++) E(R.neck, 0, -0.045f + k * 0.022f, 0, 0.052f + 0.008f * m - 0.003f * k, 0.014f, 0.052f + 0.008f * m - 0.003f * k, k % 2 ? steel : dark);
    for (int s = -1; s <= 1; s += 2) TubeC(R.neck, Vector3{ s * 0.03f, -0.05f, -0.03f }, Vector3{ s * 0.034f, 0.01f, -0.036f }, [](float) { return 0.0065f; }, amber);
    // shoulders: pauldrons with rivets
    for (int s = -1; s <= 1; s += 2) {
        for (int k = 0; k < 6; k++) { float u = k / 5.f; S(R.spine, s * Lf(0.07f, 0.17f, u), Lf(0.545f, 0.5f, u), -0.012f, 0.038f + 0.008f * m, dark); }
        S(R.spine, s * SHOULDER_X, TORSO - 0.06f, 0, 0.074f + 0.036f * m, shell);
        S(R.spine, s * (SHOULDER_X + 0.02f), TORSO - 0.06f, 0, 0.056f + 0.03f * m, teal);
        for (int k = 0; k < 8; k++) {
            float a = 2.f * PI * k / 8;
            S(R.spine, s * (SHOULDER_X + 0.052f + 0.02f * m), TORSO - 0.06f + cosf(a) * 0.046f, sinf(a) * 0.046f, 0.0046f, steel);
        }
    }
    // ---- arms: hydraulic ----
    for (int i = 0; i < 2; i++) {
        float sd = i == 0 ? -1.f : 1.f;
        auto bump = [&](float t) { return (0.008f + 0.02f * m) * expf(-powf((t - 0.4f) / 0.25f, 2.f)); };
        auto upR = [&](float t) { return 0.044f + 0.012f * m + bump(t); };
        Sweep(kUpperArm + i, R.shoulder[i], 0.f, -UPPER, 14, upR, upR, Zero, Zero,
              [&](float t) { return ((t > 0.3f && t <= 0.37f) || (t > 0.62f && t <= 0.68f)) ? teal : shell; }, { 0.3f, 0.37f, 0.62f, 0.68f });
        TubeC(R.shoulder[i], Vector3{ 0, -0.02f, 0.052f + 0.014f * m }, Vector3{ 0, -0.25f, 0.05f + 0.014f * m }, [](float) { return 0.0068f; }, steel);   // biceps piston
        if (m > 0.3f) for (int k = 0; k < 5; k++) E(R.shoulder[i], 0, -0.1f - 0.025f * k, 0.052f + 0.03f * m, 0.012f, 0.006f, 0.005f, glow, 0.f, 0.8f);
        S(R.elbow[i], 0, 0, 0, 0.05f + 0.01f * m, steel);
        S(R.elbow[i], 0, 0, 0.0, 0.034f, dark);
        auto fR = [&](float t) { return 0.038f + 0.012f * m - 0.01f * t + 0.012f * expf(-powf((t - 0.2f) / 0.2f, 2.f)); };
        Sweep(kForearm + i, R.elbow[i], 0.f, -FORE, 12, fR, fR, Zero, Zero,
              [&](float t) { return ((t > 0.28f && t <= 0.34f) || (t > 0.6f && t <= 0.66f)) ? teal : (t > 0.92f ? dark : shell); }, { 0.28f, 0.34f, 0.6f, 0.66f, 0.92f });
        Ring(R.elbow[i], -FORE + 0.01f, 0.036f, 14, 0.0085f, dark);
        TubeC(R.elbow[i], Vector3{ 0, -0.02f, 0.045f + 0.008f * m }, Vector3{ 0, -0.2f, 0.038f }, [](float) { return 0.006f; }, steel);
        // claw hand: palm, three fingers, opposing thumb
        float curl = (R.dumbbells || R.fists) ? 1.f : 0.3f;
        E(R.hand[i], 0, -0.038f, 0.0f, 0.034f, 0.034f, 0.02f, dark);
        S(R.hand[i], 0, -0.035f, 0.016f, 0.012f, glow, 0.7f);
        {
            gfx::TubeSet hs;
            Fingers(hs, 3, 0.04f, curl, steel, 0.0105f);
            Thumb(hs, sd, curl, steel, 0.0115f);
            hs.Flush(1 + i, R.hand[i]);
        }
        if (R.dumbbells) Dumbbell(R.hand[i]);
    }

    // ---- head ----
    const Matrix H = M(M(MatrixScale(1.14f, 1.14f, 1.14f), T(0, -0.016f, 0)), R.head);
    E(H, 0, 0.113f, 0, 0.117f, 0.107f, 0.108f, shell);
    E(H, 0, 0.19f, -0.008f, 0.092f, 0.04f, 0.088f, shell);                      // domed crown
    gfx::TubeSet rt;
    {   // panel seam around the crown
        std::vector<Vector3> seam;
        for (int k = 0; k < 40; k++) { float a = 2.f * PI * k / 40; seam.push_back(Vector3{ cosf(a) * 0.091f, 0.172f, -0.008f + sinf(a) * 0.087f }); }
        rt.Add(seam, std::vector<float>(seam.size(), 0.0034f), steel, true);
    }
    // visor
    E(H, 0, 0.116f, 0.074f, 0.093f, 0.07f, 0.04f, RGBA(20, 30, 52));
    {   // bezel: one smooth steel rim around the visor
        std::vector<Vector3> bez;
        for (int k = 0; k < 48; k++) { float a = 2.f * PI * k / 48; bez.push_back(Vector3{ cosf(a) * 0.097f, 0.116f + sinf(a) * 0.074f, 0.074f + 0.016f }); }
        rt.Add(bez, std::vector<float>(bez.size(), 0.0058f), steel, true);
    }
    float open = R.blink > 0 ? Clamp(fabsf(R.blink - 0.08f) / 0.08f, 0.08f, 1.f) : 1.f;
    for (int s = -1; s <= 1; s += 2) {
        float ex = s * 0.037f, ey = 0.124f;
        E(H, ex, ey, VisorZ(ex, ey) + 0.001f, 0.0215f, 0.0275f * open, 0.0065f, glow, 0.f, 1.f);       // glowing eye
        E(H, ex, ey, VisorZ(ex, ey) + 0.005f, 0.0125f, 0.017f * open, 0.004f, RGBA(220, 252, 255), 0.f, 1.f);
        S(H, ex - s * 0.007f, ey + 0.009f * open, VisorZ(ex, ey) + 0.009f, 0.0036f, RGBA(255, 255, 255), 1.f);
        for (int k = 0; k < 5; k++) {                                                                   // eyebrow LEDs, tilting with expression
            float u = k / 4.f, x = s * (0.018f + 0.036f * u);
            float y = 0.165f + R.brow * 0.012f + 0.004f * sinf(PI * u) + (1.f - u) * 0.006f;
            E(H, x, y, VisorZ(x, y) + 0.001f, 0.0062f, 0.0032f, 0.003f, glow, 0.f, 0.9f);
        }
        S(H, s * 0.066f, 0.082f, 0.1f, 0.0062f, amber, 0.8f);                                          // cheek lights
        for (int k = 0; k < 3; k++) E(H, s * 0.111f, 0.086f - 0.014f * k, 0.01f, 0.0062f, 0.0042f, 0.03f, dark);   // grille
        // ear speakers
        Cy(H, s * 0.1f, 0.113f, 0.0f, 0.04f, 0.03f, steel, 0.f, -s * PI * 0.5f);
        Cy(H, s * 0.128f, 0.113f, 0.0f, 0.03f, 0.008f, dark, 0.f, -s * PI * 0.5f);
        {
            std::vector<Vector3> rim;
            for (int k = 0; k < 28; k++) { float a = 2.f * PI * k / 28; rim.push_back(Vector3{ s * 0.106f, 0.113f + cosf(a) * 0.04f, sinf(a) * 0.04f }); }
            rt.Add(rim, std::vector<float>(rim.size(), 0.0042f), teal, true);
        }
    }
    // LED mouth: an equaliser while talking, a smile when idle
    for (int k = -5; k <= 5; k++) {
        float u = k / 5.f;
        float wave = 0.5f + 0.5f * sinf(R.clock * 22.f + k * 1.7f);
        float h = 0.0036f + R.mouth * 0.017f * wave;
        float y = 0.078f + 0.011f * u * u * (1.f - R.mouth) - h * 0.5f * (1.f - R.mouth);
        E(H, u * 0.034f, y, VisorZ(u * 0.034f, y) + 0.001f, 0.0032f, h, 0.003f, glow, 0.f, 1.f);
    }
    S(H, 0, 0.102f, VisorZ(0, 0.102f) + 0.003f, 0.0058f, steel);   // nose-bolt
    // antenna
    TubeC(H, Vector3{ 0.0f, 0.222f, -0.01f }, Vector3{ 0.012f, 0.288f, -0.012f }, [](float) { return 0.0045f; }, steel);
    for (int k = 0; k < 3; k++) Ring(H, 0.226f + 0.01f * k, 0.011f - 0.002f * k, 10, 0.0031f, dark, 1.f, 0.f, -0.01f);
    S(H, 0.012f, 0.298f, -0.012f, 0.0135f, amber, 0.35f + 0.65f * pulse);
    rt.Flush(0, H);
}
} // namespace

void Librarian::Draw() const {
    Rig R;
    R.pelvis = mPelvis; R.spine = mSpine; R.neck = mNeck; R.head = mHead;
    for (int i = 0; i < 2; i++) {
        R.shoulder[i] = mShoulder[i]; R.elbow[i] = mElbow[i]; R.hand[i] = mHand[i];
        R.hip[i] = mHip[i]; R.knee[i] = mKnee[i]; R.foot[i] = mFoot[i];
    }
    R.muscle = Clamp(muscle, 0.f, 1.f);
    R.sculpt = Clamp(muscleTarget, 0.f, 1.f);
    R.mouth = mouth; R.brow = brow; R.blink = blink; R.clock = clock; R.dumbbells = dumbbells; R.fists = fists;
    R.legs = !seated;
    if (look == "byte") { DrawRobot(R); return; }
    Spec spec = MakeSpec(look);
    DrawHumanBody(R, spec, look);
    DrawHumanHead(R, spec, look);
}

void Librarian::FreeMeshes() { look::UnloadSculpts(); }
