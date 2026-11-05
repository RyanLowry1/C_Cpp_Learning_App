// book2d.cpp - cover painting, closed-book and open-spread drawing
#include "book2d.h"
#include "art.h"
#include "ui.h"
#include "rlgl.h"
#include <algorithm>

using namespace util;
namespace pal = ui::pal;

namespace book {

static const int CW = 390, CH = 500;

void Book2D::Init() {
    cover = LoadRenderTexture(CW / 3, (CH + 2) / 3);        // low-res cover: it is blown up with hard pixels
    SetTextureFilter(cover.texture, TEXTURE_FILTER_POINT);
    ready = true;
}

void Book2D::Shutdown() {
    if (ready) UnloadRenderTexture(cover);
    ready = false;
}

static void CenteredWrap(FontId f, const std::string& s, float cx, float y, float width, float size, Color c, float lineH, float* outH = nullptr) {
    std::vector<std::string> words = Split(s, ' ');
    std::vector<std::string> lines;
    std::string cur;
    for (const std::string& w : words) {
        std::string test = cur.empty() ? w : cur + " " + w;
        if (!cur.empty() && ui::MeasureWidth(f, test, size) > width) { lines.push_back(cur); cur = w; }
        else cur = test;
    }
    if (!cur.empty()) lines.push_back(cur);
    float yy = y;
    for (const std::string& l : lines) {
        float w = ui::MeasureWidth(f, l, size);
        ui::Text(f, l, cx - w * 0.5f, yy, size, c);
        yy += size * lineH;
    }
    if (outH) *outH = yy - y;
}

void Book2D::SetLook(const Look& l) {
    look = l;
    if (!ready) return;
    BeginTextureMode(cover);
    ui::Begin(Vector2{ 0, 0 }, 1.f / 3.f, false);
    Color base = look.cover;
    DrawRectangleGradientV(0, 0, CW, CH, Shade(base, 1.18f), Shade(base, 0.8f));
    // faint leather grain
    // spine band
    DrawRectangle(0, 0, 40, CH, Shade(base, 0.62f));
    DrawRectangleGradientH(40, 0, 18, CH, WithAlpha(BLACK, 0.35f), WithAlpha(BLACK, 0.0f));
    for (int y : { 60, 440 }) { DrawRectangle(0, y, 40, 4, pal::Gold); DrawRectangle(0, y + 9, 40, 2, WithAlpha(pal::Gold, 0.7f)); }
    // gold frame
    Rectangle fr{ 66, 26, CW - 66 - 26.f, CH - 52.f };
    DrawRectangleLinesEx(fr, 3.f, pal::Gold);
    DrawRectangleLinesEx(Rectangle{ fr.x + 9, fr.y + 9, fr.width - 18, fr.height - 18 }, 1.5f, WithAlpha(pal::Gold, 0.75f));
    for (float cx : { fr.x, fr.x + fr.width }) for (float cy : { fr.y, fr.y + fr.height }) DrawRectangle((int)cx - 6, (int)cy - 6, 12, 12, pal::Gold);
    float mx = fr.x + fr.width * 0.5f;
    std::string vol = look.volume;
    float vs = 15.f;
    ui::Text(FontId::SerifBold, vol, mx - ui::MeasureWidth(FontId::SerifBold, vol, vs, 3.f) * 0.5f, fr.y + 28, vs, WithAlpha(pal::GoldSoft, 0.9f), 3.f);
    float es = 92.f;
    while (es > 30 && ui::MeasureWidth(FontId::SerifBold, look.emblem, es) > fr.width - 50) es -= 4;
    ui::Text(FontId::SerifBold, look.emblem, mx - ui::MeasureWidth(FontId::SerifBold, look.emblem, es) * 0.5f, fr.y + 62 + (92 - es) * 0.4f, es, pal::Gold);
    ui::Ornament(mx, fr.y + 200, 150, WithAlpha(pal::Gold, 0.85f));
    float th = 0;
    CenteredWrap(FontId::SerifBold, look.title, mx, fr.y + 224, fr.width - 40, 32, pal::GoldSoft, 1.16f, &th);
    CenteredWrap(FontId::SerifItalic, look.subtitle, mx, fr.y + 224 + th + 8, fr.width - 56, 17, WithAlpha(pal::GoldSoft, 0.8f), 1.2f);
    ui::End();
    EndTextureMode();
}

void Book2D::DrawClosed(Vector2 c, float w, float squash, float rotDeg, bool shadow) const {
    float h = w * CoverAspect * squash;
    if (shadow) art::Shadow(Vector2{ c.x + w * 0.03f, c.y + h * 0.4f }, w * 0.68f, h * 0.62f, 0.5f);
    rlPushMatrix();
    rlTranslatef(c.x, c.y, 0);
    rlRotatef(rotDeg, 0, 0, 1);
    DrawRectangleRec(Rectangle{ -w * 0.5f + w * 0.035f, -h * 0.5f + h * 0.03f, w, h }, Color{ 226, 210, 172, 255 });
    DrawRectangleLinesEx(Rectangle{ -w * 0.5f + w * 0.035f, -h * 0.5f + h * 0.03f, w, h }, 1.f, Color{ 160, 140, 100, 255 });
    DrawTexturePro(cover.texture, Rectangle{ 0, 0, (float)cover.texture.width, -(float)cover.texture.height }, Rectangle{ -w * 0.5f, -h * 0.5f, w, h }, Vector2{ 0, 0 }, 0, WHITE);
    rlPopMatrix();
}

void Book2D::DrawSpread(Vector2 c, float s, float squash, float open, Texture2D pageTex) const {
    float pw = PageW * s, h = PageH * s * squash;
    float p = Clamp01(open);
    float ang = p * PI;
    float cs = cosf(ang), sn = sinf(ang);
    float spine = c.x - (pw * 0.5f) * (1.f - p);
    float top = c.y - h * 0.5f;
    float lift = h * 0.035f * sn;
    Color board = Shade(look.cover, 0.72f);

    art::Shadow(Vector2{ c.x + pw * 0.03f, c.y + h * 0.07f }, pw * (0.64f + 0.52f * p), h * 0.6f, 0.5f);
    // right half: leather board, page block, page
    float e = std::max(2.f, 7.f * s * 2.f);
    DrawRectangleRec(Rectangle{ spine - 2, top - e, pw + e + 2, h + e * 2 }, board);
    DrawRectangleRec(Rectangle{ spine, top, pw + e * 0.5f, h + e * 0.5f }, Color{ 222, 206, 168, 255 });
    DrawTexturePro(pageTex, Rectangle{ pageTex.width * 0.5f, 0, pageTex.width * 0.5f, -(float)pageTex.height }, Rectangle{ spine, top, pw, h }, Vector2{ 0, 0 }, 0, WHITE);
    // soft shade where the swinging cover falls across the right page
    if (p > 0.02f && p < 0.98f) DrawRectangleRec(Rectangle{ spine, top, pw, h }, WithAlpha(BLACK, 0.16f * sn));

    float cw = pw * cs;   // signed: + = cover over the right half, - = left page over the left half
    if (cw >= 0.f) {
        DrawTexturePro(cover.texture, Rectangle{ 0, 0, (float)cover.texture.width, -(float)cover.texture.height }, Rectangle{ spine, top - lift, std::max(1.f, cw), h + lift * 2 }, Vector2{ 0, 0 }, 0, WHITE);
        if (p > 0.f) DrawRectangleRec(Rectangle{ spine, top - lift, std::max(1.f, cw), h + lift * 2 }, WithAlpha(BLACK, 0.22f * sn));
    } else {
        float w = -cw;
        DrawRectangleRec(Rectangle{ spine - w - e, top - e, w + e + 2, h + e * 2 }, board);
        DrawRectangleRec(Rectangle{ spine - w, top - lift, w, h + lift * 2 }, Color{ 222, 206, 168, 255 });
        DrawTexturePro(pageTex, Rectangle{ 0, 0, pageTex.width * 0.5f, -(float)pageTex.height }, Rectangle{ spine - w, top - lift, std::max(1.f, w), h + lift * 2 }, Vector2{ 0, 0 }, 0, Shade(WHITE, 0.74f + 0.26f * (-cs)));
    }
}

void Book2D::DrawBoard(Vector2 c, float s) const {
    float pw = PageW * s, h = PageH * s;
    float e = std::max(2.f, 14.f * s);
    art::Shadow(Vector2{ c.x + pw * 0.03f, c.y + h * 0.07f }, pw * 1.16f, h * 0.6f, 0.5f);
    DrawRectangleRec(Rectangle{ c.x - pw - e, c.y - h * 0.5f - e, 2.f * pw + 2.f * e, h + 2.f * e }, Shade(look.cover, 0.72f));
    DrawRectangleRec(Rectangle{ c.x - pw, c.y - h * 0.5f, 2.f * pw + e * 0.5f, h + e * 0.5f }, Color{ 222, 206, 168, 255 });
}

void DrawSpine(Rectangle r, Color c, const std::string& mark, bool highlight) {
    DrawRectangleRec(r, c);
    DrawRectangleRec(Rectangle{ r.x + r.width * 0.78f, r.y, r.width * 0.22f, r.height }, WithAlpha(BLACK, 0.2f));
    DrawRectangleRec(Rectangle{ r.x, r.y, r.width * 0.12f, r.height }, WithAlpha(WHITE, 0.1f));
    DrawRectangle((int)r.x, (int)(r.y + r.height * 0.14f), (int)r.width, 3, pal::Gold);
    DrawRectangle((int)r.x, (int)(r.y + r.height * 0.86f), (int)r.width, 3, pal::Gold);
    if (highlight) {
        DrawRectangle((int)r.x, (int)(r.y + r.height * 0.2f), (int)r.width, 1, WithAlpha(pal::GoldSoft, 0.8f));
        DrawRectangle((int)r.x, (int)(r.y + r.height * 0.8f), (int)r.width, 1, WithAlpha(pal::GoldSoft, 0.8f));
    }
    if (!mark.empty()) {
        float fs = 14.f;
        while (fs > 8 && ui::MeasureWidth(FontId::SerifBold, mark, fs) > r.width - 4) fs -= 1;
        ui::TextCentered(FontId::SerifBold, mark, Rectangle{ r.x, r.y + r.height * 0.2f, r.width, r.height * 0.6f }, fs, pal::GoldSoft);
    }
}

} // namespace book
