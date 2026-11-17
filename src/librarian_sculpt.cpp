// librarian_sculpt.cpp - the librarians' palettes and their heads, sculpted from melting shapes (see sdf.h).
// Each head (skin, ears, nose, hair, moustache, beard) is one continuous fine mesh built the first time it is
// needed. Eyes, brows, mouth and glasses move, so librarian_draw.cpp lays them on top every frame.
#include "librarian_look.h"
#include "sdf.h"
#include <algorithm>
#include <map>

using util::RGBA;
using util::Shade;

namespace look {

Spec MakeSpec(const std::string& id) {
    Spec s;
    // Mr. Malloc: tweed jacket, golden waistcoat, bow tie, white tufts and a moustache
    s.skin = RGBA(238, 190, 154); s.skinShade = RGBA(214, 162, 126); s.hair = RGBA(240, 237, 230); s.brow = RGBA(232, 228, 220);
    s.jacket = RGBA(128, 86, 58); s.jacketDark = RGBA(98, 64, 44); s.vest = RGBA(228, 176, 88); s.shirt = RGBA(240, 236, 224);
    s.accent = RGBA(214, 46, 56); s.trousers = RGBA(92, 80, 88); s.shoes = RGBA(62, 42, 34); s.glasses = RGBA(70, 46, 32);
    s.iris = RGBA(70, 104, 128); s.lip = RGBA(190, 110, 100);
    s.moustache = true; s.beard = false; s.hat = false; s.bowTie = true; s.chain = true; s.hood = false;
    s.lashes = false; s.labCoat = false; s.glassesStyle = 1; s.belly = 1.f; s.browSlope = 1.f; s.browBulk = 1.25f;
    if (id == "pointer") {          // Professor Pointer: wizard robes, tall hat, flowing white beard
        s.skin = RGBA(232, 186, 154); s.skinShade = RGBA(208, 158, 124); s.hair = RGBA(244, 242, 246); s.brow = RGBA(238, 236, 240);
        s.jacket = RGBA(106, 66, 160); s.jacketDark = RGBA(76, 46, 120); s.vest = RGBA(56, 38, 98); s.shirt = RGBA(242, 238, 226);
        s.accent = RGBA(244, 204, 92); s.trousers = RGBA(62, 46, 102); s.shoes = RGBA(54, 38, 72); s.glasses = RGBA(214, 172, 78);
        s.iris = RGBA(64, 110, 180); s.lip = RGBA(190, 112, 104);
        s.beard = true; s.hat = true; s.bowTie = false; s.chain = false; s.glassesStyle = 2; s.belly = 0.55f; s.browBulk = 1.7f; s.browSlope = 0.3f;
    } else if (id == "heap") {      // Dr. Heap: lab coat, a cloud of dark curls, delighted by everything
        s.skin = RGBA(176, 120, 86); s.skinShade = RGBA(152, 98, 68); s.hair = RGBA(52, 36, 32); s.brow = RGBA(44, 30, 26);
        s.jacket = RGBA(236, 240, 244); s.jacketDark = RGBA(190, 198, 208); s.vest = RGBA(48, 160, 162); s.shirt = RGBA(246, 246, 242);
        s.accent = RGBA(60, 140, 224); s.trousers = RGBA(70, 76, 98); s.shoes = RGBA(52, 52, 62); s.glasses = RGBA(40, 44, 56);
        s.iris = RGBA(92, 58, 38); s.lip = RGBA(150, 82, 74);
        s.moustache = false; s.bowTie = false; s.chain = false; s.labCoat = true; s.glassesStyle = 3; s.belly = 0.15f; s.browSlope = -0.4f; s.browBulk = 1.15f;
    } else if (id == "ada") {       // Ada: zip hoodie, auburn hair in a bun, big grin
        s.skin = RGBA(242, 200, 168); s.skinShade = RGBA(218, 170, 138); s.hair = RGBA(186, 88, 52); s.brow = RGBA(140, 66, 42);
        s.jacket = RGBA(46, 152, 154); s.jacketDark = RGBA(32, 114, 120); s.vest = RGBA(248, 238, 216); s.shirt = RGBA(248, 238, 216);
        s.accent = RGBA(255, 192, 72); s.trousers = RGBA(62, 68, 98); s.shoes = RGBA(244, 244, 240); s.glasses = RGBA(86, 54, 96);
        s.iris = RGBA(52, 132, 92); s.lip = RGBA(214, 104, 108);
        s.moustache = false; s.bowTie = false; s.chain = false; s.hood = true; s.lashes = true; s.glassesStyle = 4; s.belly = 0.f; s.browSlope = 0.6f; s.browBulk = 0.85f;
    }
    return s;
}

namespace {

enum { gSkin = 0, gHair = 1, gFacial = 2, gTrim = 3 };
const float DEG = PI / 180.f;
const Color kCheek = RGBA(244, 146, 136);
constexpr int kDN = 64;   // depth maps are kDN x kDN

Vector3 V(float x, float y, float z) { return Vector3{ x, y, z }; }
float Lf(float a, float b, float t) { return a + (b - a) * t; }
float Hash(int i) { float s = sinf((float)i * 12.9898f + 3.1f) * 43758.5453f; return s - floorf(s); }

// first point of a group's surface met going from a to b
Vector3 Trace(const sdf::Model& m, Vector3 a, Vector3 b, int group) {
    Vector3 d = Vector3Subtract(b, a);
    float len = Vector3Length(d), t = 0.f;
    d = Vector3Scale(d, 1.f / len);
    for (int i = 0; i < 48 && t < len; i++) {
        float dist = m.Dist(Vector3Add(a, Vector3Scale(d, t)), group);
        if (dist < 0.0003f) break;
        t += dist;
    }
    return Vector3Add(a, Vector3Scale(d, std::min(t, len)));
}

void SculptHead(const std::string& id, const Spec& L, sdf::Model& m, Face& f) {
    const bool mal = id == "malloc", ptr = id == "pointer", heap = id == "heap";
    const Color skin = L.skin;

    // ---- skull, jaw, chin, cheeks, neck: broad shapes that melt into one head ----
    const Vector3 C{ 0.f, 0.128f, -0.004f };   // centre of the cranium
    const Vector3 cranR = mal ? V(0.106f, 0.112f, 0.114f) : ptr ? V(0.102f, 0.113f, 0.112f) : heap ? V(0.103f, 0.113f, 0.113f) : V(0.103f, 0.111f, 0.112f);
    m.Ell(C, cranR, skin, gSkin, 0.03f);
    m.Ell(V(0, 0.079f, 0.012f), mal ? V(0.099f, 0.08f, 0.094f) : ptr ? V(0.087f, 0.083f, 0.09f) : heap ? V(0.091f, 0.08f, 0.09f) : V(0.088f, 0.077f, 0.088f), skin, gSkin, 0.035f);
    if (mal) m.Ball(V(0, 0.03f, 0.05f), 0.036f, skin, gSkin, 0.03f);
    else if (ptr) m.Ball(V(0, 0.02f, 0.054f), 0.03f, skin, gSkin, 0.03f);
    else if (heap) m.Ball(V(0, 0.022f, 0.056f), 0.031f, skin, gSkin, 0.03f);
    else m.Ball(V(0, 0.026f, 0.056f), 0.026f, skin, gSkin, 0.03f);
    for (int s = -1; s <= 1; s += 2) {
        if (mal) m.Ball(V(s * 0.055f, 0.078f, 0.064f), 0.032f, skin, gSkin, 0.03f);
        else if (ptr) m.Ball(V(s * 0.05f, 0.086f, 0.062f), 0.026f, skin, gSkin, 0.03f);
        else if (heap) m.Ball(V(s * 0.052f, 0.084f, 0.062f), 0.029f, skin, gSkin, 0.03f);
        else m.Ball(V(s * 0.05f, 0.082f, 0.062f), 0.03f, skin, gSkin, 0.03f);
    }
    m.Cone(V(0, 0.05f, -0.018f), V(0, 0.018f, -0.018f), 0.044f, 0.04f, skin, gSkin, 0.03f);   // the top of the neck (the rest belongs to the torso)

    // a point on the scalp in a direction (azimuth from the nose, elevation) seen from the head's centre
    auto scalp = [&](float az, float el) {
        Vector3 d{ sinf(az) * cosf(el), sinf(el), cosf(az) * cosf(el) };
        float t = 1.f / sqrtf(d.x * d.x / (cranR.x * cranR.x) + d.y * d.y / (cranR.y * cranR.y) + d.z * d.z / (cranR.z * cranR.z));
        return Vector3Add(C, Vector3Scale(d, t));
    };
    auto out = [&](Vector3 p) { return Vector3Normalize(Vector3Subtract(p, C)); };
    auto lift = [&](Vector3 p, float by) { return Vector3Add(p, Vector3Scale(out(p), by)); };
    auto surf = [&](float x, float y) { return Trace(m, V(x, y, 0.25f), V(x, y, -0.05f), gSkin).z; };

    // ---- brow ridge, eye sockets ----
    f.eyeX = 0.041f; f.eyeY = 0.131f;
    f.eyeR = mal ? 0.0215f : ptr ? 0.0205f : heap ? 0.022f : 0.0232f;
    f.browY = f.eyeY + (ptr ? 0.041f : 0.047f);   // the professor's half-moons leave room for lower brows
    {
        float by = f.eyeY + 0.034f;
        m.Ell(V(0, by, surf(0, by) - 0.0105f), V(0.06f, 0.013f, 0.015f), skin, gSkin, 0.03f);
    }
    f.eyeZ = surf(f.eyeX, f.eyeY) - f.eyeR + 0.0015f;
    for (int s = -1; s <= 1; s += 2) {
        float r = f.eyeR + 0.0012f;
        m.Carve(V(s * f.eyeX, f.eyeY, f.eyeZ), V(r, r, r), Shade(skin, 0.84f), gSkin, 0.005f);
    }

    // ---- nose ----
    {
        float ny = mal ? 0.101f : ptr ? 0.096f : heap ? 0.1f : 0.103f;          // height of the tip
        float tipR = mal ? 0.0195f : ptr ? 0.0205f : heap ? 0.0155f : 0.0115f;
        float fwd = mal ? 0.012f : ptr ? 0.017f : heap ? 0.009f : 0.008f;       // how far it stands out
        Color nc = (mal || ptr) ? RGBA(240, 158, 136) : Shade(skin, 1.02f);
        float nz = surf(0, ny) + fwd - tipR * 0.25f;
        float byy = f.eyeY + 0.004f;
        m.Cone(V(0, byy, surf(0, byy) - 0.004f), V(0, ny + 0.004f, nz - 0.004f), 0.0085f, tipR * 0.72f, skin, gSkin, 0.01f);
        m.Ball(V(0, ny, nz), tipR, nc, gSkin, 0.008f);
        for (int s = -1; s <= 1; s += 2) {
            m.Ball(V(s * tipR * 0.74f, ny - 0.004f, nz - tipR * 0.55f), tipR * (heap ? 0.62f : 0.52f), nc, gSkin, 0.006f);
            m.Paint(V(s * tipR * 0.42f, ny - tipR * 0.82f, nz - tipR * 0.1f), V(0.004f, 0.003f, 0.004f), Shade(skin, 0.5f), 0.002f, 0.85f, gSkin);
        }
    }

    // ---- ears ----
    for (int s = -1; s <= 1; s += 2) {
        m.Ell(V(s * 0.106f, 0.113f, -0.008f), V(0.012f, 0.033f, 0.021f), skin, gSkin, 0.008f, V(0, -s * 0.35f, 0));
        m.Carve(V(s * 0.117f, 0.115f, -0.002f), V(0.007f, 0.02f, 0.011f), Shade(L.skinShade, 0.9f), gSkin, 0.004f, V(0, -s * 0.35f, 0));
    }
    f.earX = 0.11f;

    // ---- mouth area and cheeks ----
    f.mouthY = ptr ? 0.048f : 0.05f;
    f.mouthW = mal ? 0.03f : ptr ? 0.027f : heap ? 0.034f : 0.032f;
    f.smile = 0.011f;
    f.restOpen = mal ? 0.38f : 0.f;
    f.lidOpen = ptr ? 29.f : 40.f;   // the professor's lids rest a little lower: wise, not startled
    m.Ell(V(0, f.mouthY + 0.006f, surf(0, f.mouthY + 0.006f) - 0.015f), V(0.036f, 0.024f, 0.02f), skin, gSkin, 0.02f);   // a soft muzzle
    for (int s = -1; s <= 1; s += 2)
        m.Paint(V(s * 0.062f, 0.084f, surf(s * 0.062f, 0.084f)), V(0.019f, 0.013f, 0.02f), kCheek, 0.012f, mal ? 0.75f : 0.55f, gSkin);

    // ---- hair ----
    if (mal) {
        // cotton-wool clouds round the sides and back; the crown stays bald
        for (int row = 0; row < 4; row++)
            for (int col = 0; col < 12; col++) {
                int i = row * 12 + col;
                float az0 = 104.f - 10.f * row;
                float az = Lf(az0, 360.f - az0, (col + 0.5f + 0.5f * (Hash(i) - 0.5f)) / 12.f) * DEG;
                float el = Lf(-52.f, 20.f, (row + 0.5f + 0.4f * (Hash(i + 40) - 0.5f)) / 4.f) * DEG;
                float r = 0.022f + 0.008f * fabsf(sinf(az)) + 0.004f * Hash(i + 80);
                Vector3 p = scalp(az, el);
                m.Ball(lift(p, -r * 0.3f), r, Shade(L.hair, 0.94f + 0.06f * Hash(i + 120)), gHair, 0.02f);
            }
        for (int s = -1; s <= 1; s += 2) {   // the big three-lobed puffs over the ears
            m.Ball(V(s * 0.121f, 0.168f, -0.006f), 0.034f, L.hair, gHair, 0.012f);
            m.Ball(V(s * 0.132f, 0.129f, -0.02f), 0.036f, L.hair, gHair, 0.012f);
            m.Ball(V(s * 0.121f, 0.09f, -0.03f), 0.031f, Shade(L.hair, 0.97f), gHair, 0.012f);
        }
        // moustache: two full lobes that droop and taper
        for (int s = -1; s <= 1; s += 2) {
            auto on = [&](float x, float y, float dz) { return V(s * x, y, surf(s * x, y) + dz); };
            m.Lock({ on(0.003f, 0.079f, 0.006f), on(0.02f, 0.077f, 0.009f), on(0.04f, 0.069f, 0.007f), on(0.056f, 0.062f, 0.003f), on(0.066f, 0.063f, 0.0f) },
                   { 0.0115f, 0.0138f, 0.0118f, 0.0075f, 0.003f }, L.hair, gFacial, 0.005f);
            m.Lock({ on(0.005f, 0.07f, 0.006f), on(0.022f, 0.066f, 0.008f), on(0.04f, 0.058f, 0.006f), on(0.053f, 0.053f, 0.002f) },
                   { 0.009f, 0.0108f, 0.0085f, 0.003f }, Shade(L.hair, 0.97f), gFacial, 0.005f);
        }
    } else if (ptr) {
        // long white hair: a smooth cap under the hat and a curtain to the shoulders, combed into broad locks.
        // The locks are wide and close together, so they read as gentle waves in one mass, with a scalloped hem.
        m.Ell(Vector3Add(C, V(0, 0.004f, -0.012f)), Vector3Add(cranR, V(0.012f, 0.012f, 0.014f)), L.hair, gHair, 0.02f);
        m.Ell(V(0, 0.05f, -0.052f), V(0.106f, 0.105f, 0.064f), L.hair, gHair, 0.03f);
        const int NL = 17;
        for (int i = 0; i < NL; i++) {
            float u = i / (NL - 1.f), fr = fabsf(u - 0.5f) * 2.f;                      // fr: 0 at the back, 1 beside the face
            float az = (Lf(80.f, 280.f, u) + 5.f * (Hash(i + 21) - 0.5f)) * DEG;
            float w = 0.86f + 0.3f * Hash(i + 11), wave = 0.011f * sinf(i * 2.4f);
            float fwd = Clamp((fr - 0.7f) / 0.3f, 0.f, 1.f);                           // the locks beside the face fall in front of the shoulders
            float hem = -0.062f - 0.03f * Hash(i + 3) - 0.022f * std::max(0.f, -cosf(az)) - 0.035f * fwd;
            Vector3 side{ cosf(az), 0.f, -sinf(az) };
            Vector3 p0 = lift(scalp(az, 36.f * DEG), 0.004f), p1 = lift(scalp(az, 6.f * DEG), 0.01f), p2 = lift(scalp(az, -22.f * DEG), 0.012f);
            Vector3 p3 = Vector3Add(V(p2.x * 1.02f, 0.012f, p2.z * 1.02f + 0.022f * fwd), Vector3Scale(side, wave));
            Vector3 p4 = Vector3Add(V(p2.x * (1.f - 0.1f * fwd), hem, p2.z + 0.045f * fwd), Vector3Scale(side, -wave));
            m.Lock({ p0, p1, p2, p3, p4 }, { 0.016f * w, 0.022f * w, 0.026f * w, 0.025f * w, 0.012f * w }, Shade(L.hair, 0.94f + 0.06f * Hash(i + 7)), gHair, 0.005f);
        }
        m.Carve(V(0, 0.06f, 0.13f), V(0.09f, 0.2f, 0.125f), Shade(L.hair, 0.85f), gHair, 0.008f);   // the face stays clear
        // beard: a soft mass that hugs the jaw from ear to ear and flows to a point on the chest in broad waves
        {
            std::vector<Vector3> jaw;
            std::vector<float> jr;
            for (int k = -6; k <= 6; k++) {
                float a = k / 6.f, x = a * 0.09f, y = 0.008f + 0.086f * a * a;
                jaw.push_back(V(x, y, surf(x, y + 0.01f) - 0.004f - 0.006f * a * a));
                jr.push_back(0.022f - 0.013f * a * a);   // it thins to a sideburn under the hair
            }
            m.Lock(jaw, jr, L.hair, gFacial, 0.014f);
        }
        m.Ell(V(0, -0.045f, 0.07f), V(0.05f, 0.08f, 0.028f), L.hair, gFacial, 0.02f);
        for (int c = -2; c <= 2; c++) {
            float a = c / 2.f, sgn = (c & 1) ? 1.f : -1.f;
            float x0 = a * 0.058f, y0 = 0.004f + 0.034f * a * a;
            float yEnd = c == 0 ? -0.2f : ((c == 1 || c == -1) ? -0.148f : -0.088f);   // the middle wave is the longest
            Vector3 p0{ x0, y0, surf(x0, y0 + 0.012f) + 0.004f };
            Vector3 tip{ a * 0.045f, yEnd, 0.078f - 0.006f * fabsf(a) };
            Vector3 m1 = Vector3Add(Vector3Lerp(p0, tip, 0.33f), V(sgn * 0.007f, 0, 0.014f));
            Vector3 m2 = Vector3Add(Vector3Lerp(p0, tip, 0.66f), V(-sgn * 0.007f, 0, 0.012f));
            m.Lock({ p0, m1, m2, tip }, { 0.022f, 0.03f, 0.027f, 0.013f }, Shade(L.hair, 0.94f + 0.06f * Hash(c + 30)), gFacial, 0.01f);
        }
        for (int s = -1; s <= 1; s += 2) {   // moustache: two full sweeps that melt into the beard past the corners of the mouth
            auto on = [&](float x, float y, float dz) { return V(s * x, y, surf(s * x, y) + dz); };
            m.Lock({ on(0.003f, 0.079f, 0.004f), on(0.02f, 0.074f, 0.007f), on(0.04f, 0.06f, 0.008f), on(0.054f, 0.04f, 0.01f), on(0.06f, 0.022f, 0.012f) },
                   { 0.008f, 0.015f, 0.016f, 0.012f, 0.007f }, Shade(L.hair, 1.03f), gFacial, 0.007f);
        }
    } else if (heap) {
        // a rounded cloud of tight curls
        const int N = 150;
        for (int i = 0; i < N; i++) {
            float t = (i + 0.5f) / N, phi = acosf(1.f - 2.f * t), th = PI * (1.f + sqrtf(5.f)) * i;
            Vector3 d{ sinf(phi) * cosf(th), cosf(phi), sinf(phi) * sinf(th) };
            bool face = d.z > 0.25f && d.y < 0.58f - 0.25f * fabsf(d.x);      // keep the face clear
            if (face || d.y < -0.42f) continue;
            m.Ball(V(d.x * 0.128f, 0.15f + d.y * 0.118f, -0.016f + d.z * 0.122f), 0.024f + 0.007f * Hash(i),
                   Shade(L.hair, 0.9f + 0.55f * Hash(i + 50)), gHair, 0.009f);
        }
        m.Ell(V(0, 0.155f, -0.03f), V(0.118f, 0.105f, 0.105f), L.hair, gHair, 0.01f);   // so no scalp shows between curls
    } else {
        // Ada: hair combed back smooth into a bun on the crown, a side-swept fringe and a loose strand by each ear
        const Vector3 bun{ 0.f, 0.262f, -0.06f };
        const Vector3 capC = Vector3Add(C, V(0, 0.008f, -0.014f)), capR = Vector3Add(cranR, V(0.011f, 0.012f, 0.012f));
        m.Ell(capC, capR, L.hair, gHair, 0.02f);   // thin at the hairline, full at the crown
        for (int i = 0; i < 22; i++) {   // combed back: broad, shallow ridges that sweep round and up into the bun
            float az0 = 2.f * PI * (i + 0.5f * Hash(i + 90)) / 22.f;
            std::vector<Vector3> p;
            for (int k = 0; k < 7; k++) {
                float t = k / 6.f, el = Lf(-42.f, 74.f, t) * DEG, az = az0 + 0.4f * (1.f - t);
                Vector3 d{ sinf(az) * cosf(el), sinf(el), cosf(az) * cosf(el) };
                d = Vector3{ d.x, d.y * cosf(0.33f) + d.z * sinf(0.33f), d.z * cosf(0.33f) - d.y * sinf(0.33f) };   // lean the pole back, towards the bun
                p.push_back(Vector3Add(capC, V(d.x * capR.x * 0.85f, d.y * capR.y * 0.85f, d.z * capR.z * 0.85f)));
            }
            m.Lock(p, std::vector<float>(p.size(), 0.021f), Shade(L.hair, 0.95f + 0.11f * Hash(i + 3)), gHair, 0.008f);
        }
        m.Carve(V(0, 0.06f, 0.175f), V(0.211f, 0.14f, 0.2f), Shade(L.hair, 0.82f), gHair, 0.006f);    // the face: forehead, temples, cheeks
        m.Carve(V(0, -0.02f, -0.05f), V(0.2f, 0.066f, 0.2f), Shade(L.hair, 0.82f), gHair, 0.006f);    // a level hairline at the nape
        for (int s = -1; s <= 1; s += 2)                                                               // and round each ear
            m.Carve(V(s * 0.114f, 0.106f, -0.002f), V(0.03f, 0.044f, 0.032f), Shade(L.hair, 0.82f), gHair, 0.006f);
        for (int i = 0; i < 5; i++) {   // fringe swept across the forehead to her left
            Vector3 a = lift(scalp((-20.f + 4.f * i) * DEG, 62.f * DEG), 0.0f), b = lift(scalp((5.f + 9.f * i) * DEG, (40.f - 2.f * i) * DEG), 0.009f);
            Vector3 c = lift(scalp((26.f + 10.f * i) * DEG, (30.f - 4.f * i) * DEG), 0.011f), d = lift(scalp((42.f + 9.f * i) * DEG, (21.f - 4.f * i) * DEG), 0.004f);
            m.Lock({ a, b, c, d }, { 0.011f, 0.0175f, 0.0145f, 0.005f }, Shade(L.hair, 0.95f + 0.16f * Hash(i + 60)), gHair, 0.006f);
        }
        for (int i = 0; i < 2; i++) {   // and a short sweep to her right of the parting
            Vector3 a = scalp((-28.f - 4.f * i) * DEG, 60.f * DEG), b = lift(scalp((-48.f - 6.f * i) * DEG, (40.f - 4.f * i) * DEG), 0.008f);
            Vector3 c = lift(scalp((-66.f - 6.f * i) * DEG, (24.f - 5.f * i) * DEG), 0.002f);
            m.Lock({ a, b, c }, 0.011f, 0.016f, 0.006f, Shade(L.hair, 0.98f + 0.1f * Hash(i + 70)), gHair, 0.006f);
        }
        for (int s = -1; s <= 1; s += 2)   // a loose curl in front of each ear
            m.Lock({ lift(scalp(s * 70.f * DEG, 18.f * DEG), 0.002f), V(s * 0.103f, 0.118f, 0.036f), V(s * 0.107f, 0.09f, 0.037f), V(s * 0.104f, 0.066f, 0.042f), V(s * 0.108f, 0.05f, 0.046f) },
                   { 0.008f, 0.0105f, 0.0095f, 0.007f, 0.0035f }, Shade(L.hair, 1.05f), gHair, 0.004f);
        m.Ball(bun, 0.04f, L.hair, gHair, 0.01f);
        for (int k = 0; k < 3; k++) {   // hair wound round the bun
            std::vector<Vector3> sp;
            for (int q = 0; q < 9; q++) {
                float a = 2.f * PI * q / 8.f * 0.92f + k * 2.1f;
                sp.push_back(V(bun.x + cosf(a) * 0.035f, bun.y + 0.012f * sinf(a * 1.5f) + 0.008f * (k - 1), bun.z + sinf(a) * 0.035f));
            }
            m.Lock(sp, 0.006f, 0.011f, 0.006f, Shade(L.hair, 0.86f + 0.14f * k), gHair, 0.005f);
        }
        std::vector<Vector3> ring;      // scrunchie
        for (int q = 0; q <= 10; q++) { float a = 2.f * PI * q / 10.f; ring.push_back(V(bun.x + cosf(a) * 0.031f, bun.y - 0.03f, bun.z + sinf(a) * 0.031f + 0.006f)); }
        m.Lock(ring, std::vector<float>(ring.size(), 0.0085f), L.accent, gTrim, 0.003f);
    }
}

// ---------------------------------------------------------------------------
// the torso: jacket, waistcoat, lapels, collar and shoulders sculpted in spine space.
// `m` is fitness 0..1: it broadens the chest and shoulders and slims the paunch.
// ---------------------------------------------------------------------------
void SculptTorso(const std::string& id, const Spec& L, float m, sdf::Model& t) {
    enum { gCoat = 0, gInner = 1, gDark = 2, gTrim = 3, gNeck = 4 };
    const bool robe = id == "pointer", coat = L.labCoat, hoodie = L.hood;
    const float b = L.belly * (1.f - m), sx = ShoulderX(id);
    const Color gold = RGBA(244, 204, 104);
    const float chestW = sx - 0.02f + 0.052f * m, chestD = 0.098f + 0.026f * m;
    const float yoke = 0.048f + 0.016f * m, pec = Clamp((m - 0.25f) / 0.5f, 0.f, 1.f);
    auto body = [&](int g, Color c, float grow) {   // a long core, hips, belly, chest and chest muscles: the hem hangs below the hip joints
        t.Ell(V(0, 0.2f, 0.f), V(0.126f + grow, 0.25f, 0.095f + grow), c, g, 0.04f);
        t.Ell(V(0, 0.05f, 0.f), V(0.146f + grow, 0.3f, 0.106f + grow), c, g, 0.04f);
        t.Ell(V(0, 0.17f, 0.01f + 0.026f * b), V(0.132f + 0.02f * b + grow, 0.125f, 0.102f + 0.036f * b + grow), c, g, 0.04f);
        t.Ell(V(0, 0.365f, 0.004f), V(chestW + grow, 0.125f, chestD + grow), c, g, 0.04f);
        for (int s = -1; s <= 1; s += 2)
            t.Ell(V(s * chestW * 0.48f, 0.4f, 0.045f + 0.03f * pec), V(chestW * 0.46f + grow, 0.064f, 0.012f + 0.032f * pec + grow), c, g, 0.035f);
    };
    body(gCoat, L.jacket, 0.f);
    t.Cone(V(0, 0.47f, -0.01f), V(0, 0.572f, -0.012f), 0.072f, 0.05f + 0.012f * m, L.jacket, gCoat, 0.03f);        // up to the collar
    t.Cone(V(0, 0.5f, -0.014f), V(0, 0.56f, -0.016f), 0.055f, 0.054f, L.skin, gNeck, 0.01f);                       // the neck stands in the collar; the head turns on top of it
    for (int s = -1; s <= 1; s += 2) {
        t.Cone(V(0, 0.532f, -0.008f), V(s * (sx - 0.012f), 0.488f, -0.004f), yoke + 0.004f, yoke - 0.006f, L.jacket, gCoat, 0.04f);   // shoulders slope down from the neck
        // shoulder caps the arms turn in; at top fitness the sleeves are gone and the shoulders are bare
        if (m >= 0.8f) t.Ball(V(s * sx, 0.497f, 0.f), 0.044f + 0.04f * m, L.skin, gNeck, 0.01f);
        else t.Ball(V(s * sx, 0.497f, 0.f), 0.044f + 0.04f * m, L.jacket, gCoat, 0.03f);
    }
    auto front = [&](float x, float y) { return Trace(t, V(x, y, 0.4f), V(x, y, -0.1f), -1).z; };
    auto hoop = [&](float y, float r, Color c, int g) {   // a band round the body at height y
        std::vector<Vector3> ring;
        for (int k = 0; k <= 16; k++) {
            float a = 2.f * PI * k / 16.f;
            Vector3 d{ sinf(a), 0.f, cosf(a) };
            Vector3 p = Trace(t, Vector3Add(V(0, y, 0.f), Vector3Scale(d, 0.4f)), V(0, y, 0.f), gCoat);
            ring.push_back(Vector3Add(p, Vector3Scale(d, r * 0.35f)));
        }
        t.Lock(ring, std::vector<float>(ring.size(), r), c, g, 0.003f);
    };

    if (hoodie) {
        // hood rolled behind the neck, a ribbed collar and a kangaroo pocket
        std::vector<Vector3> hood, rib;
        for (int k = 0; k <= 8; k++) {
            float a = (-20.f + 220.f * k / 8.f) * DEG;
            hood.push_back(V(cosf(a) * (sx - 0.055f), 0.535f + 0.01f * sinf(k * 1.4f), -0.03f - sinf(a) * 0.072f));
        }
        t.Lock(hood, 0.016f, 0.032f, 0.016f, L.jacket, gCoat, 0.014f);
        for (int k = 0; k <= 12; k++) {
            float a = 2.f * PI * k / 12.f;
            rib.push_back(V(cosf(a) * 0.057f, 0.563f - 0.012f * sinf(a), -0.012f + sinf(a) * 0.057f));
        }
        t.Lock(rib, std::vector<float>(rib.size(), 0.012f), Shade(L.jacket, 0.86f), gDark, 0.004f);
        t.Ell(V(0, 0.115f, front(0, 0.115f) - 0.013f), V(0.092f, 0.048f, 0.02f), Shade(L.jacket, 0.9f), gDark, 0.004f);
        hoop(-0.052f, 0.014f, Shade(L.jacket, 0.86f), gDark);   // ribbed hem
    } else if (robe) {
        // a long tunic with a rope belt and a clasp at the throat (the beard covers the front)
        t.Ell(V(0, -0.05f, 0.f), V(0.156f, 0.09f, 0.116f), L.jacket, gCoat, 0.04f);
        hoop(0.14f, 0.009f, L.accent, gTrim);
        t.Lock({ V(0.045f, 0.14f, front(0.045f, 0.14f)), V(0.05f, 0.1f, front(0.05f, 0.1f) + 0.006f), V(0.046f, 0.06f, front(0.046f, 0.06f) + 0.006f) }, 0.008f, 0.008f, 0.011f, L.accent, gTrim, 0.003f);
        t.Ball(V(0, 0.525f, front(0, 0.525f) + 0.002f), 0.017f, gold, gTrim, 0.003f);
    } else {
        // a jacket (or lab coat) hanging open over a waistcoat (or shirt), with lapels, tie and buttons
        body(gInner, L.vest, -0.005f);
        t.Cone(V(0, 0.44f, -0.01f), V(0, 0.57f, -0.012f), 0.066f, 0.048f, L.vest, gInner, 0.03f);
        // the opening is narrow under the collar and widens towards the hem
        const float w0 = (coat ? 0.04f : 0.034f) + 0.01f * m, w1 = (coat ? 0.088f : 0.072f) + 0.02f * b;
        auto edge = [&](float y) { return Lf(w0, w1, Clamp((0.5f - y) / 0.6f, 0.f, 1.f)); };
        {
            const float ys[5] = { 0.495f, 0.36f, 0.22f, 0.08f, -0.1f };
            Vector3 p[5];
            for (int i = 0; i < 5; i++) p[i] = V(0, ys[i], front(0, ys[i]));
            for (int i = 0; i < 4; i++) {
                t.Cone(p[i], p[i + 1], edge(ys[i]), edge(ys[i + 1]), Shade(L.jacket, 0.78f), gCoat, 0.004f);
                t.prims.back().mode = sdf::Prim::Carve;
            }
        }
        t.Paint(V(0, -0.085f, 0.08f), V(0.2f, 0.115f, 0.2f), L.trousers, 0.004f, 1.f, gInner);        // trousers below the waistcoat
        t.Paint(V(0, 0.5f, front(0, 0.5f)), V(0.03f, 0.058f, 0.03f), L.shirt, 0.003f, 1.f, gInner);    // shirt in the V of the waistcoat
        for (int s = -1; s <= 1; s += 2) {   // lapels: a wide, flat roll round the back of the neck and down each edge of the opening, tapering to a point
            auto on = [&](float y, float r) { float x = s * (edge(y) + 0.55f * r); return V(x, y, front(x, y) - 0.62f * r); };
            t.Lock({ V(0, 0.566f, -0.062f), V(s * 0.046f, 0.563f, -0.042f), V(s * 0.06f, 0.55f, 0.002f), on(0.5f, 0.028f), on(0.42f, 0.026f), on(0.33f, 0.016f), on(0.26f, 0.005f) },
                   { 0.013f, 0.014f, 0.018f, 0.028f, 0.026f, 0.016f, 0.005f }, L.jacketDark, gDark, 0.004f);
        }
        for (int k = 0; k < 4; k++) {
            float y = 0.16f + 0.078f * k;
            t.Ball(V(0, y, front(0, y) + 0.001f), 0.0085f, coat ? RGBA(236, 240, 244) : gold, gTrim, 0.002f);
        }
        if (L.bowTie) {
            // the bow sits on whatever is beneath it: at high fitness the chest pushes out under the lobes
            float zl = std::max(front(-0.03f, 0.532f), front(0.03f, 0.532f)) + 0.006f, zt = std::max(front(0, 0.532f) + 0.008f, zl);
            for (int s = -1; s <= 1; s += 2)
                for (int v = -1; v <= 1; v += 2)   // each wing is two lobes fanning out from the knot
                    t.Ell(V(s * 0.026f, 0.532f + v * 0.007f, zl), V(0.022f, 0.0095f, 0.008f), L.accent, gTrim, 0.004f, V(0, 0, s * v * 0.4f));
            t.Ball(V(0, 0.532f, zt + 0.003f), 0.0105f, Shade(L.accent, 0.8f), gTrim, 0.002f);
        }
        if (coat) t.Ell(V(-0.092f, 0.33f, front(-0.092f, 0.33f) - 0.004f), V(0.034f, 0.028f, 0.007f), Shade(L.jacket, 0.92f), gDark, 0.003f);   // breast pocket
    }
    // a level hem: slice the bottom of the garment off flat
    t.Carve(V(0, -0.27f, 0.f), V(0.4f, 0.2f, 0.4f), Shade(L.jacket, 0.7f), gCoat, 0.004f);
    t.Carve(V(0, -0.27f, 0.f), V(0.4f, 0.2f, 0.4f), Shade(L.jacket, 0.7f), gInner, 0.004f);
    t.Carve(V(0, 0.25f, -0.42f), V(0.6f, 0.7f, 0.4f), L.jacket, gInner, 0.004f);   // nothing is worn under the back: it could only show through
}

// ---------------------------------------------------------------------------
// a cartoon hand in hand space: wrist at the origin, fingers along -Y, palm towards +Z.
// `side` (-1 / +1) is the side the thumb is on; `grip` closes the fist round a dumbbell bar.
// ---------------------------------------------------------------------------
void SculptHand(const Spec& L, float side, bool grip, sdf::Model& h) {
    const Color skin = L.skin;
    h.Cone(V(0, 0.012f, 0.f), V(0, -0.02f, 0.001f), 0.021f, 0.024f, skin, 0, 0.01f);   // wrist
    h.Ell(V(0, -0.042f, 0.002f), V(0.031f, 0.034f, 0.0155f), skin, 0, 0.012f);         // palm
    for (int k = 0; k < 4; k++) {
        float x = (k - 1.5f) * 0.0158f, len = (k == 0 || k == 3) ? 0.88f : 1.f;
        if (grip)
            h.Lock({ V(x, -0.064f, 0.004f), V(x, -0.09f, 0.012f), V(x, -0.1f, 0.036f), V(x, -0.084f, 0.056f), V(x, -0.066f, 0.05f) },
                   { 0.0092f, 0.009f, 0.0088f, 0.0082f, 0.0074f }, skin, 0, 0.0035f);
        else
            h.Lock({ V(x, -0.066f, 0.f), V(x * 1.06f, -0.066f - 0.024f * len, 0.004f), V(x * 1.08f, -0.066f - 0.042f * len, 0.012f), V(x * 1.06f, -0.066f - 0.054f * len, 0.024f) },
                   { 0.0092f, 0.009f, 0.0084f, 0.0072f }, skin, 0, 0.0035f);
    }
    if (grip) h.Lock({ V(side * 0.026f, -0.034f, 0.01f), V(side * 0.03f, -0.056f, 0.038f), V(side * 0.012f, -0.068f, 0.06f) }, 0.0098f, 0.0094f, 0.008f, skin, 0, 0.004f);
    else h.Lock({ V(side * 0.024f, -0.028f, 0.004f), V(side * 0.04f, -0.046f, 0.012f), V(side * 0.044f, -0.066f, 0.022f) }, 0.0098f, 0.0094f, 0.0078f, skin, 0, 0.004f);
}

// polygonise a model and map its front surface over [x0,x1] x [y0,y1]
void Bake(Sculpt& s, const sdf::Model& m, float cell, float x0, float x1, float y0, float y1, float zFront, float zBack, int group) {
    s.meshes = m.Build(cell);
    s.x0 = x0; s.x1 = x1; s.y0 = y0; s.y1 = y1;
    s.depth.resize((size_t)kDN * kDN);
    for (int j = 0; j < kDN; j++)
        for (int i = 0; i < kDN; i++) {
            float x = Lf(x0, x1, i / (kDN - 1.f)), y = Lf(y0, y1, j / (kDN - 1.f));
            s.depth[(size_t)j * kDN + i] = Trace(m, V(x, y, zFront), V(x, y, zBack), group).z;
        }
}

struct TorsoSlot { Sculpt s; int level = -1; };
std::map<std::string, Head> g_heads;
std::map<std::string, TorsoSlot> g_torsos;
std::map<std::string, Sculpt> g_hands;

} // namespace

float Sculpt::Z(float x, float y) const {
    float u = Clamp((x - x0) / (x1 - x0) * (kDN - 1), 0.f, kDN - 1.001f), v = Clamp((y - y0) / (y1 - y0) * (kDN - 1), 0.f, kDN - 1.001f);
    int i = (int)u, j = (int)v;
    float fu = u - i, fv = v - j;
    auto D = [&](int a, int b) { return depth[(size_t)b * kDN + a]; };
    return (D(i, j) * (1 - fu) + D(i + 1, j) * fu) * (1 - fv) + (D(i, j + 1) * (1 - fu) + D(i + 1, j + 1) * fu) * fv;
}

const Head& GetHead(const std::string& id) {
    auto it = g_heads.find(id);
    if (it != g_heads.end()) return it->second;
    Head& h = g_heads[id];
    sdf::Model m;
    SculptHead(id, MakeSpec(id), m, h.face);
    Bake(h, m, 0.002f, -0.13f, 0.13f, -0.03f, 0.26f, 0.22f, -0.06f, gSkin);
    return h;
}

const Sculpt& GetTorso(const std::string& id, float muscle) {
    int level = (int)floorf(Clamp(muscle, 0.f, 1.f) * 20.f + 0.5f);   // rebuilt in steps of 5% fitness
    TorsoSlot& t = g_torsos[id];
    if (t.level == level) return t.s;
    for (Mesh& mesh : t.s.meshes) UnloadMesh(mesh);
    t.s = Sculpt();
    sdf::Model m;
    SculptTorso(id, MakeSpec(id), level / 20.f, m);
    Bake(t.s, m, 0.003f, -0.32f, 0.32f, -0.14f, 0.62f, 0.4f, -0.2f, -1);
    t.level = level;
    return t.s;
}

const Sculpt& GetHand(const std::string& id, int arm, bool grip) {
    std::string key = id + (arm ? "L" : "R") + (grip ? "g" : "r");
    auto it = g_hands.find(key);
    if (it != g_hands.end()) return it->second;
    Sculpt& s = g_hands[key];
    sdf::Model m;
    SculptHand(MakeSpec(id), arm == 0 ? -1.f : 1.f, grip, m);
    s.meshes = m.Build(0.0015f);
    return s;
}

void UnloadSculpts() {
    for (auto& kv : g_heads)
        for (Mesh& mesh : kv.second.meshes) UnloadMesh(mesh);
    for (auto& kv : g_torsos)
        for (Mesh& mesh : kv.second.s.meshes) UnloadMesh(mesh);
    g_heads.clear();
    g_torsos.clear();
    for (auto& kv : g_hands)
        for (Mesh& mesh : kv.second.meshes) UnloadMesh(mesh);
    g_hands.clear();
}

} // namespace look
