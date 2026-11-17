// book3d.cpp - hinged 3D book with a generated cover and live page texture
#include "book3d.h"
#include "gfx.h"
#include "ui.h"

namespace book {

static const Color kPaper = { 238, 229, 206, 255 };
static const Color kPaperEdge = { 214, 200, 170, 255 };
static const Color kGold = { 214, 176, 96, 255 };

void Book3D::Init() {
    // render textures are stored upside down, so v runs 1 -> 0 from the top edge
    leftPage = gfx::MakeQuadMesh(0.0f, 1.0f, 0.5f, 0.0f);
    rightPage = gfx::MakeQuadMesh(0.5f, 1.0f, 1.0f, 0.0f);
    coverQuad = gfx::MakeQuadMesh(0, 0, 1, 1);
}

void Book3D::Shutdown() {
    UnloadMesh(leftPage);
    UnloadMesh(rightPage);
    UnloadMesh(coverQuad);
    if (hasCover) UnloadTexture(coverTex);
}

void Book3D::SetLook(const Look& l) {
    look = l;
    if (hasCover) UnloadTexture(coverTex);
    const int TW = 512, TH = 664;
    RenderTexture2D rt = LoadRenderTexture(TW, TH);
    BeginTextureMode(rt);
    ClearBackground(l.cover);
    ui::Begin(Vector2{ 0, 0 }, 1.f, false);
    Color light = util::Shade(l.cover, 1.25f), dark = util::Shade(l.cover, 0.62f);
    DrawRectangleGradientV(0, 0, TW, TH / 2, light, l.cover);
    DrawRectangleGradientV(0, TH / 2, TW, TH / 2, l.cover, dark);
    // leather grain speckle
    util::Rng rng(1234u + (unsigned)l.title.size() * 77u);
    for (int i = 0; i < 900; i++) {
        float x = rng.R(0, (float)TW), y = rng.R(0, (float)TH);
        DrawCircleV(Vector2{ x, y }, rng.R(0.6f, 1.8f), util::WithAlpha(rng.F() < 0.5f ? BLACK : WHITE, rng.R(0.03f, 0.08f)));
    }
    // borders
    DrawRectangleLinesEx(Rectangle{ 22, 22, TW - 44.f, TH - 44.f }, 4, kGold);
    DrawRectangleLinesEx(Rectangle{ 34, 34, TW - 68.f, TH - 68.f }, 1.5f, util::WithAlpha(kGold, 0.8f));
    const Vector2 corners[4] = { { 34, 34 }, { TW - 34.f, 34 }, { 34, TH - 34.f }, { TW - 34.f, TH - 34.f } };
    for (auto c : corners) { DrawPoly(c, 4, 12, 45, kGold); DrawPoly(c, 4, 6, 45, l.cover); }
    // volume line
    if (!l.volume.empty()) {
        float w = ui::MeasureWidth(FontId::Serif, l.volume, 24, 3);
        ui::Text(FontId::Serif, l.volume, (TW - w) * 0.5f, 62, 24, util::WithAlpha(kGold, 0.9f), 3);
    }
    ui::Ornament(TW * 0.5f, 104, 220, kGold);
    // title (wrapped & centred)
    {
        float size = 56;
        std::vector<std::string> words = util::Split(l.title, ' ');
        std::vector<std::string> lines;
        std::string cur;
        for (auto& w : words) {
            std::string t = cur.empty() ? w : cur + " " + w;
            if (ui::MeasureWidth(FontId::SerifBold, t, size) > TW - 120 && !cur.empty()) { lines.push_back(cur); cur = w; }
            else cur = t;
        }
        if (!cur.empty()) lines.push_back(cur);
        if (lines.size() > 2) size = 46;
        float y = 150;
        for (auto& ln : lines) {
            float w = ui::MeasureWidth(FontId::SerifBold, ln, size);
            ui::Text(FontId::SerifBold, ln, (TW - w) * 0.5f + 2, y + 3, size, util::WithAlpha(BLACK, 0.45f));
            ui::Text(FontId::SerifBold, ln, (TW - w) * 0.5f, y, size, kGold);
            y += size * 1.12f;
        }
    }
    // emblem medallion
    {
        Vector2 c{ TW * 0.5f, TH * 0.66f };
        DrawCircleV(c, 84, util::Shade(l.cover, 0.8f));
        DrawRing(c, 78, 84, 0, 360, 48, kGold);
        DrawRing(c, 70, 72, 0, 360, 48, util::WithAlpha(kGold, 0.7f));
        float es = l.emblem.size() > 3 ? 50.f : 64.f;
        float w = ui::MeasureWidth(FontId::MonoBold, l.emblem, es);
        ui::Text(FontId::MonoBold, l.emblem, c.x - w * 0.5f, c.y - es * 0.58f, es, kGold);
    }
    if (!l.subtitle.empty()) {
        float size = 24;
        float h = ui::Wrap(FontId::SerifItalic, l.subtitle, 0, 0, TW - 140, size, kGold, 1.25f, false);
        // centre each wrapped line manually
        std::vector<std::string> words = util::Split(l.subtitle, ' ');
        std::string cur;
        float y = TH - 92 - h * 0.5f;
        auto emit = [&](const std::string& s) {
            float w = ui::MeasureWidth(FontId::SerifItalic, s, size);
            ui::Text(FontId::SerifItalic, s, (TW - w) * 0.5f, y, size, util::WithAlpha(kGold, 0.92f));
            y += size * 1.25f;
        };
        for (auto& w : words) {
            std::string t = cur.empty() ? w : cur + " " + w;
            if (ui::MeasureWidth(FontId::SerifItalic, t, size) > TW - 140 && !cur.empty()) { emit(cur); cur = w; }
            else cur = t;
        }
        if (!cur.empty()) emit(cur);
    }
    ui::End();
    EndTextureMode();
    Image img = LoadImageFromTexture(rt.texture);
    ImageFlipVertical(&img);
    coverTex = LoadTextureFromImage(img);
    UnloadImage(img);
    UnloadRenderTexture(rt);
    GenTextureMipmaps(&coverTex);
    SetTextureFilter(coverTex, TEXTURE_FILTER_TRILINEAR);
    hasCover = true;
}

static Matrix Local(Vector3 c, Vector3 s, const Matrix& parent) { return MatrixMultiply(gfx::BoxM(c, s), parent); }

void Book3D::Draw() const {
    if (!visible) return;
    const Color cover = look.cover;
    const Color coverIn = util::Shade(cover, 0.85f);
    Texture2D white = gfx::WhiteTexture();
    const Texture2D& pt = pagesTextured ? pageTex : white;

    // ----- right half (back cover + right pages) -----
    gfx::Cube(Local(Vector3{ W * 0.5f + 0.003f, Cover * 0.5f, 0 }, Vector3{ W + 0.006f, Cover, D + 0.012f }, world), coverIn);
    gfx::Cube(Local(Vector3{ W * 0.5f, Cover + Half * 0.5f, 0 }, Vector3{ W - 0.004f, Half, D }, world), kPaperEdge);
    {
        Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(W - 0.004f, 1, D), MatrixTranslate(W * 0.5f, PageY + 0.0004f, 0)), world);
        gfx::DrawMeshLit(rightPage, m, pagesTextured ? WHITE : kPaper, pageEmissive, &pt);
    }

    // ----- left half (front cover + left pages), hinged about the spine -----
    float theta = -PI * (1.f - util::Clamp01(open));
    Matrix hinge = MatrixMultiply(MatrixMultiply(MatrixTranslate(0, -PageY, 0), MatrixRotateZ(theta)), MatrixTranslate(0, PageY, 0));
    Matrix L = MatrixMultiply(hinge, world);
    gfx::Cube(Local(Vector3{ -(W * 0.5f + 0.003f), Cover * 0.5f, 0 }, Vector3{ W + 0.006f, Cover, D + 0.012f }, L), cover);
    gfx::Cube(Local(Vector3{ -W * 0.5f, Cover + Half * 0.5f, 0 }, Vector3{ W - 0.004f, Half, D }, L), kPaperEdge);
    {
        Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(W - 0.004f, 1, D), MatrixTranslate(-W * 0.5f, PageY + 0.0004f, 0)), L);
        gfx::DrawMeshLit(leftPage, m, pagesTextured ? WHITE : kPaper, pageEmissive, &pt);
    }
    if (hasCover) {
        Matrix m = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(W + 0.006f, 1, D + 0.012f), MatrixRotateZ(PI)),
                                                 MatrixTranslate(-(W * 0.5f + 0.003f), -0.0007f, 0)), L);
        gfx::DrawMeshLit(coverQuad, m, WHITE, 0.15f, &coverTex);
    }
    // ----- spine (only meaningful while closed) -----
    if (open < 0.35f) {
        gfx::Cube(Local(Vector3{ -0.003f, PageY, 0 }, Vector3{ 0.006f, PageY * 2.f, D + 0.012f }, world), cover);
        for (int i = -1; i <= 1; i += 2)
            gfx::Cube(Local(Vector3{ -0.0062f, PageY, i * D * 0.32f }, Vector3{ 0.0012f, PageY * 1.8f, 0.012f }, world), kGold);
    }
}

Vector3 Book3D::SpreadTopLeft() const { return Vector3Transform(Vector3{ -W, PageY, -D * 0.5f }, world); }
Vector3 Book3D::SpreadBottomRight() const { return Vector3Transform(Vector3{ W, PageY, D * 0.5f }, world); }
Vector3 Book3D::SpreadCenter() const { return Vector3Transform(Vector3{ 0, PageY, 0 }, world); }

void DrawClosedBook(const Matrix& m, Color cover, bool goldBands) {
    const Color coverIn = util::Shade(cover, 0.9f);
    gfx::Cube(Local(Vector3{ W * 0.5f + 0.003f, Cover * 0.5f, 0 }, Vector3{ W + 0.006f, Cover, D + 0.012f }, m), coverIn);
    gfx::Cube(Local(Vector3{ W * 0.5f, PageY, 0 }, Vector3{ W - 0.004f, Half * 2.f, D }, m), kPaperEdge);
    gfx::Cube(Local(Vector3{ W * 0.5f + 0.003f, PageY * 2.f - Cover * 0.5f, 0 }, Vector3{ W + 0.006f, Cover, D + 0.012f }, m), cover);
    gfx::Cube(Local(Vector3{ -0.003f, PageY, 0 }, Vector3{ 0.006f, PageY * 2.f, D + 0.012f }, m), cover);
    if (goldBands)
        for (int i = -1; i <= 1; i += 2)
            gfx::Cube(Local(Vector3{ -0.0062f, PageY, i * D * 0.32f }, Vector3{ 0.0012f, PageY * 1.8f, 0.012f }, m), kGold);
}

Matrix StandingInShelf(Vector3 p) {
    return gfx::Basis(Vector3{ 0, 0, -1 }, Vector3{ 1, 0, 0 }, Vector3{ 0, -1, 0 }, Vector3{ p.x, p.y + D * 0.5f, p.z });
}

} // namespace book
