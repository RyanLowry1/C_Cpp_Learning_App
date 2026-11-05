// art.cpp - 2D drawing helpers
#include "art.h"
#include "rlgl.h"
#include <algorithm>

namespace art {

static Texture2D g_glow{};

void Init() {
    const int N = 128;
    Color* px = (Color*)MemAlloc((unsigned int)(N * N * sizeof(Color)));
    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            float dx = (x + 0.5f) / N * 2.f - 1.f, dy = (y + 0.5f) / N * 2.f - 1.f;
            float d = sqrtf(dx * dx + dy * dy);
            float a = d >= 1.f ? 0.f : util::Smooth(1.f - d);
            a = powf(a, 1.25f);
            px[y * N + x] = Color{ 255, 255, 255, (unsigned char)(a * 255.f) };
        }
    }
    Image img{ px, N, N, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    g_glow = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(g_glow, TEXTURE_FILTER_BILINEAR);
}

void Shutdown() { if (g_glow.id) UnloadTexture(g_glow); g_glow = Texture2D{}; }

void Glow(Vector2 c, float rx, float ry, Color col) {
    DrawTexturePro(g_glow, Rectangle{ 0, 0, (float)g_glow.width, (float)g_glow.height }, Rectangle{ c.x - rx, c.y - ry, rx * 2, ry * 2 }, Vector2{ 0, 0 }, 0, col);
}

void Shadow(Vector2 c, float rx, float ry, float alpha) { Glow(c, rx, ry, Color{ 0, 0, 0, (unsigned char)(util::Clamp01(alpha) * 255.f) }); }

void Ell(Vector2 c, float rx, float ry, Color col, float rot) {
    if (rx <= 0.f || ry <= 0.f) return;
    if (rot == 0.f) { DrawEllipseV(c, rx, ry, col); return; }
    rlPushMatrix();
    rlTranslatef(c.x, c.y, 0);
    rlRotatef(rot, 0, 0, 1);
    DrawEllipseV(Vector2{ 0, 0 }, rx, ry, col);
    rlPopMatrix();
}

void EllShaded(Vector2 c, float rx, float ry, Color col) {
    Ell(c, rx, ry, util::Shade(col, 0.78f));
    Ell(Vector2{ c.x - rx * 0.05f, c.y - ry * 0.07f }, rx * 0.92f, ry * 0.9f, col);
    Ell(Vector2{ c.x - rx * 0.24f, c.y - ry * 0.3f }, rx * 0.46f, ry * 0.36f, util::WithAlpha(util::Shade(col, 1.25f), 0.55f));
}

void Poly(const Vector2* p, int n, Color col) {
    for (int i = 1; i + 1 < n; i++) {
        Vector2 a = p[0], b = p[i], c = p[i + 1];
        float cr = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (cr > 0) std::swap(b, c);   // raylib wants counter-clockwise triangles
        DrawTriangle(a, b, c, col);
    }
}

void Cap(Vector2 a, Vector2 b, float thick, Color col) {
    DrawLineEx(a, b, thick, col);
    DrawCircleV(a, thick * 0.5f, col);
    DrawCircleV(b, thick * 0.5f, col);
}

void RRect(Rectangle r, float radius, Color col) {
    float m = std::min(r.width, r.height);
    DrawRectangleRounded(r, util::Clamp01(2.f * radius / std::max(1.f, m)), 10, col);
}

void RRectLines(Rectangle r, float radius, float thick, Color col) {
    float m = std::min(r.width, r.height);
    DrawRectangleRoundedLinesEx(r, util::Clamp01(2.f * radius / std::max(1.f, m)), 10, thick, col);
}

void Quad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color col) {
    Vector2 p[4] = { a, b, c, d };
    Poly(p, 4, col);
}

Vector2 ClampReach(Vector2 s, Vector2 h, float l1, float l2) {
    Vector2 d{ h.x - s.x, h.y - s.y };
    float dist = sqrtf(d.x * d.x + d.y * d.y);
    float maxD = l1 + l2 - 1.f;
    if (dist > maxD) { float k = maxD / dist; return Vector2{ s.x + d.x * k, s.y + d.y * k }; }
    return h;
}

Vector2 Elbow(Vector2 s, Vector2 h, float l1, float l2, float side) {
    Vector2 d{ h.x - s.x, h.y - s.y };
    float dist = std::max(1.f, sqrtf(d.x * d.x + d.y * d.y));
    dist = std::min(dist, l1 + l2 - 0.5f);
    Vector2 dir{ d.x / std::max(1.f, sqrtf(d.x * d.x + d.y * d.y)), d.y / std::max(1.f, sqrtf(d.x * d.x + d.y * d.y)) };
    float a = (l1 * l1 - l2 * l2 + dist * dist) / (2.f * dist);
    float hh = sqrtf(std::max(0.f, l1 * l1 - a * a));
    Vector2 perp{ -dir.y, dir.x };
    Vector2 base{ s.x + dir.x * a, s.y + dir.y * a };
    Vector2 p1{ base.x + perp.x * hh, base.y + perp.y * hh }, p2{ base.x - perp.x * hh, base.y - perp.y * hh };
    auto score = [&](Vector2 p) { return (p.x - s.x) * side + (p.y - s.y) * 0.45f; };
    return score(p1) >= score(p2) ? p1 : p2;
}

} // namespace art
