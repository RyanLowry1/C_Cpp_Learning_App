// scene.cpp - procedural library interior
#include "scene.h"
#include "book3d.h"
#include "content.h"
#include "ui.h"
#include "rlgl.h"
#include <ctime>

using namespace world;
using util::RGBA;

static const Color kWood = RGBA(138, 92, 56);
static const Color kWoodDark = RGBA(92, 60, 38);
static const Color kWoodLight = RGBA(172, 120, 76);
static const Color kBrass = RGBA(226, 184, 98);
static const Color kLeather = RGBA(30, 78, 56);
static const Color kPlaster = RGBA(214, 188, 148);

static const Color kBookPalette[] = {
    RGBA(112, 32, 30), RGBA(70, 22, 26), RGBA(32, 52, 92), RGBA(24, 40, 62), RGBA(32, 72, 42), RGBA(22, 52, 34),
    RGBA(132, 92, 40), RGBA(92, 60, 30), RGBA(72, 40, 82), RGBA(44, 40, 38), RGBA(146, 116, 74), RGBA(104, 42, 22),
    RGBA(50, 82, 82), RGBA(150, 132, 100), RGBA(120, 60, 50), RGBA(60, 66, 30), RGBA(30, 30, 50), RGBA(160, 70, 40),
};

// ---------------------------------------------------------------------------
static Texture2D TextureFromRT(RenderTexture2D rt) {
    Image img = LoadImageFromTexture(rt.texture);
    ImageFlipVertical(&img);
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    return t;
}

static Texture2D MakeRugTexture() {
    const int S = 512, H = 416;
    Image img = GenImageColor(S, H, RGBA(110, 26, 30));
    Color border = RGBA(60, 16, 22), gold = RGBA(196, 150, 80), cream = RGBA(214, 190, 150), navy = RGBA(30, 36, 64);
    ImageDrawRectangle(&img, 0, 0, S, H, border);
    ImageDrawRectangle(&img, 22, 22, S - 44, H - 44, RGBA(118, 30, 34));
    ImageDrawRectangleLines(&img, Rectangle{ 14, 14, S - 28.f, H - 28.f }, 3, gold);
    ImageDrawRectangleLines(&img, Rectangle{ 30, 30, S - 60.f, H - 60.f }, 2, cream);
    // border motif
    for (int x = 40; x < S - 40; x += 24) {
        ImageDrawCircle(&img, x, 22, 4, gold);
        ImageDrawCircle(&img, x, H - 22, 4, gold);
    }
    for (int y = 40; y < H - 40; y += 24) {
        ImageDrawCircle(&img, 22, y, 4, gold);
        ImageDrawCircle(&img, S - 22, y, 4, gold);
    }
    // central medallion
    int cx = S / 2, cy = H / 2;
    for (int r = 120; r > 0; r -= 20) ImageDrawCircle(&img, cx, cy, r, (r / 20) % 2 ? navy : RGBA(150, 40, 40));
    ImageDrawCircle(&img, cx, cy, 18, gold);
    for (int i = 0; i < 16; i++) {
        float a = i * PI / 8.f;
        ImageDrawLineEx(&img, Vector2{ (float)cx, (float)cy }, Vector2{ cx + cosf(a) * 120.f, cy + sinf(a) * 120.f }, 2, util::WithAlpha(gold, 0.6f));
    }
    // corner pieces
    const int cxs[4] = { 70, S - 70, 70, S - 70 }, cys[4] = { 70, 70, H - 70, H - 70 };
    for (int i = 0; i < 4; i++) { ImageDrawCircle(&img, cxs[i], cys[i], 26, navy); ImageDrawCircle(&img, cxs[i], cys[i], 12, gold); }
    // a little wear noise
    util::Rng rng(99);
    for (int i = 0; i < 4000; i++) ImageDrawPixel(&img, rng.I(0, S - 1), rng.I(0, H - 1), util::WithAlpha(rng.F() < 0.5f ? BLACK : cream, 0.18f));
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    return t;
}

void Scene::AddLabel(const std::string& text, Vector3 center, Vector2 size, float yaw, int texW, int texH, float fontSize, bool brass) {
    RenderTexture2D rt = LoadRenderTexture(texW, texH);
    BeginTextureMode(rt);
    ClearBackground(BLANK);
    ui::Begin(Vector2{ 0, 0 }, 1.f, false);
    if (brass) {
        DrawRectangleGradientV(0, 0, texW, texH, RGBA(222, 186, 110), RGBA(160, 118, 56));
        DrawRectangleLinesEx(Rectangle{ 3, 3, texW - 6.f, texH - 6.f }, 3, RGBA(110, 76, 30));
        DrawCircle(10, texH / 2, 3, RGBA(110, 76, 30));
        DrawCircle(texW - 10, texH / 2, 3, RGBA(110, 76, 30));
    } else {
        DrawRectangleGradientV(0, 0, texW, texH, RGBA(54, 34, 20), RGBA(36, 22, 14));
        DrawRectangleLinesEx(Rectangle{ 4, 4, texW - 8.f, texH - 8.f }, 3, RGBA(200, 160, 90));
    }
    std::vector<std::string> lines = util::Split(text, '\n');
    float lh = fontSize * 1.15f;
    float y = (texH - lh * lines.size()) * 0.5f + fontSize * 0.04f;
    Color ink = brass ? RGBA(48, 30, 14) : RGBA(230, 196, 120);
    for (size_t i = 0; i < lines.size(); i++) {
        float fs = i == 0 ? fontSize : fontSize * 0.72f;
        FontId f = i == 0 ? FontId::SerifBold : FontId::SerifItalic;
        float w = ui::MeasureWidth(f, lines[i], fs, 1.5f);
        if (w > texW - 24) { fs *= (texW - 24) / w; w = ui::MeasureWidth(f, lines[i], fs, 1.5f); }
        ui::Text(f, lines[i], (texW - w) * 0.5f, y + (lh - fs * 1.15f) * 0.5f, fs, ink, 1.5f);
        y += lh;
    }
    ui::End();
    EndTextureMode();
    Label L;
    L.tex = TextureFromRT(rt);
    UnloadRenderTexture(rt);
    L.m = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(size.x, 1, size.y), MatrixRotateX(PI * 0.5f)), MatrixRotateY(yaw)),
                         MatrixTranslate(center.x, center.y, center.z));
    labels.push_back(L);
}

// ---------------------------------------------------------------------------
// Book filling. Works in a local frame: x along the shelf, front of the shelf at
// z = 0 with books going back to -depth, spines facing +z. `frame` maps to world.
// ---------------------------------------------------------------------------
static void BookBox(gfx::Builder& b, const Matrix& frame, Vector3 c, Vector3 s, Color col, float yaw = 0.f) {
    b.Box(MatrixMultiply(gfx::BoxM(c, s, yaw), frame), col);
}

static void FillRow(gfx::Builder& b, const Matrix& frame, float x0, float x1, float y, float clearance, util::Rng& rng,
                    const std::vector<std::pair<float, float>>& reserved) {
    float x = x0;
    const int npal = (int)(sizeof(kBookPalette) / sizeof(kBookPalette[0]));
    while (x < x1 - 0.02f) {
        bool skipped = false;
        for (auto& r : reserved) if (x + 0.07f > r.first && x < r.second) { x = r.second + 0.004f; skipped = true; }
        if (skipped) continue;
        float roll = rng.F();
        if (roll < 0.05f) { x += rng.R(0.04f, 0.12f); continue; }   // gap
        if (roll < 0.11f && x1 - x > 0.3f) {                            // lying stack
            float w = rng.R(0.16f, 0.24f);
            bool clash = false;
            for (auto& r : reserved) if (x + w > r.first && x < r.second) clash = true;
            if (!clash) {
                float yy = y;
                int n = rng.I(2, 5);
                for (int i = 0; i < n && yy < y + clearance - 0.06f; i++) {
                    float t = rng.R(0.028f, 0.05f);
                    float dz = rng.R(0.19f, 0.27f);
                    Color c = util::Shade(kBookPalette[rng.I(0, npal - 1)], rng.R(0.95f, 1.3f));
                    BookBox(b, frame, Vector3{ x + w * 0.5f + rng.R(-0.01f, 0.01f), yy + t * 0.5f, -dz * 0.5f - rng.R(0.01f, 0.04f) }, Vector3{ w * rng.R(0.85f, 1.f), t, dz }, c, rng.R(-0.08f, 0.08f));
                    yy += t;
                }
                x += w + 0.01f;
                continue;
            }
        }
        float w = rng.R(0.022f, 0.062f);
        if (x + w > x1) break;
        float h = std::min(clearance - 0.02f, clearance * rng.R(0.6f, 0.97f));
        float d = rng.R(0.17f, 0.3f);
        float inset = rng.R(0.006f, 0.04f);
        Color c = util::Shade(kBookPalette[rng.I(0, npal - 1)], rng.R(0.98f, 1.4f));
        BookBox(b, frame, Vector3{ x + w * 0.5f, y + h * 0.5f, -inset - d * 0.5f }, Vector3{ w, h, d }, c);
        float fz = -inset + 0.0015f;
        if (rng.F() < 0.4f) {
            Color g = rng.F() < 0.7f ? RGBA(196, 156, 82) : util::Shade(c, 1.6f);
            BookBox(b, frame, Vector3{ x + w * 0.5f, y + h * 0.12f, fz }, Vector3{ w * 0.96f, 0.007f, 0.003f }, g);
            BookBox(b, frame, Vector3{ x + w * 0.5f, y + h * 0.86f, fz }, Vector3{ w * 0.96f, 0.007f, 0.003f }, g);
        }
        if (rng.F() < 0.3f) BookBox(b, frame, Vector3{ x + w * 0.5f, y + h * 0.6f, fz }, Vector3{ w * 0.7f, h * 0.16f, 0.003f }, util::Shade(c, 1.45f));
        x += w + rng.R(0.0f, 0.004f);
    }
}

void Scene::BuildRoom(gfx::Builder& b) {
    util::Rng rng(4242);
    // floor planks
    for (float x = -5.8f; x < 5.8f; x += 0.2f) {
        float z = -3.7f;
        while (z < 3.0f) {
            float len = std::min(rng.R(1.1f, 2.6f), 3.0f - z);
            Color c = util::Shade(RGBA(150, 102, 66), rng.R(0.82f, 1.1f));
            b.Box(Vector3{ x + 0.1f, -0.01f, z + len * 0.5f }, Vector3{ 0.196f, 0.02f, len - 0.004f }, c);
            z += len;
        }
    }
    // walls
    b.Box(Vector3{ 0, 2.15f, -3.74f }, Vector3{ 11.8f, 4.3f, 0.06f }, kPlaster);
    b.Box(Vector3{ 0, 0.55f, -3.70f }, Vector3{ 11.8f, 1.1f, 0.03f }, kWoodDark);
    b.Box(Vector3{ 0, 2.15f, 3.05f }, Vector3{ 11.8f, 4.3f, 0.06f }, kPlaster);
    for (int s = -1; s <= 1; s += 2) {
        float x = s * 5.86f;
        // wall with a window opening on the left wall (z -2.0 .. -0.6, y 1.1 .. 3.3)
        if (s < 0) {
            b.Box(Vector3{ x, 2.15f, (-3.7f - 2.0f) * 0.5f }, Vector3{ 0.06f, 4.3f, 1.7f }, kPlaster);
            b.Box(Vector3{ x, 2.15f, (-0.6f + 3.0f) * 0.5f }, Vector3{ 0.06f, 4.3f, 3.6f }, kPlaster);
            b.Box(Vector3{ x, 0.55f, -1.3f }, Vector3{ 0.06f, 1.1f, 1.4f }, kPlaster);
            b.Box(Vector3{ x, 3.8f, -1.3f }, Vector3{ 0.06f, 1.0f, 1.4f }, kPlaster);
            // window frame + sill
            b.Box(Vector3{ x + 0.05f, 1.06f, -1.3f }, Vector3{ 0.2f, 0.08f, 1.6f }, kWoodLight);
            b.Box(Vector3{ x + 0.03f, 3.34f, -1.3f }, Vector3{ 0.12f, 0.1f, 1.56f }, kWood);
            b.Box(Vector3{ x + 0.03f, 2.2f, -2.03f }, Vector3{ 0.12f, 2.36f, 0.08f }, kWood);
            b.Box(Vector3{ x + 0.03f, 2.2f, -0.57f }, Vector3{ 0.12f, 2.36f, 0.08f }, kWood);
            b.Box(Vector3{ x + 0.03f, 2.2f, -1.3f }, Vector3{ 0.08f, 2.3f, 0.04f }, kWood);
            for (int i = 1; i < 4; i++) b.Box(Vector3{ x + 0.03f, 1.1f + i * 0.55f, -1.3f }, Vector3{ 0.08f, 0.035f, 1.4f }, kWood);
        } else {
            b.Box(Vector3{ x, 2.15f, -0.35f }, Vector3{ 0.06f, 4.3f, 6.8f }, kPlaster);
        }
        b.Box(Vector3{ x - s * 0.02f, 0.55f, -0.35f }, Vector3{ 0.03f, 1.1f, 6.8f }, kWoodDark);
    }
    // skirting + picture rail
    b.Box(Vector3{ 0, 4.12f, -3.69f }, Vector3{ 11.8f, 0.08f, 0.05f }, kWood);
    // ceiling + beams
    b.Box(Vector3{ 0, 4.33f, -0.35f }, Vector3{ 11.8f, 0.04f, 6.9f }, RGBA(196, 166, 128));
    for (float z = -3.2f; z < 3.0f; z += 1.55f) b.Box(Vector3{ 0, 4.2f, z }, Vector3{ 11.8f, 0.2f, 0.24f }, RGBA(132, 90, 56));
    b.Box(Vector3{ 0, 4.18f, -0.35f }, Vector3{ 0.26f, 0.16f, 6.9f }, RGBA(126, 86, 54));
}

void Scene::BuildShelves(gfx::Builder& b) {
    const float L = -3.0f * BayWidth, R = 3.0f * BayWidth;
    const float depth = ShelfFrontZ - ShelfBackZ;
    const float zc = (ShelfFrontZ + ShelfBackZ) * 0.5f;
    const float top = BoardTop[7];
    b.Box(Vector3{ 0, top * 0.5f, ShelfBackZ + 0.015f }, Vector3{ R - L + 0.1f, top, 0.03f }, RGBA(104, 70, 48));
    b.Box(Vector3{ 0, 0.05f, zc }, Vector3{ R - L + 0.12f, 0.1f, depth + 0.04f }, kWoodDark);   // plinth
    for (int k = 0; k <= Bays; k++) {
        float x = L + k * BayWidth;
        float w = (k == 0 || k == Bays) ? 0.07f : 0.05f;
        b.Box(Vector3{ x, top * 0.5f, zc }, Vector3{ w, top, depth + (k == 0 || k == Bays ? 0.04f : 0.0f) }, kWood);
    }
    for (int r = 0; r < 7; r++) {
        float y = BoardTop[r];
        b.Box(Vector3{ 0, y - 0.015f, zc }, Vector3{ R - L, 0.03f, depth }, kWood);
        b.Box(Vector3{ 0, y - 0.02f, ShelfFrontZ - 0.005f }, Vector3{ R - L, 0.04f, 0.012f }, kWoodLight);   // lip
    }
    // cornice
    b.Box(Vector3{ 0, top + 0.12f, zc + 0.02f }, Vector3{ R - L + 0.2f, 0.24f, depth + 0.08f }, kWood);
    b.Box(Vector3{ 0, top + 0.27f, zc + 0.04f }, Vector3{ R - L + 0.32f, 0.06f, depth + 0.14f }, kWoodDark);
    b.Box(Vector3{ 0, top + 0.015f, ShelfFrontZ + 0.03f }, Vector3{ R - L + 0.16f, 0.03f, 0.03f }, kBrass);

    // books
    const Content& C = Data();
    util::Rng rng(2024);
    Matrix frame = MatrixTranslate(0, 0, ShelfFrontZ);
    for (int bay = 0; bay < Bays; bay++) {
        float bx0 = L + bay * BayWidth + 0.035f, bx1 = L + (bay + 1) * BayWidth - 0.035f;
        for (int r = 0; r < 7; r++) {
            float y = BoardTop[r];
            float clearance = BoardTop[r + 1] - 0.03f - y;
            std::vector<std::pair<float, float>> reserved;
            for (const ShelfBook& sb : shelf) {
                Vector3 o = gfx::Origin(sb.inShelf);
                if (fabsf(o.y - (y + book::D * 0.5f)) < 0.05f && o.x > bx0 - 0.2f && o.x < bx1 + 0.2f)
                    reserved.push_back({ o.x - 0.015f, o.x + book::PageY * 2.f + 0.015f });
            }
            FillRow(b, frame, bx0, bx1, y, clearance, rng, reserved);
        }
    }
    (void)C;
}

void Scene::BuildSideShelves(gfx::Builder& b, float x, float dir) {
    // dir = +1: shelf on the left wall facing +x ; -1: right wall facing -x
    float zStart = dir > 0 ? 1.6f : -2.9f;
    float len = dir > 0 ? 1.9f : 4.4f;
    Matrix frame = MatrixMultiply(MatrixRotateY(dir > 0 ? PI * 0.5f : -PI * 0.5f), MatrixTranslate(x, 0, zStart));
    auto box = [&](Vector3 c, Vector3 s, Color col) { b.Box(MatrixMultiply(gfx::BoxM(c, s), frame), col); };
    const float depth = 0.36f, top = BoardTop[7];
    box(Vector3{ len * 0.5f, top * 0.5f, -depth + 0.015f }, Vector3{ len, top, 0.03f }, RGBA(104, 70, 48));
    box(Vector3{ len * 0.5f, 0.05f, -depth * 0.5f }, Vector3{ len + 0.1f, 0.1f, depth + 0.04f }, kWoodDark);
    int bays = (int)(len / 1.1f + 0.5f);
    float bw = len / bays;
    for (int k = 0; k <= bays; k++) box(Vector3{ k * bw, top * 0.5f, -depth * 0.5f }, Vector3{ 0.05f, top, depth }, kWood);
    for (int r = 0; r < 7; r++) box(Vector3{ len * 0.5f, BoardTop[r] - 0.015f, -depth * 0.5f }, Vector3{ len, 0.03f, depth }, kWood);
    box(Vector3{ len * 0.5f, top + 0.12f, -depth * 0.5f + 0.02f }, Vector3{ len + 0.15f, 0.24f, depth + 0.08f }, kWood);
    util::Rng rng(dir > 0 ? 77u : 91u);
    for (int k = 0; k < bays; k++)
        for (int r = 0; r < 7; r++)
            FillRow(b, frame, k * bw + 0.03f, (k + 1) * bw - 0.03f, BoardTop[r], BoardTop[r + 1] - 0.03f - BoardTop[r], rng, {});
}

void Scene::BuildDesk(gfx::Builder& b) {
    const float cz = (DeskBackZ + DeskFrontZ) * 0.5f;
    const float dz = DeskFrontZ - DeskBackZ;
    b.Box(Vector3{ 0, 0.755f, cz }, Vector3{ 2.56f, 0.06f, dz + 0.06f }, kWood);
    b.Box(Vector3{ 0, 0.718f, cz }, Vector3{ 2.6f, 0.02f, dz + 0.1f }, kWoodDark);
    b.Box(Vector3{ 0, DeskTopY - 0.0015f, cz }, Vector3{ 2.2f, 0.003f, 0.86f }, kLeather);
    // gold tooling around the leather
    b.Box(Vector3{ 0, DeskTopY, cz - 0.415f }, Vector3{ 2.16f, 0.0012f, 0.006f }, kBrass);
    b.Box(Vector3{ 0, DeskTopY, cz + 0.415f }, Vector3{ 2.16f, 0.0012f, 0.006f }, kBrass);
    b.Box(Vector3{ -1.075f, DeskTopY, cz }, Vector3{ 0.006f, 0.0012f, 0.83f }, kBrass);
    b.Box(Vector3{ 1.075f, DeskTopY, cz }, Vector3{ 0.006f, 0.0012f, 0.83f }, kBrass);
    // pedestals
    for (int s = -1; s <= 1; s += 2) {
        float x = s * 0.92f;
        b.Box(Vector3{ x, 0.37f, cz }, Vector3{ 0.62f, 0.7f, dz - 0.04f }, util::Shade(kWood, 0.92f));
        b.Box(Vector3{ x, 0.025f, cz }, Vector3{ 0.66f, 0.05f, dz }, kWoodDark);
        for (int d = 0; d < 3; d++) {
            if (s > 0 && d == 0) continue;   // ledger drawer is animated separately
            float y = 0.6f - d * 0.21f;
            b.Box(Vector3{ x, y, DeskBackZ + 0.008f }, Vector3{ 0.54f, 0.18f, 0.02f }, kWoodLight);
            b.Box(Vector3{ x, y, DeskBackZ - 0.008f }, Vector3{ 0.07f, 0.018f, 0.02f }, kBrass);
        }
    }
    // visitor-side panel with raised fields
    b.Box(Vector3{ 0, 0.4f, DeskFrontZ - 0.015f }, Vector3{ 2.48f, 0.64f, 0.035f }, util::Shade(kWood, 0.9f));
    for (int i = -1; i <= 1; i++) {
        b.Box(Vector3{ i * 0.8f, 0.42f, DeskFrontZ + 0.005f }, Vector3{ 0.66f, 0.46f, 0.012f }, kWoodLight);
        b.Box(Vector3{ i * 0.8f, 0.42f, DeskFrontZ + 0.012f }, Vector3{ 0.56f, 0.36f, 0.006f }, util::Shade(kWood, 1.05f));
    }
}

void Scene::BuildFurniture(gfx::Builder& b) {
    // reading table at the back left
    Vector3 t{ -3.0f, 0, -1.7f };
    b.Box(Vector3{ t.x, 0.76f, t.z }, Vector3{ 1.6f, 0.05f, 0.8f }, kWood);
    for (int i = -1; i <= 1; i += 2) for (int j = -1; j <= 1; j += 2)
        b.Box(Vector3{ t.x + i * 0.72f, 0.37f, t.z + j * 0.32f }, Vector3{ 0.06f, 0.74f, 0.06f }, kWoodDark);
    for (int i = 0; i < 3; i++)
        b.Box(Vector3{ t.x + 0.3f, 0.795f + i * 0.035f, t.z - 0.1f }, Vector3{ 0.26f, 0.034f, 0.34f }, kBookPalette[(i * 5) % 12], 0.1f * i);
    b.Box(Vector3{ t.x - 0.25f, 0.787f, t.z + 0.05f }, Vector3{ 0.5f, 0.004f, 0.34f }, RGBA(225, 214, 190), 0.2f);
    // chairs at the reading table
    for (int s = -1; s <= 1; s += 2) {
        Vector3 c{ t.x + s * 0.45f, 0, t.z + 0.62f };
        b.Box(Vector3{ c.x, 0.45f, c.z }, Vector3{ 0.42f, 0.05f, 0.42f }, kWood);
        b.Box(Vector3{ c.x, 0.75f, c.z + 0.19f }, Vector3{ 0.42f, 0.55f, 0.04f }, kWood);
        for (int i = -1; i <= 1; i += 2) for (int j = -1; j <= 1; j += 2)
            b.Box(Vector3{ c.x + i * 0.18f, 0.22f, c.z + j * 0.18f }, Vector3{ 0.04f, 0.44f, 0.04f }, kWoodDark);
    }
    // rolling ladder leaning on the back shelves
    {
        float lx = BayCenter(4) + 0.45f;
        Vector3 bot{ lx, 0.0f, -2.55f }, top{ lx, 3.1f, ShelfFrontZ + 0.05f };
        Vector3 d = Vector3Subtract(top, bot);
        float len = Vector3Length(d);
        Vector3 Y = Vector3Scale(d, 1.f / len), X{ 1, 0, 0 }, Z = Vector3CrossProduct(X, Y);
        for (int s = -1; s <= 1; s += 2) {
            Vector3 c = Vector3Add(Vector3Scale(Vector3Add(bot, top), 0.5f), Vector3{ s * 0.22f, 0, 0 });
            b.Box(gfx::Basis(Vector3Scale(X, 0.045f), Vector3Scale(Y, len), Vector3Scale(Z, 0.06f), c), kWoodLight);
        }
        for (int i = 1; i < 11; i++) {
            Vector3 c = Vector3Add(bot, Vector3Scale(d, i / 11.f));
            b.Box(gfx::Basis(Vector3Scale(X, 0.44f), Vector3Scale(Y, 0.03f), Vector3Scale(Z, 0.035f), c), kWoodLight);
        }
        b.Box(Vector3{ 0, 3.12f, ShelfFrontZ + 0.07f }, Vector3{ 9.0f, 0.02f, 0.02f }, kBrass);   // rail
    }
    // floor stacks of books
    util::Rng rng(31337);
    const Vector3 stacks[] = { { -4.0f, 0, -2.75f }, { 1.55f, 0, -2.85f }, { 4.7f, 0, 1.6f }, { -2.2f, 0, -2.9f } };
    for (Vector3 s : stacks) {
        float y = 0;
        int n = rng.I(4, 8);
        for (int i = 0; i < n; i++) {
            float th = rng.R(0.035f, 0.06f);
            b.Box(Vector3{ s.x + rng.R(-0.02f, 0.02f), y + th * 0.5f, s.z + rng.R(-0.02f, 0.02f) }, Vector3{ rng.R(0.22f, 0.3f), th, rng.R(0.28f, 0.36f) },
                  util::Shade(kBookPalette[rng.I(0, 17)], rng.R(0.8f, 1.1f)), rng.R(-0.4f, 0.4f));
            y += th;
        }
    }
    // globe stand
    Vector3 g{ 3.6f, 0, -1.2f };
    for (int i = 0; i < 3; i++) {
        float a = i * 2.f * PI / 3.f;
        b.Box(Vector3{ g.x + cosf(a) * 0.14f, 0.3f, g.z + sinf(a) * 0.14f }, Vector3{ 0.035f, 0.6f, 0.035f }, kWoodDark, -a);
    }
    b.Cylinder(MatrixMultiply(MatrixScale(0.2f, 0.03f, 0.2f), MatrixTranslate(g.x, 0.6f, g.z)), kWood);
    b.Cylinder(MatrixMultiply(MatrixScale(0.02f, 0.3f, 0.02f), MatrixTranslate(g.x, 0.6f, g.z)), kBrass, 8);
    // plant pot
    b.Cylinder(MatrixMultiply(MatrixScale(0.2f, 0.42f, 0.2f), MatrixTranslate(4.6f, 0, 0.5f)), RGBA(150, 78, 50));
    b.Cylinder(MatrixMultiply(MatrixScale(0.22f, 0.05f, 0.22f), MatrixTranslate(4.6f, 0.38f, 0.5f)), RGBA(130, 66, 42));
    // pendant lamp chain + desk lamp base are drawn dynamically; papers on the desk
    b.Box(Vector3{ 0.55f, DeskTopY + 0.004f, 0.16f }, Vector3{ 0.24f, 0.008f, 0.32f }, RGBA(232, 222, 198), 0.15f);
    b.Box(Vector3{ 0.56f, DeskTopY + 0.010f, 0.17f }, Vector3{ 0.24f, 0.004f, 0.32f }, RGBA(240, 232, 212), 0.08f);
    // pencil cup
    b.Cylinder(MatrixMultiply(MatrixScale(0.04f, 0.11f, 0.04f), MatrixTranslate(-0.55f, DeskTopY, 0.12f)), RGBA(60, 40, 30));
    for (int i = 0; i < 4; i++) {
        float a = i * 1.6f;
        Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(0.006f, 0.17f, 0.006f), MatrixRotateZ(0.12f * cosf(a))),
                                  MatrixTranslate(-0.55f + 0.015f * cosf(a), DeskTopY + 0.02f, 0.12f + 0.015f * sinf(a)));
        b.Cylinder(m, i % 2 ? RGBA(200, 160, 40) : RGBA(40, 80, 140), 6);
    }
    // inkwell
    b.Cylinder(MatrixMultiply(MatrixScale(0.035f, 0.05f, 0.035f), MatrixTranslate(0.78f, DeskTopY, 0.32f)), RGBA(20, 22, 34));
    b.Cylinder(MatrixMultiply(MatrixScale(0.018f, 0.02f, 0.018f), MatrixTranslate(0.78f, DeskTopY + 0.05f, 0.32f)), kBrass);
}

// ---------------------------------------------------------------------------
void Scene::Build() {
    const Content& C = Data();
    // ---- fetchable books ----
    shelf.clear();
    for (const Topic& t : C.topics) {
        ShelfBook sb;
        sb.key = t.id;
        sb.title = t.name;
        sb.color = t.color;
        float x = BayCenter(t.bay) + (t.side == 0 ? -0.36f : 0.32f);
        sb.inShelf = book::StandingInShelf(Vector3{ x, BoardTop[2], ShelfFrontZ - 0.012f });
        sb.standPos = Vector3{ x + 0.02f, 0, ShelfFrontZ + 0.56f };
        sb.reachY = BoardTop[2] + book::D * 0.5f;
        shelf.push_back(sb);
    }
    struct Special { const char* key; const char* title; Color c; int bay; };
    const Special specials[] = {
        { "paper_exam1", "Exam 1 Paper", RGBA(140, 120, 90), 0 },
        { "workbook_exam1", "Coding Exam 1 Workbook", RGBA(40, 70, 110), 1 },
        { "paper_exam2", "Exam 2 Paper", RGBA(120, 100, 80), 2 },
        { "workbook_exam2", "Coding Exam 2 Workbook", RGBA(110, 40, 40), 3 },
        { "workbook_cpp", "C++ Workbook", RGBA(40, 90, 70), 4 },
        { "paper_all", "Complete Paper", RGBA(90, 70, 110), 5 },
    };
    for (const Special& s : specials) {
        ShelfBook sb;
        sb.key = s.key; sb.title = s.title; sb.color = s.c;
        float x = BayCenter(s.bay) - 0.02f;
        sb.inShelf = book::StandingInShelf(Vector3{ x, BoardTop[1], ShelfFrontZ - 0.012f });
        sb.standPos = Vector3{ x + 0.02f, 0, ShelfFrontZ + 0.55f };
        sb.reachY = BoardTop[1] + book::D * 0.5f;
        shelf.push_back(sb);
    }

    // ---- static geometry ----
    gfx::Builder b;
    BuildRoom(b);
    staticMeshes.push_back(b.Build());
    BuildShelves(b);
    staticMeshes.push_back(b.Build());
    BuildSideShelves(b, -5.36f, 1.f);
    BuildSideShelves(b, 5.36f, -1.f);
    staticMeshes.push_back(b.Build());
    BuildDesk(b);
    BuildFurniture(b);
    staticMeshes.push_back(b.Build());

    // ---- labels ----
    const char* bayNames[Bays] = { "C FUNDAMENTALS  I", "C FUNDAMENTALS  II", "POINTERS & MEMORY", "LISTS & OBJECTS", "C++ TECHNIQUES", "STANDARD LIBRARY" };
    for (int i = 0; i < Bays; i++)
        AddLabel(bayNames[i], Vector3{ BayCenter(i), BoardTop[7] + 0.12f, ShelfFrontZ + 0.071f }, Vector2{ 1.16f, 0.17f }, 0, 680, 100, 46, false);
    for (const ShelfBook& sb : shelf) {
        Vector3 o = gfx::Origin(sb.inShelf);
        float y = o.y - book::D * 0.5f - 0.045f;
        std::string text = sb.title;
        AddLabel(text, Vector3{ o.x + 0.02f, y, ShelfFrontZ + 0.003f }, Vector2{ 0.3f, 0.042f }, 0, 420, 60, 34, true);
    }
    AddLabel("A. MALLOC\nHead Librarian", Vector3{ 0.42f, DeskTopY + 0.045f, DeskFrontZ - 0.07f }, Vector2{ 0.30f, 0.075f }, 0, 480, 120, 52, true);
    // the nameplate leans back slightly: rebuild its matrix
    {
        nameplateIdx = (int)labels.size() - 1;
        Label& L = labels.back();
        Vector3 c{ 0.42f, DeskTopY + 0.045f, DeskFrontZ - 0.065f };
        L.m = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(0.30f, 1, 0.075f), MatrixRotateX(PI * 0.5f - 0.35f)), MatrixRotateY(0)),
                             MatrixTranslate(c.x, c.y, c.z));
    }
    rugTex = MakeRugTexture();

    // dust motes
    util::Rng rng(55);
    for (int i = 0; i < 110; i++) {
        Mote m;
        m.p = Vector3{ rng.R(-2.6f, 2.6f), rng.R(0.6f, 3.4f), rng.R(-2.8f, 1.2f) };
        m.v = Vector3{ rng.R(-0.02f, 0.02f), rng.R(-0.01f, 0.012f), rng.R(-0.02f, 0.02f) };
        m.phase = rng.R(0, 6.28f);
        motes.push_back(m);
    }
    for (int i = 0; i < 40; i++) pileColors.push_back(util::Shade(kBookPalette[rng.I(0, 17)], rng.R(0.85f, 1.15f)));
}

void Scene::SetNameplate(const std::string& text) {
    if (nameplateIdx < 0 || nameplateIdx >= (int)labels.size()) return;
    // render the new lettering as a fresh label, then move its texture onto the existing leaning plate
    AddLabel(text, Vector3{ 0, 0, 0 }, Vector2{ 0.30f, 0.075f }, 0, 480, 120, 52, true);
    Label fresh = labels.back();
    labels.pop_back();
    UnloadTexture(labels[(size_t)nameplateIdx].tex);
    labels[(size_t)nameplateIdx].tex = fresh.tex;
}

void Scene::Shutdown() {
    for (auto& m : staticMeshes) UnloadMesh(m);
    for (auto& l : labels) UnloadTexture(l.tex);
    UnloadTexture(rugTex);
}

void Scene::Update(float dt, float time) {
    for (auto& m : motes) {
        m.p = Vector3Add(m.p, Vector3Scale(m.v, dt));
        m.p.y += sinf(time * 0.4f + m.phase) * 0.004f * dt;
        if (m.p.x < -2.8f) m.p.x = 2.8f; if (m.p.x > 2.8f) m.p.x = -2.8f;
        if (m.p.y < 0.5f) m.p.y = 3.4f; if (m.p.y > 3.5f) m.p.y = 0.6f;
        if (m.p.z < -3.0f) m.p.z = 1.3f; if (m.p.z > 1.4f) m.p.z = -2.9f;
    }
    if (catPet > 0) catPet = std::max(0.f, catPet - dt);
    if (bellRing > 0) bellRing = std::max(0.f, bellRing - dt);
}

std::vector<gfx::Light> Scene::Lights(float time) const {
    float flicker = 1.f + 0.025f * sinf(time * 13.f) + 0.015f * sinf(time * 29.f + 1.3f);
    std::vector<gfx::Light> L(4);
    L[0] = { Vector3{ LampPos.x + 0.05f, LampPos.y + 0.16f, LampPos.z + 0.12f }, Vector3Scale(Vector3{ 1.0f, 0.84f, 0.58f }, 1.5f * flicker), 3.6f };
    L[1] = { Vector3{ 0.0f, 2.72f, 0.9f }, Vector3Scale(Vector3{ 1.0f, 0.90f, 0.72f }, 1.55f), 8.0f };
    L[2] = { Vector3{ 0.0f, 2.55f, -1.6f }, Vector3Scale(Vector3{ 1.0f, 0.92f, 0.76f }, 1.4f), 8.0f };
    L[3] = { Vector3{ -4.8f, 2.6f, -1.3f }, Vector3Scale(Vector3{ 1.0f, 0.95f, 0.82f }, 1.3f), 9.5f };   // daylight from the window
    return L;
}

int Scene::FindShelfBook(const std::string& key) const {
    for (int i = 0; i < (int)shelf.size(); i++) if (shelf[(size_t)i].key == key) return i;
    return -1;
}

Matrix Scene::PileSlot(int index) const {
    // books stacked on the right-hand corner of the desk, slight random twist
    float y = DeskTopY + index * (book::PageY * 2.f + 0.001f);
    float yaw = sinf(index * 2.3f) * 0.18f + 0.1f;
    Matrix rot = MatrixRotateY(yaw);
    Vector3 c{ 0.98f, y, 0.16f };
    // book origin is the spine; centre the stack on c
    return MatrixMultiply(MatrixMultiply(MatrixTranslate(-book::W * 0.5f, 0, 0), rot), MatrixTranslate(c.x, c.y, c.z));
}

Matrix Scene::ChairMatrix() const {
    return MatrixTranslate(0, 0, -0.40f - 0.32f * chairBack);
}

void Scene::DrawChair() const {
    Matrix m = ChairMatrix();
    auto box = [&](Vector3 c, Vector3 s, Color col) { gfx::Cube(MatrixMultiply(gfx::BoxM(c, s), m), col); };
    Color leather = RGBA(126, 38, 38), wood = kWoodDark;
    box(Vector3{ 0, 0.48f, 0 }, Vector3{ 0.54f, 0.08f, 0.5f }, leather);
    box(Vector3{ 0, 0.43f, 0 }, Vector3{ 0.56f, 0.04f, 0.52f }, wood);
    box(Vector3{ 0, 0.89f, -0.24f }, Vector3{ 0.54f, 0.72f, 0.07f }, leather);
    box(Vector3{ 0, 1.27f, -0.25f }, Vector3{ 0.58f, 0.06f, 0.09f }, wood);
    for (int s = -1; s <= 1; s += 2) {
        box(Vector3{ s * 0.3f, 0.65f, -0.02f }, Vector3{ 0.06f, 0.05f, 0.46f }, wood);
        box(Vector3{ s * 0.3f, 0.56f, 0.18f }, Vector3{ 0.05f, 0.16f, 0.05f }, wood);
        for (int t = -1; t <= 1; t += 2) box(Vector3{ s * 0.24f, 0.205f, t * 0.21f }, Vector3{ 0.05f, 0.41f, 0.05f }, wood);
    }
}

void Scene::DrawLamp(float time) const {
    // banker's lamp
    Vector3 p = LampPos;
    gfx::Cylinder(MatrixMultiply(MatrixScale(0.075f, 0.022f, 0.075f), MatrixTranslate(p.x, p.y, p.z)), kBrass);
    gfx::Cylinder(MatrixMultiply(MatrixScale(0.011f, 0.22f, 0.011f), MatrixTranslate(p.x, p.y + 0.02f, p.z - 0.02f)), kBrass);
    Matrix shade = MatrixMultiply(MatrixMultiply(MatrixScale(0.058f, 0.28f, 0.058f), MatrixRotateZ(PI * 0.5f)), MatrixTranslate(p.x + 0.14f, p.y + 0.245f, p.z + 0.02f));
    gfx::Cylinder(MatrixMultiply(MatrixScale(1.0f, 1.0f, 0.72f), shade), RGBA(28, 104, 62), 0.35f);
    gfx::Box(Vector3{ p.x, p.y + 0.188f, p.z + 0.02f }, Vector3{ 0.25f, 0.005f, 0.06f }, RGBA(255, 232, 170), 0.f, 1.0f);
    // pendant lamp
    gfx::Cylinder(MatrixMultiply(MatrixScale(0.008f, 1.3f, 0.008f), MatrixTranslate(0, 2.95f, 0.35f)), RGBA(60, 50, 40));
    gfx::Cone(MatrixMultiply(MatrixMultiply(MatrixScale(0.24f, 0.18f, 0.24f), MatrixRotateX(PI)), MatrixTranslate(0, 2.98f, 0.35f)), RGBA(30, 90, 56), 0.2f);
    gfx::Sphere(MatrixMultiply(MatrixScale(0.05f, 0.05f, 0.05f), MatrixTranslate(0, 2.80f, 0.35f)), RGBA(255, 236, 190), 1.f);
    // window panes (moonlit)
    gfx::Box(Vector3{ -5.85f, 2.2f, -1.3f }, Vector3{ 0.01f, 2.3f, 1.42f }, RGBA(150, 204, 248), 0, 1.0f);
    gfx::Sphere(MatrixMultiply(MatrixScale(0.01f, 0.16f, 0.16f), MatrixTranslate(-5.84f, 2.85f, -1.6f)), RGBA(255, 244, 196), 1.f);
    // candle on the reading table
    gfx::Cylinder(MatrixMultiply(MatrixScale(0.025f, 0.14f, 0.025f), MatrixTranslate(-3.5f, 0.785f, -1.55f)), RGBA(236, 226, 200));
    float fl = 1.f + 0.15f * sinf(time * 17.f);
    gfx::Sphere(MatrixMultiply(MatrixScale(0.012f, 0.025f * fl, 0.012f), MatrixTranslate(-3.5f, 0.945f, -1.55f)), RGBA(255, 200, 100), 1.f);
    // globe
    gfx::Sphere(MatrixMultiply(MatrixMultiply(MatrixScale(0.2f, 0.2f, 0.2f), MatrixRotateY(time * 0.15f)), MatrixTranslate(3.6f, 0.92f, -1.2f)), RGBA(70, 110, 120));
    gfx::Torus(MatrixMultiply(MatrixMultiply(MatrixScale(0.22f, 0.22f, 0.22f), MatrixRotateY(PI * 0.5f)), MatrixTranslate(3.6f, 0.92f, -1.2f)), kBrass);
    // plant leaves
    for (int i = 0; i < 9; i++) {
        float a = i * 0.7f;
        float h = 0.55f + 0.08f * (i % 3);
        Matrix m = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(0.05f, 0.3f, 0.12f), MatrixRotateZ(0.6f + 0.1f * sinf(time * 0.7f + i))), MatrixRotateY(a)),
                                  MatrixTranslate(4.6f + cosf(a) * 0.08f, h, 0.5f + sinf(a) * 0.08f));
        gfx::Sphere(m, RGBA(40, 96, 50));
    }
}

void Scene::DrawClock(const Camera3D&) const {
    Vector3 c{ 0, 3.82f, -3.70f };
    Matrix face = MatrixMultiply(MatrixMultiply(MatrixScale(0.28f, 0.05f, 0.28f), MatrixRotateX(PI * 0.5f)), MatrixTranslate(c.x, c.y, c.z));
    gfx::Cylinder(face, kWoodDark);
    gfx::Cylinder(MatrixMultiply(MatrixMultiply(MatrixScale(0.25f, 0.052f, 0.25f), MatrixRotateX(PI * 0.5f)), MatrixTranslate(c.x, c.y, c.z + 0.002f)), RGBA(232, 220, 190), 0.25f);
    for (int i = 0; i < 12; i++) {
        float a = i * PI / 6.f;
        float l = i % 3 == 0 ? 0.04f : 0.022f;
        gfx::Cube(MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(0.008f, l, 0.004f), MatrixTranslate(0, 0.2f, 0)), MatrixRotateZ(-a)),
                                 MatrixTranslate(c.x, c.y, c.z + 0.056f)), RGBA(40, 30, 20));
    }
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
    localtime_s(&tmv, &t);
    float minA = (tmv.tm_min + tmv.tm_sec / 60.f) / 60.f * 2 * PI;
    float hrA = ((tmv.tm_hour % 12) + tmv.tm_min / 60.f) / 12.f * 2 * PI;
    auto hand = [&](float ang, float len, float w, Color col, float z) {
        gfx::Cube(MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(w, len, 0.004f), MatrixTranslate(0, len * 0.4f, 0)), MatrixRotateZ(-ang)),
                                 MatrixTranslate(c.x, c.y, c.z + z)), col);
    };
    hand(hrA, 0.13f, 0.014f, RGBA(30, 24, 20), 0.06f);
    hand(minA, 0.19f, 0.009f, RGBA(30, 24, 20), 0.064f);
    gfx::Sphere(MatrixMultiply(MatrixScale(0.012f, 0.012f, 0.012f), MatrixTranslate(c.x, c.y, c.z + 0.066f)), kBrass);
}

void Scene::DrawCat(float time) const {
    // Segfault, the library cat, asleep on the corner of the desk
    Vector3 p = CatPos;
    float breathe = 1.f + 0.035f * sinf(time * 1.9f);
    float perk = util::Smooth(catPet > 0 ? std::min(1.f, catPet * 2.f) : 0.f);
    Color fur = RGBA(52, 48, 50), furLight = RGBA(80, 74, 76), pink = RGBA(200, 130, 130);
    Matrix base = MatrixMultiply(MatrixRotateY(-0.5f), MatrixTranslate(p.x, p.y, p.z));
    auto S = [&](Vector3 c, Vector3 s, Color col, float yaw = 0.f, float pitch = 0.f) {
        Matrix m = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(s.x, s.y, s.z), MatrixRotateX(pitch)), MatrixRotateY(yaw)), MatrixTranslate(c.x, c.y, c.z));
        gfx::Sphere(MatrixMultiply(m, base), col);
    };
    S(Vector3{ 0, 0.055f * breathe, 0 }, Vector3{ 0.11f, 0.06f * breathe, 0.085f }, fur);
    S(Vector3{ 0.03f, 0.075f * breathe, 0.0f }, Vector3{ 0.07f, 0.04f, 0.06f }, furLight);
    float headLift = perk * 0.03f;
    Vector3 h{ -0.09f, 0.06f + headLift, 0.045f };
    S(h, Vector3{ 0.048f, 0.042f, 0.046f }, fur);
    for (int s = -1; s <= 1; s += 2) {
        Matrix ear = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(0.018f, 0.03f, 0.012f), MatrixRotateZ(s * 0.3f)), MatrixRotateY(0.2f)),
                                    MatrixTranslate(h.x, h.y + 0.032f, h.z + s * 0.024f));
        gfx::Cone(MatrixMultiply(ear, base), fur);
    }
    // eyes: closed lines while asleep, open when petted
    for (int s = -1; s <= 1; s += 2) {
        float open = perk;
        Matrix eye = MatrixMultiply(MatrixScale(0.006f, 0.0015f + 0.006f * open, 0.008f), MatrixTranslate(h.x - 0.042f, h.y + 0.008f, h.z + s * 0.018f));
        gfx::Sphere(MatrixMultiply(eye, base), open > 0.3f ? RGBA(170, 200, 60) : RGBA(20, 18, 18), open > 0.3f ? 0.6f : 0.f);
    }
    S(Vector3{ h.x - 0.046f, h.y - 0.008f, h.z }, Vector3{ 0.006f, 0.005f, 0.007f }, pink);
    // tail curled around the body
    {   // one smooth tapered tail, curled round the body and gently swaying (a tube, not a chain of balls)
        std::vector<Vector3> pts;
        std::vector<float> rad;
        const int N = 22;
        for (int i = 0; i < N; i++) {
            float u = (float)i / (N - 1);
            float a = -0.4f + u * 2.24f + 0.08f * sinf(time * 1.3f + u * 3.5f);
            pts.push_back(Vector3{ cosf(a) * (0.1f - 0.012f * u), 0.024f + 0.004f * sinf(u * 6.f), sinf(a) * 0.08f + 0.02f });
            rad.push_back(0.026f - 0.006f * u);
        }
        gfx::TubeSet tail;
        std::vector<Vector3> tp(pts.begin(), pts.end() - 4);
        tail.Add(tp, std::vector<float>(rad.begin(), rad.end() - 4), fur);
        tail.Add(std::vector<Vector3>(pts.end() - 5, pts.end()), { 0.0215f, 0.0205f, 0.019f, 0.0175f, 0.0f + 0.0125f }, furLight);   // paler tip
        tail.Flush(5, base);
        S(pts.back(), Vector3{ 0.0125f, 0.0125f, 0.0125f }, furLight);   // rounded tip
    }
}

void Scene::DrawBell() const {
    Vector3 p = BellPos;
    float ring = bellRing > 0 ? sinf(bellRing * 60.f) * bellRing * 0.08f : 0.f;
    gfx::Cylinder(MatrixMultiply(MatrixScale(0.05f, 0.012f, 0.05f), MatrixTranslate(p.x, p.y, p.z)), kWoodDark);
    gfx::Hemi(MatrixMultiply(MatrixMultiply(MatrixScale(0.042f, 0.04f, 0.042f), MatrixRotateZ(ring)), MatrixTranslate(p.x, p.y + 0.012f, p.z)), RGBA(216, 180, 100));
    gfx::Cylinder(MatrixMultiply(MatrixScale(0.006f, 0.02f, 0.006f), MatrixTranslate(p.x, p.y + 0.05f, p.z)), kBrass);
    gfx::Sphere(MatrixMultiply(MatrixScale(0.01f, 0.007f, 0.01f), MatrixTranslate(p.x, p.y + 0.072f, p.z)), kBrass);
}

void Scene::DrawDrawer() const {
    float z = DeskBackZ + 0.008f - drawerOpen * 0.36f;
    Vector3 c{ 0.92f, 0.6f, z };
    gfx::Box(c, Vector3{ 0.54f, 0.18f, 0.02f }, kWoodLight);
    gfx::Box(Vector3{ c.x, c.y, c.z - 0.016f }, Vector3{ 0.07f, 0.018f, 0.02f }, kBrass);
    if (drawerOpen > 0.01f) {
        float depth = drawerOpen * 0.36f;
        gfx::Box(Vector3{ c.x, c.y - 0.07f, c.z + depth * 0.5f }, Vector3{ 0.5f, 0.02f, depth }, kWood);
        gfx::Box(Vector3{ c.x - 0.25f, c.y - 0.02f, c.z + depth * 0.5f }, Vector3{ 0.015f, 0.12f, depth }, kWood);
        gfx::Box(Vector3{ c.x + 0.25f, c.y - 0.02f, c.z + depth * 0.5f }, Vector3{ 0.015f, 0.12f, depth }, kWood);
    }
}

void Scene::DrawPile() const {
    for (int i = 0; i < pileCount && i < 9; i++)
        book::DrawClosedBook(PileSlot(i), pileColors[(size_t)i % pileColors.size()], i % 2 == 0);
}

void Scene::Draw(const Camera3D& cam, float time) const {
    for (const Mesh& m : staticMeshes) gfx::DrawMeshLit(m, MatrixIdentity(), WHITE);
    gfx::Quad(MatrixMultiply(MatrixScale(4.4f, 1, 3.6f), MatrixTranslate(0, 0.003f, 0.3f)), rugTex, WHITE);
    for (const Label& l : labels) gfx::Quad(l.m, l.tex, WHITE, 0.15f);
    for (const ShelfBook& sb : shelf) if (sb.present) book::DrawClosedBook(sb.inShelf, sb.color);
    DrawChair();
    DrawLamp(time);
    DrawClock(cam);
    DrawCat(time);
    DrawBell();
    DrawDrawer();
    DrawPile();
}

void Scene::DrawTransparent(const Camera3D& cam, float time) const {
    gfx::BeginTransparent();
    gfx::BlobShadow(Vector3{ LampPos.x + 0.05f, DeskTopY + 0.002f, LampPos.z + 0.03f }, 0.3f, 0.22f, 0.45f);
    gfx::BlobShadow(Vector3{ CatPos.x, DeskTopY + 0.002f, CatPos.z }, 0.34f, 0.28f, 0.55f, -0.5f);
    gfx::BlobShadow(Vector3{ BellPos.x, DeskTopY + 0.002f, BellPos.z }, 0.12f, 0.12f, 0.5f);
    gfx::BlobShadow(Vector3{ 0, 0.006f, 0.55f }, 3.2f, 1.9f, 0.55f);
    gfx::BlobShadow(Vector3{ 3.6f, 0.006f, -1.2f }, 0.6f, 0.6f, 0.4f);
    gfx::BlobShadow(Vector3{ -3.0f, 0.006f, -1.7f }, 2.0f, 1.2f, 0.45f);
    if (pileCount > 0) gfx::BlobShadow(Vector3{ 0.98f, DeskTopY + 0.002f, 0.16f }, 0.42f, 0.5f, 0.5f);
    gfx::EndTransparent();
    // glows
    gfx::Glow(Vector3{ 0, 2.78f, 0.35f }, 0.55f, util::WithAlpha(RGBA(255, 210, 140), 0.5f), cam);
    gfx::Glow(Vector3{ LampPos.x + 0.05f, LampPos.y + 0.17f, LampPos.z + 0.05f }, 0.38f, util::WithAlpha(RGBA(255, 220, 150), 0.32f), cam);
    float fl = 0.8f + 0.2f * sinf(time * 11.f);
    gfx::Glow(Vector3{ -3.5f, 0.95f, -1.55f }, 0.3f * fl, util::WithAlpha(RGBA(255, 180, 80), 0.6f), cam);
    gfx::Glow(Vector3{ -5.8f, 2.85f, -1.6f }, 0.7f, util::WithAlpha(RGBA(255, 240, 200), 0.4f), cam);
    // dust motes catching the light
    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ADDITIVE);
    Texture2D g = gfx::GlowTexture();
    for (const Mote& m : motes) {
        float tw = 0.5f + 0.5f * sinf(time * 1.7f + m.phase * 3.f);
        DrawBillboard(cam, g, m.p, 0.025f, util::WithAlpha(RGBA(255, 220, 170), 0.18f * tw));
    }
    EndBlendMode();
}
