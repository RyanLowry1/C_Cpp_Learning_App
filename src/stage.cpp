// stage.cpp - the painted library set
#include "stage.h"
#include "art.h"
#include "book2d.h"
#include "content.h"
#include "ui.h"
#include "rlgl.h"
#include <algorithm>
#include <ctime>

using namespace util;
namespace pal = ui::pal;

static const Color kBrass{ 202, 160, 74, 255 };
static const Color kBrassDark{ 140, 104, 40, 255 };
static const Rectangle kLightWorld{ -400.f, -100.f, 2400.f, 1100.f };   // scene-space area covered by the light map

static const Color kBookPalette[] = {
    { 122, 32, 36, 255 }, { 34, 54, 104, 255 }, { 30, 86, 52, 255 }, { 168, 118, 32, 255 }, { 92, 40, 96, 255 },
    { 28, 96, 100, 255 }, { 176, 78, 30, 255 }, { 54, 72, 110, 255 }, { 110, 72, 42, 255 }, { 70, 52, 38, 255 },
    { 120, 100, 80, 255 }, { 88, 34, 52, 255 }, { 60, 76, 58, 255 }, { 140, 90, 60, 255 }, { 46, 46, 70, 255 },
};

Vector2 Stage::Shift(int layer, Vector2 par) const {
    const float amp[3] = { 7.f, 13.f, 26.f };
    return Vector2{ -par.x * amp[layer], 0.f };
}

// ---------------------------------------------------------------------------
void Stage::Build() {
    const Content& C = Data();
    shelf.clear();
    for (const Topic& tp : C.topics) {
        ShelfBook sb;
        sb.key = tp.id; sb.title = tp.name; sb.mark = tp.emblem; sb.color = tp.color;
        float x = world::Bay0X + tp.bay * world::BayW + 44.f + tp.side * 96.f;
        float yb = world::RowBottom(2);
        sb.rect = Rectangle{ x, yb - 56.f, 34.f, 56.f };
        sb.standX = x + 17.f;
        sb.low = false;
        shelf.push_back(sb);
    }
    struct Special { const char* key; const char* title; const char* mark; Color c; int bay; };
    const Special specials[] = {
        { "paper_exam1", "Exam 1 Paper", "I", { 140, 120, 90, 255 }, 0 },
        { "workbook_exam1", "Coding Exam 1 Workbook", "</>", { 40, 70, 110, 255 }, 1 },
        { "paper_exam2", "Exam 2 Paper", "II", { 120, 100, 80, 255 }, 2 },
        { "workbook_exam2", "Coding Exam 2 Workbook", "</>", { 110, 40, 40, 255 }, 3 },
        { "workbook_cpp", "C++ Workbook", "</>", { 40, 90, 70, 255 }, 4 },
        { "paper_all", "Complete Paper", "ALL", { 90, 70, 110, 255 }, 5 },
    };
    for (const Special& s : specials) {
        ShelfBook sb;
        sb.key = s.key; sb.title = s.title; sb.mark = s.mark; sb.color = s.c;
        float x = world::Bay0X + s.bay * world::BayW + 104.f;
        float yb = world::RowBottom(1);
        sb.rect = Rectangle{ x, yb - 52.f, 40.f, 52.f };
        sb.standX = x + 20.f;
        sb.low = true;
        shelf.push_back(sb);
    }

    // filler books (everything that is not fetchable), laid out once
    fillers.clear();
    for (int b = -2; b <= 7; b++) {
        for (int r = 0; r < world::Rows; r++) {
            Rng rng((uint32_t)(5000 + (b + 5) * 131 + r * 17));
            float x0 = world::Bay0X + b * world::BayW + 10.f, x1 = x0 + world::BayW - 22.f;
            float yb = world::RowBottom(r);
            float x = x0;
            while (x < x1 - 7.f) {
                for (const ShelfBook& s : shelf) {
                    if (fabsf(s.rect.y + s.rect.height - yb) > 1.f) continue;
                    if (x + 3.f > s.rect.x - 4.f && x < s.rect.x + s.rect.width + 4.f) x = s.rect.x + s.rect.width + 5.f;
                }
                if (x >= x1 - 7.f) break;
                if (rng.F() < 0.06f) { x += rng.R(9.f, 26.f); continue; }
                float w = rng.R(11.f, 25.f);
                if (x + w > x1) w = x1 - x;
                for (const ShelfBook& s : shelf) {
                    if (fabsf(s.rect.y + s.rect.height - yb) > 1.f) continue;
                    if (x < s.rect.x - 4.f && x + w > s.rect.x - 5.f) w = s.rect.x - 5.f - x;
                }
                if (w < 7.f) { x += w + 1.f; continue; }
                Filler f;
                f.x = x; f.w = w; f.y = yb;
                f.h = rng.R(36.f, r == 0 ? 50.f : 56.f);
                Color c = kBookPalette[rng.I(0, (int)(sizeof(kBookPalette) / sizeof(kBookPalette[0])) - 1)];
                float v = rng.R(0.82f, 1.12f);
                f.c = Shade(c, v);
                f.style = rng.I(0, 5);
                f.tilt = (rng.F() < 0.05f) ? rng.R(-9.f, 9.f) : 0.f;
                fillers.push_back(f);
                x += w + rng.R(0.f, 1.6f);
            }
        }
    }

    motes.clear();
    Rng rm(99);
    for (int i = 0; i < 70; i++) motes.push_back(Mote{ Vector2{ rm.R(150.f, 1500.f), rm.R(120.f, 760.f) }, Vector2{ rm.R(-5.f, 6.f), rm.R(-9.f, -2.f) }, rm.R(0.f, 6.28f), rm.R(1.2f, 2.8f) });

    lightRT = LoadRenderTexture(480, 220);
    SetTextureFilter(lightRT.texture, TEXTURE_FILTER_BILINEAR);
    pileColors.assign(9, Color{ 90, 60, 40, 255 });
    built = true;
}

void Stage::Shutdown() {
    if (built) UnloadRenderTexture(lightRT);
    built = false;
}

int Stage::FindShelfBook(const std::string& key) const {
    for (int i = 0; i < (int)shelf.size(); i++) if (shelf[(size_t)i].key == key) return i;
    return -1;
}

Vector2 Stage::PileSlot(int index) const {
    return Vector2{ world::PileBase.x + ((index % 3) - 1) * 4.f, world::PileBase.y - 18.f * (float)index - 10.f };
}

void Stage::Update(float dt, float time) {
    t = time;
    catPet = std::max(0.f, catPet - dt);
    bellRing = std::max(0.f, bellRing - dt);
    for (Mote& m : motes) {
        m.p.x += (m.v.x + sinf(time * 0.5f + m.phase) * 6.f) * dt;
        m.p.y += m.v.y * dt;
        if (m.p.y < 80.f) { m.p.y = 780.f; m.p.x = 150.f + fmodf(m.phase * 977.f, 1350.f); }
    }
}

bool Stage::HitCat(Vector2 p) const { return CheckCollisionPointRec(p, Rectangle{ world::CatPos.x - 90, world::CatPos.y - 40, 160, 80 }); }
bool Stage::HitBell(Vector2 p) const { return CheckCollisionPointRec(p, Rectangle{ world::BellPos.x - 34, world::BellPos.y - 40, 68, 52 }); }

// ---------------------------------------------------------------------------
// Back layer: wall, shelves, clock, floor, rug
// ---------------------------------------------------------------------------
void Stage::DrawBay(int b) const {
    float x0 = world::Bay0X + b * world::BayW;
    // back panel with a soft vertical falloff
    DrawRectangleGradientV((int)x0, 96, (int)world::BayW, (int)(world::FloorY - 96), Color{ 34, 22, 15, 255 }, Color{ 20, 13, 9, 255 });
    for (int r = 0; r < world::Rows; r++) {
        float yb = world::RowBottom(r);
        // shadow cast by the board above
        DrawRectangleGradientV((int)x0, (int)(yb - 60), (int)world::BayW, 60, WithAlpha(BLACK, 0.28f), WithAlpha(BLACK, 0.0f));
    }
}

void Stage::DrawBack(float time, Vector2 par) const {
    // ---- far layer: the wall and shelves ----
    Vector2 s0 = Shift(0, par);
    rlPushMatrix();
    rlTranslatef(s0.x, s0.y, 0);
    DrawRectangleGradientV(-700, -200, 3000, (int)(world::FloorY + 200), Color{ 58, 40, 30, 255 }, Color{ 40, 28, 20, 255 });
    for (int b = -2; b <= 7; b++) DrawBay(b);
    // filler books
    for (const Filler& f : fillers) {
        Rectangle r{ f.x, f.y - f.h, f.w, f.h };
        if (f.tilt != 0.f) {
            rlPushMatrix();
            rlTranslatef(f.x + f.w * 0.5f, f.y, 0);
            rlRotatef(f.tilt, 0, 0, 1);
            r = Rectangle{ -f.w * 0.5f, -f.h, f.w, f.h };
        }
        DrawRectangleRec(r, f.c);
        DrawRectangleRec(Rectangle{ r.x + r.width * 0.76f, r.y, r.width * 0.24f, r.height }, WithAlpha(BLACK, 0.2f));
        DrawRectangleRec(Rectangle{ r.x, r.y, r.width * 0.1f, r.height }, WithAlpha(WHITE, 0.1f));
        if (f.style == 0 || f.style == 3) { DrawRectangle((int)r.x, (int)(r.y + r.height * 0.15f), (int)r.width, 2, WithAlpha(kBrass, 0.85f)); DrawRectangle((int)r.x, (int)(r.y + r.height * 0.82f), (int)r.width, 2, WithAlpha(kBrass, 0.85f)); }
        else if (f.style == 1) DrawRectangle((int)(r.x + 2), (int)(r.y + r.height * 0.3f), (int)(r.width - 4), (int)(r.height * 0.26f), WithAlpha(Color{ 230, 214, 176, 255 }, 0.8f));
        else if (f.style == 2) DrawRectangle((int)r.x, (int)(r.y + r.height * 0.5f), (int)r.width, 3, WithAlpha(kBrass, 0.7f));
        if (f.tilt != 0.f) rlPopMatrix();
    }
    // fetchable books that are on the shelf
    for (const ShelfBook& sb : shelf) if (sb.present) book::DrawSpine(sb.rect, sb.color, sb.mark, true);
    // shelf boards and dividers
    for (int b = -2; b <= 7; b++) {
        float x0 = world::Bay0X + b * world::BayW;
        for (int r = 0; r <= world::Rows; r++) {
            float yb = (r == world::Rows) ? world::RowBottom(r) : world::RowBottom(r);
            DrawRectangleGradientV((int)x0, (int)yb, (int)world::BayW, 9, Color{ 112, 74, 44, 255 }, Color{ 70, 44, 26, 255 });
            DrawRectangle((int)x0, (int)yb, (int)world::BayW, 1, Color{ 160, 112, 70, 255 });
        }
        DrawRectangleGradientH((int)x0 - 6, 90, 12, (int)(world::FloorY - 90), Color{ 118, 78, 46, 255 }, Color{ 74, 48, 28, 255 });
    }
    // bay name plates and the crown moulding
    DrawRectangleGradientV(-700, 70, 3000, 22, Color{ 96, 62, 36, 255 }, Color{ 60, 38, 22, 255 });
    DrawRectangle(-700, 68, 3000, 3, Color{ 150, 106, 64, 255 });
    DrawRectangleGradientV(-700, 0, 3000, 20, Color{ 80, 52, 30, 255 }, Color{ 50, 32, 18, 255 });
    const char* bayNames[6] = { "C FUNDAMENTALS  I", "C FUNDAMENTALS  II", "POINTERS & MEMORY", "LISTS & OBJECTS", "C++ TECHNIQUES", "STANDARD LIBRARY" };
    for (int i = 0; i < 6; i++) {
        float cx = world::Bay0X + i * world::BayW + world::BayW * 0.5f;
        Rectangle pr{ cx - 100.f, 74.f, 200.f, 17.f };
        art::RRect(pr, 4.f, kBrass);
        DrawRectangleLinesEx(pr, 1.f, kBrassDark);
        ui::TextCentered(FontId::SerifBold, bayNames[i], pr, 11.5f, Color{ 52, 34, 14, 255 });
    }
    DrawClock(time);
    // a library ladder leaning on the right-hand bays
    {
        float lx = 1590.f;
        DrawLineEx(Vector2{ lx, 110 }, Vector2{ lx + 34, 566 }, 8.f, Color{ 120, 80, 48, 255 });
        DrawLineEx(Vector2{ lx + 52, 110 }, Vector2{ lx + 82, 566 }, 8.f, Color{ 100, 66, 38, 255 });
        for (int i = 0; i < 9; i++) { float y = 150.f + i * 48.f; float k = (y - 110.f) / 456.f; DrawLineEx(Vector2{ lx + 34 * k, y }, Vector2{ lx + 52 + 30 * k, y }, 6.f, Color{ 150, 104, 62, 255 }); }
    }
    rlPopMatrix();

    // ---- mid layer: floor and rug ----
    Vector2 s1 = Shift(1, par);
    rlPushMatrix();
    rlTranslatef(s1.x, s1.y, 0);
    DrawRectangleGradientV(-700, 548, 3000, 18, Color{ 74, 48, 28, 255 }, Color{ 52, 34, 20, 255 });     // baseboard
    DrawRectangle(-700, 548, 3000, 2, Color{ 150, 108, 66, 255 });
    DrawRectangleGradientV(-700, (int)world::FloorY, 3000, 520, Color{ 84, 54, 32, 255 }, Color{ 46, 29, 18, 255 });
    // planks converge towards the shelves
    for (int i = -26; i <= 34; i++) {
        float xi = 800.f + i * 105.f;
        float k0 = (world::FloorY + 260.f) / 1160.f, k1 = 1.f;
        Vector2 a{ 800.f + (xi - 800.f) * k0, world::FloorY }, b{ 800.f + (xi - 800.f) * k1, 900.f };
        DrawLineEx(a, b, 1.6f, WithAlpha(BLACK, 0.3f));
    }
    for (int j = 0; j < 7; j++) { float y = world::FloorY + 14.f + j * j * 7.f + j * 12.f; DrawLineEx(Vector2{ -700, y }, Vector2{ 2300, y }, 1.f, WithAlpha(BLACK, 0.12f)); }
    // rug under the desk
    {
        Vector2 q[4] = { { 360, 580 }, { 1240, 580 }, { 1560, 900 }, { 40, 900 } };
        art::Poly(q, 4, Color{ 108, 26, 30, 255 });
        Vector2 q2[4] = { { 392, 592 }, { 1208, 592 }, { 1500, 900 }, { 100, 900 } };
        art::Poly(q2, 4, Color{ 134, 38, 38, 255 });
        DrawLineEx(Vector2{ 392, 592 }, Vector2{ 1208, 592 }, 3.f, WithAlpha(kBrass, 0.8f));
        DrawLineEx(Vector2{ 392, 592 }, Vector2{ 100, 900 }, 3.f, WithAlpha(kBrass, 0.8f));
        DrawLineEx(Vector2{ 1208, 592 }, Vector2{ 1500, 900 }, 3.f, WithAlpha(kBrass, 0.8f));
        for (int i = 0; i < 9; i++) { float k = (i + 0.5f) / 9.f; Vector2 c{ Lerp(430.f, 1170.f, k), 614.f }; DrawPoly(c, 4, 7.f, 45.f, WithAlpha(kBrass, 0.55f)); }
    }
    rlPopMatrix();
}

void Stage::DrawClock(float time) const {
    (void)time;
    Vector2 c{ 800.f, 40.f };
    DrawCircleV(Vector2{ c.x + 3, c.y + 4 }, 36.f, WithAlpha(BLACK, 0.35f));
    DrawCircleV(c, 36.f, kBrassDark);
    DrawCircleV(c, 33.f, kBrass);
    DrawCircleV(c, 29.f, Color{ 238, 226, 196, 255 });
    for (int i = 0; i < 12; i++) {
        float a = i * PI / 6.f;
        Vector2 p0{ c.x + sinf(a) * 24.f, c.y - cosf(a) * 24.f }, p1{ c.x + sinf(a) * 28.f, c.y - cosf(a) * 28.f };
        DrawLineEx(p0, p1, i % 3 == 0 ? 2.5f : 1.2f, Color{ 60, 40, 24, 255 });
    }
    time_t now = ::time(nullptr);
    struct tm lt;
#ifdef _WIN32
    localtime_s(&lt, &now);
#else
    lt = *localtime(&now);
#endif
    float hm = (lt.tm_hour % 12 + lt.tm_min / 60.f) / 12.f * 2.f * PI, mm = (lt.tm_min + lt.tm_sec / 60.f) / 60.f * 2.f * PI;
    DrawLineEx(c, Vector2{ c.x + sinf(hm) * 15.f, c.y - cosf(hm) * 15.f }, 3.5f, Color{ 40, 28, 18, 255 });
    DrawLineEx(c, Vector2{ c.x + sinf(mm) * 23.f, c.y - cosf(mm) * 23.f }, 2.2f, Color{ 40, 28, 18, 255 });
    DrawCircleV(c, 3.f, kBrassDark);
}

// ---------------------------------------------------------------------------
// Chair
// ---------------------------------------------------------------------------
void Stage::DrawChair(Vector2 par) const {
    Vector2 s = Shift(1, par);
    float push = Smooth(chairBack);
    rlPushMatrix();
    rlTranslatef(s.x + 120.f * push, s.y + 14.f * push, 0);
    rlTranslatef(800, 690, 0);
    rlRotatef(-5.f * push, 0, 0, 1);
    rlTranslatef(-800, -690, 0);
    art::Shadow(Vector2{ 800, 700 }, 150.f, 26.f, 0.4f);
    // wooden frame and tufted red back
    art::RRect(Rectangle{ 686, 322, 228, 372 }, 40.f, Color{ 74, 44, 26, 255 });
    art::RRect(Rectangle{ 698, 334, 204, 346 }, 32.f, Color{ 130, 30, 34, 255 });
    art::RRect(Rectangle{ 712, 348, 176, 318 }, 26.f, Color{ 150, 38, 40, 255 });
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 3; col++) {
            Vector2 p{ 744.f + col * 56.f + (row % 2) * 28.f - 14.f, 384.f + row * 56.f };
            if (p.x < 722 || p.x > 878) continue;
            art::Ell(p, 7, 7, Color{ 202, 160, 74, 255 });
            art::Ell(Vector2{ p.x - 1, p.y - 1 }, 3, 3, Color{ 244, 220, 150, 255 });
            DrawLineEx(Vector2{ p.x - 28, p.y }, Vector2{ p.x - 7, p.y }, 1.5f, WithAlpha(BLACK, 0.2f));
        }
    }
    // arm rests
    art::RRect(Rectangle{ 650, 600, 64, 26 }, 12.f, Color{ 88, 54, 32, 255 });
    art::RRect(Rectangle{ 886, 600, 64, 26 }, 12.f, Color{ 88, 54, 32, 255 });
    rlPopMatrix();
}

// ---------------------------------------------------------------------------
// Desk and everything standing on it
// ---------------------------------------------------------------------------
static void GradQuad(Vector2 tl, Vector2 tr, Vector2 br, Vector2 bl, Color top, Color bottom) {
    rlSetTexture(0);
    rlBegin(RL_QUADS);
    rlColor4ub(top.r, top.g, top.b, top.a); rlVertex2f(tl.x, tl.y);
    rlColor4ub(bottom.r, bottom.g, bottom.b, bottom.a); rlVertex2f(bl.x, bl.y);
    rlColor4ub(bottom.r, bottom.g, bottom.b, bottom.a); rlVertex2f(br.x, br.y);
    rlColor4ub(top.r, top.g, top.b, top.a); rlVertex2f(tr.x, tr.y);
    rlEnd();
}

void Stage::DrawDesk(float time, Vector2 par) const {
    Vector2 s = Shift(2, par);
    rlPushMatrix();
    rlTranslatef(s.x, s.y, 0);
    const float yB = 1100.f;
    // wooden sides, then the leather top
    art::Quad(Vector2{ 340, 664 }, Vector2{ 1260, 664 }, Vector2{ 1260 + (yB - 664) * 1.05f, yB }, Vector2{ 340 - (yB - 664) * 1.05f, yB }, Color{ 72, 44, 26, 255 });
    GradQuad(Vector2{ 340, 680 }, Vector2{ 1260, 680 }, Vector2{ 1260 + (yB - 680) * 1.05f - 18.f, yB }, Vector2{ 340 - (yB - 680) * 1.05f + 18.f, yB }, Color{ 26, 62, 46, 255 }, Color{ 40, 88, 64, 255 });
    // desk mat and tooled border
    {
        Vector2 mat[4] = { { 548, 704 }, { 1052, 704 }, { 1200, 960 }, { 400, 960 } };
        art::Poly(mat, 4, Color{ 34, 78, 56, 255 });
        for (int i = 0; i < 4; i++) DrawLineEx(mat[i], mat[(i + 1) % 4], 2.f, WithAlpha(kBrass, 0.7f));
        DrawLineEx(Vector2{ 380, 698 }, Vector2{ 1220, 698 }, 1.5f, WithAlpha(kBrass, 0.45f));
        DrawLineEx(Vector2{ 380, 698 }, Vector2{ -4, 1100 }, 1.5f, WithAlpha(kBrass, 0.35f));
        DrawLineEx(Vector2{ 1220, 698 }, Vector2{ 1604, 1100 }, 1.5f, WithAlpha(kBrass, 0.35f));
    }
    // back lip of the desk
    GradQuad(Vector2{ 330, 662 }, Vector2{ 1270, 662 }, Vector2{ 1270, 682 }, Vector2{ 330, 682 }, Color{ 128, 86, 52, 255 }, Color{ 80, 50, 30, 255 });
    DrawRectangle(330, 662, 940, 2, Color{ 190, 140, 90, 255 });
    DrawRectangle(330, 681, 940, 2, WithAlpha(BLACK, 0.4f));
    // props, back to front
    DrawLamp(time);
    DrawInk();
    DrawCat(time);
    DrawPile();
    DrawBell(time);
    rlPopMatrix();
}

void Stage::DrawLamp(float time) const {
    (void)time;
    Vector2 b = world::LampPos;
    art::Shadow(Vector2{ b.x + 6, b.y + 8 }, 78.f, 16.f, 0.5f);
    art::EllShaded(Vector2{ b.x, b.y }, 46.f, 13.f, kBrass);
    DrawRectangleGradientH((int)b.x - 5, (int)b.y - 176, 10, 176, kBrassDark, Color{ 232, 196, 108, 255 });
    art::Cap(Vector2{ b.x, b.y - 170 }, Vector2{ b.x + 4, b.y - 184 }, 12.f, kBrass);
    // green glass shade with a glowing underside
    Vector2 sh[4] = { { b.x - 34, b.y - 214 }, { b.x + 34, b.y - 214 }, { b.x + 68, b.y - 160 }, { b.x - 68, b.y - 160 } };
    art::Poly(sh, 4, Color{ 22, 96, 62, 255 });
    Vector2 sh2[4] = { { b.x - 26, b.y - 208 }, { b.x + 14, b.y - 208 }, { b.x + 40, b.y - 164 }, { b.x - 52, b.y - 164 } };
    art::Poly(sh2, 4, Color{ 40, 128, 84, 255 });
    DrawLineEx(sh[0], sh[3], 2.f, Color{ 14, 60, 38, 255 });
    DrawLineEx(sh[1], sh[2], 2.f, Color{ 14, 60, 38, 255 });
    art::Ell(Vector2{ b.x, b.y - 160 }, 68.f, 9.f, Color{ 255, 226, 150, 255 });
    art::Ell(Vector2{ b.x, b.y - 160 }, 46.f, 5.f, Color{ 255, 246, 210, 255 });
    art::Ell(Vector2{ b.x, b.y - 218 }, 34.f, 5.f, Color{ 30, 118, 76, 255 });
}

void Stage::DrawInk() const {
    Vector2 b = world::InkPos;
    art::Shadow(Vector2{ b.x + 4, b.y + 6 }, 34.f, 9.f, 0.45f);
    // quill first (it leans out of the pot)
    DrawLineEx(Vector2{ b.x + 4, b.y - 22 }, Vector2{ b.x + 44, b.y - 104 }, 3.f, Color{ 236, 230, 214, 255 });
    art::Ell(Vector2{ b.x + 30, b.y - 78 }, 7.f, 30.f, Color{ 244, 240, 228, 255 }, 26.f);
    art::Ell(Vector2{ b.x + 36, b.y - 86 }, 3.f, 22.f, Color{ 214, 206, 188, 255 }, 26.f);
    art::RRect(Rectangle{ b.x - 17, b.y - 24, 34, 28 }, 8.f, Color{ 28, 32, 52, 255 });
    art::Ell(Vector2{ b.x, b.y - 24 }, 17.f, 6.f, Color{ 70, 80, 120, 255 });
    art::Ell(Vector2{ b.x, b.y - 24 }, 9.f, 3.f, Color{ 14, 16, 30, 255 });
    DrawRectangle((int)b.x - 12, (int)b.y - 18, 3, 16, WithAlpha(WHITE, 0.25f));
}

void Stage::DrawBell(float time) const {
    Vector2 b = world::BellPos;
    float wob = bellRing > 0 ? sinf(time * 38.f) * 0.2f * Clamp01(bellRing) : 0.f;
    art::Shadow(Vector2{ b.x + 3, b.y + 6 }, 40.f, 9.f, 0.5f);
    art::Ell(Vector2{ b.x, b.y }, 34.f, 8.f, kBrassDark);
    rlPushMatrix();
    rlTranslatef(b.x, b.y - 2, 0);
    rlRotatef(wob * RAD2DEG, 0, 0, 1);
    DrawCircleSector(Vector2{ 0, 0 }, 28.f, 270.f - 90.f, 270.f + 90.f, 24, Color{ 218, 176, 82, 255 });
    DrawCircleSector(Vector2{ -6, -2 }, 18.f, 270.f - 90.f, 270.f + 90.f, 18, WithAlpha(Color{ 255, 236, 170, 255 }, 0.55f));
    art::Ell(Vector2{ 0, 0 }, 28.f, 5.f, kBrassDark);
    art::Cap(Vector2{ 0, -28 }, Vector2{ 0, -33 }, 9.f, kBrass);
    rlPopMatrix();
}

void Stage::DrawPile() const {
    for (int i = 0; i < std::min(pileCount, 9); i++) {
        Vector2 c = PileSlot(i);
        float w = 150.f - (i % 3) * 6.f, h = 18.f;
        Color col = pileColors[(size_t)i % pileColors.size()];
        art::Shadow(Vector2{ c.x + 4, c.y + h * 0.7f }, w * 0.6f, 8.f, i == 0 ? 0.5f : 0.2f);
        Rectangle r{ c.x - w * 0.5f, c.y - h * 0.5f, w, h };
        art::RRect(r, 3.f, col);
        DrawRectangle((int)r.x + 6, (int)(r.y + h * 0.7f), (int)w - 12, 4, Color{ 228, 212, 176, 255 });   // page edge
        DrawRectangle((int)r.x, (int)r.y, (int)w, 2, WithAlpha(WHITE, 0.2f));
        DrawRectangle((int)r.x + 4, (int)(r.y + 4), 5, (int)(h - 8), WithAlpha(kBrass, 0.9f));
    }
}

void Stage::DrawCat(float time) const {
    Vector2 c = world::CatPos;
    bool awake = catPet > 0.f;
    float breathe = sinf(time * 1.9f) * 1.6f * (awake ? 0.5f : 1.f);
    Color fur{ 34, 33, 40, 255 }, rim{ 74, 72, 86, 255 };
    art::Shadow(Vector2{ c.x + 6, c.y + 22 }, 92.f, 14.f, 0.55f);
    // tail curls around the front
    Vector2 t0{ c.x - 62, c.y + 10 }, t1{ c.x - 20, c.y + 44 + (awake ? sinf(time * 7.f) * 8.f : 0.f) }, t2{ c.x + 48, c.y + 22 };
    DrawSplineSegmentBezierQuadratic(t0, t1, t2, 13.f, fur);
    art::Cap(t2, t2, 13.f, fur);
    // body
    art::Ell(Vector2{ c.x + 6, c.y + 2 - breathe * 0.5f }, 66.f, 30.f + breathe * 0.6f, fur);
    art::Ell(Vector2{ c.x + 4, c.y - 6 - breathe * 0.5f }, 52.f, 16.f, WithAlpha(rim, 0.55f));
    // head
    float lift = awake ? 14.f : 0.f;
    Vector2 h{ c.x - 56, c.y - 2 - lift };
    art::Ell(h, 25.f, 22.f, fur);
    Vector2 e1[3] = { { h.x - 22, h.y - 8 }, { h.x - 14, h.y - 34 }, { h.x - 2, h.y - 16 } };
    Vector2 e2[3] = { { h.x + 22, h.y - 8 }, { h.x + 14, h.y - 34 }, { h.x + 2, h.y - 16 } };
    art::Poly(e1, 3, fur); art::Poly(e2, 3, fur);
    Vector2 i1[3] = { { h.x - 19, h.y - 12 }, { h.x - 14, h.y - 27 }, { h.x - 7, h.y - 16 } };
    Vector2 i2[3] = { { h.x + 19, h.y - 12 }, { h.x + 14, h.y - 27 }, { h.x + 7, h.y - 16 } };
    art::Poly(i1, 3, Color{ 120, 70, 82, 255 }); art::Poly(i2, 3, Color{ 120, 70, 82, 255 });
    if (awake) {
        for (int s = -1; s <= 1; s += 2) { art::Ell(Vector2{ h.x + s * 10.f, h.y - 2 }, 6.f, 7.f, Color{ 210, 220, 90, 255 }); art::Ell(Vector2{ h.x + s * 10.f, h.y - 2 }, 1.8f, 6.f, BLACK); }
    } else {
        for (int s = -1; s <= 1; s += 2) DrawSplineSegmentBezierQuadratic(Vector2{ h.x + s * 15.f, h.y - 3 }, Vector2{ h.x + s * 10.f, h.y + 3 }, Vector2{ h.x + s * 5.f, h.y - 3 }, 2.f, Color{ 150, 150, 170, 255 });
    }
    art::Ell(Vector2{ h.x, h.y + 6 }, 3.5f, 2.6f, Color{ 190, 120, 130, 255 });
    // sleeping letters
    if (!awake) {
        for (int i = 0; i < 3; i++) {
            float k = fmodf(time * 0.35f + i / 3.f, 1.f);
            ui::Text(FontId::SerifItalic, "z", h.x - 18 - k * 20.f, h.y - 40 - k * 60.f, 14.f + i * 5.f, WithAlpha(pal::GoldSoft, (1.f - k) * 0.75f));
        }
    }
}

// ---------------------------------------------------------------------------
// Lighting and atmosphere
// ---------------------------------------------------------------------------
void Stage::BuildLightmap(float time, Vector2 headPos, float headLight) {
    if (!built) return;
    float flick = 1.f + sinf(time * 7.3f) * 0.012f + sinf(time * 3.1f) * 0.02f;
    BeginTextureMode(lightRT);
    ClearBackground(Color{ 98, 84, 78, 255 });
    BeginBlendMode(BLEND_ADDITIVE);
    rlPushMatrix();
    rlScalef(480.f / kLightWorld.width, 220.f / kLightWorld.height, 1.f);
    rlTranslatef(-kLightWorld.x, -kLightWorld.y, 0);
    art::Glow(Vector2{ 800, 150 }, 1100.f, 420.f, Color{ 162, 130, 94, 255 });                    // light from above
    art::Glow(Vector2{ 120, 380 }, 560.f, 460.f, Color{ 150, 112, 76, 255 });                      // wall sconces left and right
    art::Glow(Vector2{ 1480, 380 }, 560.f, 460.f, Color{ 150, 112, 76, 255 });
    art::Glow(Vector2{ 800, 470 }, 760.f, 380.f, Color{ 92, 72, 52, 255 });
    art::Glow(lampLight, 560.f * flick, 480.f * flick, Color{ 255, 206, 130, 255 });                // desk lamp
    art::Glow(Vector2{ lampLight.x + 20, 760 }, 420.f, 150.f, Color{ 255, 190, 110, 150 });
    art::Glow(headPos, 400.f, 440.f, Color{ 210, 168, 116, (unsigned char)(255 * Clamp01(headLight)) });   // keeps the librarian readable
    art::Glow(Vector2{ 800, 780 }, 560.f, 200.f, Color{ 110, 90, 64, 255 });
    rlPopMatrix();
    EndBlendMode();
    EndTextureMode();
}

void Stage::DrawAtmosphere(float time) const {
    // drifting dust, catching the lamp light
    BeginBlendMode(BLEND_ADDITIVE);
    for (const Mote& m : motes) {
        float tw = 0.5f + 0.5f * sinf(time * 1.6f + m.phase * 3.f);
        float lamp = Clamp01(1.f - Vector2Distance(m.p, lampLight) / 700.f);
        float a = (0.15f + 0.6f * lamp) * tw;
        art::Glow(m.p, m.size * 3.f, m.size * 3.f, Color{ 255, 226, 170, (unsigned char)(a * 255.f) });
    }
    EndBlendMode();
    // light map multiplied over the whole set
    BeginBlendMode(BLEND_MULTIPLIED);
    DrawTexturePro(lightRT.texture, Rectangle{ 0, 0, (float)lightRT.texture.width, -(float)lightRT.texture.height }, kLightWorld, Vector2{ 0, 0 }, 0, WHITE);
    EndBlendMode();
}
