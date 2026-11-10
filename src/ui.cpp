// ui.cpp - fonts, text layout and immediate-mode widgets
#include "ui.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace ui {

// ---------------------------------------------------------------------------
// Fonts: each family is rasterised at several pixel sizes; drawing picks the
// atlas closest to the on-screen pixel size, while layout always uses the
// metrics of the largest atlas so wrapping never depends on zoom level.
// ---------------------------------------------------------------------------
static const int kSizes[] = { 16, 24, 34, 50, 76 };
static const int kNumSizes = 5;

struct Family {
    Font f[kNumSizes];
    bool ok = false;
    bool isDefault = false;
};
static Family g_fam[(int)FontId::Count];
static std::vector<int> g_codepoints;
static int g_cpIndex[0x2800];   // codepoint -> glyph index (same for all atlases)

static void BuildCodepoints() {
    g_codepoints.clear();
    for (int c = 32; c <= 126; c++) g_codepoints.push_back(c);
    for (int c = 0xA0; c <= 0xFF; c++) g_codepoints.push_back(c);
    const int extra[] = { 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026,
                          0x2190, 0x2191, 0x2192, 0x2193, 0x2260, 0x2264, 0x2265, 0x221A, 0x2122 };
    for (int c : extra) g_codepoints.push_back(c);
    for (int i = 0; i < 0x2800; i++) g_cpIndex[i] = -1;
    for (int i = 0; i < (int)g_codepoints.size(); i++) g_cpIndex[g_codepoints[i]] = i;
}

static std::string FindFont(std::initializer_list<const char*> names) {
    const char* windir = getenv("WINDIR");
    std::string sys = std::string(windir ? windir : "C:\\Windows") + "\\Fonts\\";
    for (const char* n : names) {
        std::string a = util::AssetPath(std::string("fonts/") + n);
        if (util::FileExistsAt(a)) return a;
        std::string b = sys + n;
        if (util::FileExistsAt(b)) return b;
    }
    return std::string();
}

void LoadFonts() {
    BuildCodepoints();
    struct Spec { FontId id; std::initializer_list<const char*> files; };
    const Spec specs[] = {
        { FontId::Serif,       { "georgia.ttf", "times.ttf", "arial.ttf" } },
        { FontId::SerifBold,   { "georgiab.ttf", "timesbd.ttf", "arialbd.ttf" } },
        { FontId::SerifItalic, { "georgiai.ttf", "timesi.ttf", "ariali.ttf" } },
        { FontId::Mono,        { "consola.ttf", "cour.ttf", "lucon.ttf" } },
        { FontId::MonoBold,    { "consolab.ttf", "courbd.ttf", "consola.ttf" } },
        { FontId::Hand,        { "Inkfree.ttf", "segoepr.ttf", "georgiai.ttf" } },
    };
    for (const Spec& s : specs) {
        Family& fam = g_fam[(int)s.id];
        std::string path = FindFont(s.files);
        if (path.empty()) {
            for (int i = 0; i < kNumSizes; i++) fam.f[i] = GetFontDefault();
            fam.isDefault = true; fam.ok = true;
            TraceLog(LOG_WARNING, "UI: no TTF found for font %d, using default", (int)s.id);
            continue;
        }
        for (int i = 0; i < kNumSizes; i++) {
            fam.f[i] = LoadFontEx(path.c_str(), kSizes[i], g_codepoints.data(), (int)g_codepoints.size());
            SetTextureFilter(fam.f[i].texture, TEXTURE_FILTER_BILINEAR);
        }
        fam.ok = true;
    }
}

void UnloadFonts() {
    for (auto& fam : g_fam) {
        if (!fam.ok || fam.isDefault) continue;
        for (int i = 0; i < kNumSizes; i++) UnloadFont(fam.f[i]);
    }
}

// ---------------------------------------------------------------------------
// Pass / input state
// ---------------------------------------------------------------------------
static Vector2 g_off{ 0, 0 };
static float g_scale = 1.f;
static bool g_interactive = false;
static bool g_inPass = false;
static bool g_pressConsumed = false;
static bool g_wheelConsumed = false;
static bool g_wantHand = false;
static bool g_handShown = false;
static std::vector<Rectangle> g_clip;

static bool g_kbCaptured = false;
static bool g_kbCapturedPrev = false;
void NewFrame() { g_pressConsumed = false; g_wheelConsumed = false; g_wantHand = false; g_kbCapturedPrev = g_kbCaptured; g_kbCaptured = false; }
void CaptureKeyboard() { g_kbCaptured = true; }
bool KeyboardCaptured() { return g_kbCaptured || g_kbCapturedPrev; }

void EndFrameCursor() {
    if (g_wantHand != g_handShown) {
        SetMouseCursor(g_wantHand ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
        g_handShown = g_wantHand;
    }
}

void Begin(Vector2 offset, float scale, bool interactive) {
    g_off = offset; g_scale = scale; g_interactive = interactive; g_inPass = true;
    Camera2D cam{};
    cam.offset = offset; cam.target = Vector2{ 0, 0 }; cam.rotation = 0; cam.zoom = scale;
    BeginMode2D(cam);
}

void End() {
    while (!g_clip.empty()) PopClip();
    EndMode2D();
    g_inPass = false;
}

float Scale() { return g_scale; }
Vector2 Offset() { return g_off; }
Vector2 ToScreen(Vector2 v) { return Vector2{ g_off.x + v.x * g_scale, g_off.y + v.y * g_scale }; }

Vector2 Mouse() {
    Vector2 m = GetMousePosition();
    return Vector2{ (m.x - g_off.x) / g_scale, (m.y - g_off.y) / g_scale };
}
bool Interactive() { return g_interactive; }
bool Hover(Rectangle r) {
    if (!g_interactive) return false;
    if (!g_clip.empty()) {
        // respect clipping: a hidden part of a scrolled list is not hoverable
        Vector2 sm = GetMousePosition();
        Rectangle c = g_clip.back();
        if (!CheckCollisionPointRec(sm, c)) return false;
    }
    return CheckCollisionPointRec(Mouse(), r);
}
bool Pressed(Rectangle r) {
    if (!Hover(r)) return false;
    WantHand();
    if (g_pressConsumed || !IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return false;
    g_pressConsumed = true;
    return true;
}
bool AnyPress() { return g_interactive && !g_pressConsumed && IsMouseButtonPressed(MOUSE_BUTTON_LEFT); }
bool MouseDown() { return g_interactive && IsMouseButtonDown(MOUSE_BUTTON_LEFT); }
float Wheel() { return (g_interactive && !g_wheelConsumed) ? GetMouseWheelMove() : 0.f; }
void ConsumePress() { g_pressConsumed = true; }
void ConsumeWheel() { g_wheelConsumed = true; }
void WantHand() { g_wantHand = true; }

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------
static inline int GlyphIndex(int cp) {
    if (cp == '\t') cp = ' ';
    if (cp >= 0 && cp < 0x2800 && g_cpIndex[cp] >= 0) return g_cpIndex[cp];
    return g_cpIndex['?'];
}

static const Font& PickFont(FontId f, float size) {
    const Family& fam = g_fam[(int)f];
    float px = size * g_scale;
    for (int i = 0; i < kNumSizes; i++) if (kSizes[i] >= px * 0.92f) return fam.f[i];
    return fam.f[kNumSizes - 1];
}
static const Font& RefFont(FontId f) { return g_fam[(int)f].f[kNumSizes - 1]; }

static inline float Advance(const Font& ref, int idx, float size) {
    if (g_fam[0].isDefault || idx < 0 || idx >= ref.glyphCount) return size * 0.5f;
    float sc = size / (float)ref.baseSize;
    const GlyphInfo& g = ref.glyphs[idx];
    return (g.advanceX == 0 ? ref.recs[idx].width : (float)g.advanceX) * sc;
}

float MeasureWidth(FontId f, const std::string& s, float size, float spacing) {
    const Font& ref = RefFont(f);
    if (g_fam[(int)f].isDefault) return MeasureTextEx(ref, s.c_str(), size, spacing).x;
    float w = 0, best = 0;
    const char* p = s.c_str();
    while (*p) {
        int n = 0;
        int cp = GetCodepointNext(p, &n);
        p += n;
        if (cp == '\n') { best = std::max(best, w); w = 0; continue; }
        float adv = Advance(ref, GlyphIndex(cp), size);
        if (cp == '\t') adv *= 4;
        w += adv + spacing;
    }
    return std::max(best, w);
}

std::string Ellipsize(FontId f, const std::string& s, float size, float maxW) {
    if (MeasureWidth(f, s, size) <= maxW) return s;
    std::string t = s;
    while (!t.empty()) {
        // drop one UTF-8 code point from the end
        size_t cut = t.size() - 1;
        while (cut > 0 && ((unsigned char)t[cut] & 0xC0) == 0x80) cut--;
        t.erase(cut);
        while (!t.empty() && (t.back() == ' ' || t.back() == ',')) t.pop_back();
        if (MeasureWidth(f, t + "...", size) <= maxW) return t + "...";
    }
    return "";
}

Vector2 Measure(FontId f, const std::string& s, float size, float spacing) {
    int lines = 1;
    for (char c : s) if (c == '\n') lines++;
    return Vector2{ MeasureWidth(f, s, size, spacing), size * lines * 1.2f };
}

float MonoCharWidth(float size) { return Advance(RefFont(FontId::Mono), GlyphIndex('M'), size); }

void Text(FontId f, const std::string& s, float x, float y, float size, Color c, float spacing) {
    if (s.empty()) return;
    const Family& fam = g_fam[(int)f];
    if (fam.isDefault) { DrawTextEx(fam.f[0], s.c_str(), Vector2{ x, y }, size, spacing + 1, c); return; }
    const Font& df = PickFont(f, size);
    const Font& ref = RefFont(f);
    float sc = size / (float)df.baseSize;
    float cx = x, cy = y;
    const char* p = s.c_str();
    while (*p) {
        int n = 0;
        int cp = GetCodepointNext(p, &n);
        p += n;
        if (cp == '\n') { cx = x; cy += size * 1.2f; continue; }
        int idx = GlyphIndex(cp);
        if (idx < 0) continue;
        if (cp != ' ' && cp != '\t') {
            const Rectangle& rc = df.recs[idx];
            const GlyphInfo& g = df.glyphs[idx];
            float pad = (float)df.glyphPadding;
            Rectangle src{ rc.x - pad, rc.y - pad, rc.width + 2 * pad, rc.height + 2 * pad };
            Rectangle dst{ cx + (g.offsetX - pad) * sc, cy + (g.offsetY - pad) * sc, (rc.width + 2 * pad) * sc, (rc.height + 2 * pad) * sc };
            DrawTexturePro(df.texture, src, dst, Vector2{ 0, 0 }, 0.f, c);
        }
        float adv = Advance(ref, idx, size);
        if (cp == '\t') adv *= 4;
        cx += adv + spacing;
    }
}

void TextCentered(FontId f, const std::string& s, Rectangle r, float size, Color c) {
    float w = MeasureWidth(f, s, size);
    Text(f, s, r.x + (r.width - w) * 0.5f, r.y + (r.height - size * 1.12f) * 0.5f, size, c);
}

void TextRight(FontId f, const std::string& s, float rightX, float y, float size, Color c) {
    Text(f, s, rightX - MeasureWidth(f, s, size), y, size, c);
}

float Wrap(FontId f, const std::string& s, float x, float y, float width, float size, Color c, float lineH, bool draw) {
    float lh = size * lineH;
    float cy = y;
    float spaceW = MeasureWidth(f, " ", size);
    std::vector<std::string> paras = util::Split(s, '\n');
    for (const std::string& para : paras) {
        std::vector<std::string> words = util::Split(para, ' ');
        std::string line;
        float lineW = 0;
        for (const std::string& w : words) {
            float ww = MeasureWidth(f, w, size);
            if (!line.empty() && lineW + spaceW + ww > width) {
                if (draw) Text(f, line, x, cy, size, c);
                cy += lh; line = w; lineW = ww;
            } else {
                if (!line.empty()) { line += ' '; lineW += spaceW; }
                line += w; lineW += ww;
            }
        }
        if (draw) Text(f, line, x, cy, size, c);
        cy += lh;
    }
    return cy - y;
}

// ----- rich text -----
struct RTok { std::string text; int style; bool spaceAfter; bool newline; };
enum { ST_BOLD = 1, ST_ITALIC = 2, ST_CODE = 4 };

static void Tokenize(const std::string& s, std::vector<RTok>& out) {
    int style = 0;
    std::string cur;
    auto flush = [&](bool space) {
        if (!cur.empty()) { out.push_back(RTok{ cur, style, space, false }); cur.clear(); }
        else if (space && !out.empty()) out.back().spaceAfter = true;
    };
    for (size_t i = 0; i < s.size(); i++) {
        char ch = s[i];
        if (ch == '`') { flush(false); style ^= ST_CODE; continue; }
        if (!(style & ST_CODE)) {
            if (ch == '*' && i + 1 < s.size() && s[i + 1] == '*') { flush(false); style ^= ST_BOLD; i++; continue; }
            if (ch == '_' && i + 1 < s.size() && s[i + 1] == '_') {
                size_t run = i;
                while (run < s.size() && s[run] == '_') run++;
                if (run - i >= 3) { cur.append(s, i, run - i); i = run - 1; continue; }   // "____" is a blank, not italics
                flush(false); style ^= ST_ITALIC; i++; continue;
            }
        }
        if (ch == '\n') { flush(false); out.push_back(RTok{ "", style, false, true }); continue; }
        if (ch == ' ') { flush(true); continue; }
        cur += ch;
    }
    flush(false);
}

static FontId StyleFont(int st) {
    if (st & ST_CODE) return (st & ST_BOLD) ? FontId::MonoBold : FontId::Mono;
    if (st & ST_BOLD) return FontId::SerifBold;
    if (st & ST_ITALIC) return FontId::SerifItalic;
    return FontId::Serif;
}

float Rich(const std::string& s, float x, float y, float width, float size, Color c, float lineH, bool draw) {
    std::vector<RTok> toks;
    Tokenize(s, toks);
    float lh = size * lineH;
    float cx = 0, cy = y;
    float spaceW = MeasureWidth(FontId::Serif, " ", size);
    const float codeScale = 0.9f;
    Color codeBg = util::WithAlpha(pal::Ink, 0.08f);
    Color codeInk = util::Mix(c, Color{ 120, 30, 30, 255 }, 0.55f);
    bool chained = false;   // the previous code chip runs on into this one
    for (size_t i = 0; i < toks.size(); i++) {
        const RTok& t = toks[i];
        if (t.newline) { cx = 0; cy += lh; chained = false; continue; }
        bool code = (t.style & ST_CODE) != 0;
        float sz = code ? size * codeScale : size;
        FontId fid = StyleFont(t.style);
        float w = MeasureWidth(fid, t.text, sz);
        if (cx > 0 && cx + w > width) { cx = 0; cy += lh; chained = false; }
        bool extend = code && t.spaceAfter && i + 1 < toks.size() && (toks[i + 1].style & ST_CODE) && !toks[i + 1].newline;
        if (draw) {
            float ty = cy + (code ? size * 0.07f : 0.f);
            if (code) {
                // multi-word code spans become one continuous chip (segments butt together, no overlap)
                float left = x + cx - (chained ? 0.f : 3.f);
                float right = extend ? x + cx + w + spaceW : x + cx + w + 3;
                Rectangle chip{ left, cy + size * 0.02f, right - left, size * 1.18f };
                if (chained || extend) DrawRectangleRec(chip, codeBg);
                else DrawRectangleRounded(chip, 0.3f, 4, codeBg);
                Text(fid, t.text, x + cx, ty, sz, codeInk);
            } else {
                Text(fid, t.text, x + cx, ty, sz, c);
            }
        }
        cx += w;
        if (t.spaceAfter) cx += spaceW;
        chained = extend;
    }
    return (cy - y) + lh;
}

// ---------------------------------------------------------------------------
// Clipping
// ---------------------------------------------------------------------------
void PushClip(Rectangle r) {
    Rectangle s{ g_off.x + r.x * g_scale, g_off.y + r.y * g_scale, r.width * g_scale, r.height * g_scale };
    if (!g_clip.empty()) {
        Rectangle p = g_clip.back();
        float x0 = std::max(s.x, p.x), y0 = std::max(s.y, p.y);
        float x1 = std::min(s.x + s.width, p.x + p.width), y1 = std::min(s.y + s.height, p.y + p.height);
        s = Rectangle{ x0, y0, std::max(0.f, x1 - x0), std::max(0.f, y1 - y0) };
    }
    g_clip.push_back(s);
    BeginScissorMode((int)floorf(s.x), (int)floorf(s.y), (int)ceilf(s.width), (int)ceilf(s.height));
}

void PopClip() {
    if (g_clip.empty()) return;
    g_clip.pop_back();
    if (g_clip.empty()) EndScissorMode();
    else {
        Rectangle s = g_clip.back();
        BeginScissorMode((int)floorf(s.x), (int)floorf(s.y), (int)ceilf(s.width), (int)ceilf(s.height));
    }
}

// ---------------------------------------------------------------------------
// Widgets
// ---------------------------------------------------------------------------
void Panel(Rectangle r, Color fill, Color border, float roundness, float borderW) {
    DrawRectangleRounded(r, roundness, 8, fill);
    if (borderW > 0) DrawRectangleRoundedLinesEx(r, roundness, 8, borderW, border);
}

bool Button(Rectangle r, const std::string& label, ButtonStyle style, bool enabled, float fontSize) {
    bool hov = enabled && Hover(r);
    bool hit = enabled && Pressed(r);
    bool down = hov && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    float fs = fontSize > 0 ? fontSize : r.height * 0.44f;
    Color fill, border, ink;
    switch (style) {
        case BtnPrimary: fill = pal::Green; border = util::Shade(pal::Green, 0.6f); ink = pal::GoldSoft; break;
        case BtnDanger:  fill = pal::Red; border = util::Shade(pal::Red, 0.6f); ink = Color{ 255, 236, 220, 255 }; break;
        case BtnWood:    fill = pal::Wood; border = pal::Gold; ink = pal::GoldSoft; break;
        case BtnGhost:   fill = util::WithAlpha(pal::Ink, hov ? 0.10f : 0.0f); border = util::WithAlpha(pal::Ink, 0.35f); ink = pal::Ink; break;
        case BtnLink: {
            Color lc = enabled ? (hov ? pal::Red : pal::Blue) : pal::InkFaint;
            float w = MeasureWidth(FontId::SerifItalic, label, fs);
            float tx = r.x + (r.width - w) * 0.5f, ty = r.y + (r.height - fs * 1.12f) * 0.5f;
            Text(FontId::SerifItalic, label, tx, ty, fs, lc);
            if (hov) DrawLineEx(Vector2{ tx, ty + fs * 1.1f }, Vector2{ tx + w, ty + fs * 1.1f }, 1.5f, lc);
            return hit;
        }
        default: fill = pal::PaperDark; border = pal::InkSoft; ink = pal::Ink; break;
    }
    if (!enabled) { fill = util::Mix(fill, pal::PaperEdge, 0.6f); ink = util::WithAlpha(ink, 0.45f); border = util::WithAlpha(border, 0.4f); }
    else if (hov) fill = util::Shade(fill, style == BtnPaper ? 1.05f : 1.18f);
    Rectangle rr = r;
    if (down) { rr.y += 1.5f; }
    // drop shadow
    if (style != BtnGhost) DrawRectangleRounded(Rectangle{ rr.x + 2, rr.y + 3, rr.width, rr.height }, 0.25f, 6, util::WithAlpha(BLACK, 0.18f));
    Panel(rr, fill, border, 0.25f, 2.f);
    if (style == BtnWood || style == BtnPrimary)
        DrawRectangleRoundedLinesEx(Rectangle{ rr.x + 4, rr.y + 4, rr.width - 8, rr.height - 8 }, 0.25f, 6, 1.f, util::WithAlpha(pal::Gold, 0.5f));
    TextCentered(FontId::SerifBold, label, rr, fs, ink);
    return hit;
}

bool IconButton(Rectangle r, Icon icon, ButtonStyle style, bool enabled) {
    bool hov = enabled && Hover(r);
    bool hit = enabled && Pressed(r);
    Color fill = style == BtnWood ? pal::Wood : (style == BtnPrimary ? pal::Green : pal::PaperDark);
    Color ink = (style == BtnWood || style == BtnPrimary) ? pal::GoldSoft : pal::Ink;
    if (hov) fill = util::Shade(fill, 1.15f);
    if (!enabled) ink = util::WithAlpha(ink, 0.4f);
    DrawRectangleRounded(Rectangle{ r.x + 2, r.y + 3, r.width, r.height }, 0.3f, 6, util::WithAlpha(BLACK, 0.2f));
    Panel(r, fill, style == BtnWood ? pal::Gold : pal::InkSoft, 0.3f, 2.f);
    DrawIcon(icon, Vector2{ r.x + r.width * 0.5f, r.y + r.height * 0.5f }, std::min(r.width, r.height) * 0.56f, ink);
    return hit;
}

void ProgressBar(Rectangle r, float t, Color fill, Color back) {
    DrawRectangleRounded(r, 0.5f, 6, back);
    t = util::Clamp01(t);
    if (t > 0.001f) DrawRectangleRounded(Rectangle{ r.x, r.y, std::max(r.height, r.width * t), r.height }, 0.5f, 6, fill);
}

void Ornament(float cx, float y, float width, Color c) {
    float hw = width * 0.5f;
    DrawLineEx(Vector2{ cx - hw, y }, Vector2{ cx - 10, y }, 1.5f, c);
    DrawLineEx(Vector2{ cx + 10, y }, Vector2{ cx + hw, y }, 1.5f, c);
    DrawPoly(Vector2{ cx, y }, 4, 6, 45, c);
    DrawCircleV(Vector2{ cx - hw, y }, 2.5f, c);
    DrawCircleV(Vector2{ cx + hw, y }, 2.5f, c);
}

bool Checkbox(Rectangle r, const std::string& label, bool* value) {
    Rectangle box{ r.x, r.y + (r.height - 26) * 0.5f, 26, 26 };
    bool hit = Pressed(r);
    if (hit) *value = !*value;
    Panel(box, pal::Paper, pal::InkSoft, 0.2f, 2.f);
    if (*value) DrawIcon(Icon::Check, Vector2{ box.x + 13, box.y + 13 }, 20, pal::Green);
    Text(FontId::Serif, label, box.x + 38, r.y + (r.height - 24) * 0.5f, 22, pal::Ink);
    return hit;
}

bool Slider(Rectangle r, float* value) {
    static Rectangle s_active{ 0, 0, 0, 0 };
    bool changed = false;
    bool hov = Hover(r);
    if (hov) WantHand();
    if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { s_active = r; ConsumePress(); }
    bool dragging = s_active.width > 0 && s_active.x == r.x && s_active.y == r.y && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) s_active = Rectangle{ 0, 0, 0, 0 };
    if (dragging && Interactive()) {
        float v = util::Clamp01((Mouse().x - r.x) / r.width);
        if (v != *value) { *value = v; changed = true; }
    }
    float cy = r.y + r.height * 0.5f;
    DrawRectangleRounded(Rectangle{ r.x, cy - 4, r.width, 8 }, 0.5f, 6, pal::PaperEdge);
    DrawRectangleRounded(Rectangle{ r.x, cy - 4, r.width * *value, 8 }, 0.5f, 6, pal::Green);
    DrawCircleV(Vector2{ r.x + r.width * *value, cy }, 11, pal::Wood);
    DrawCircleV(Vector2{ r.x + r.width * *value, cy }, 7, pal::Gold);
    return changed;
}

// ---------------------------------------------------------------------------
// Vector icons (drawn from primitives so they scale cleanly)
// ---------------------------------------------------------------------------
void DrawIcon(Icon icon, Vector2 c, float s, Color col) {
    float h = s * 0.5f;
    float t = std::max(1.5f, s * 0.09f);
    auto L = [&](float x0, float y0, float x1, float y1) { DrawLineEx(Vector2{ c.x + x0 * h, c.y + y0 * h }, Vector2{ c.x + x1 * h, c.y + y1 * h }, t, col); };
    switch (icon) {
    case Icon::Book:
        DrawRectangleRoundedLinesEx(Rectangle{ c.x - h, c.y - h * 0.75f, h * 0.95f, h * 1.5f }, 0.15f, 4, t, col);
        DrawRectangleRoundedLinesEx(Rectangle{ c.x + h * 0.05f, c.y - h * 0.75f, h * 0.95f, h * 1.5f }, 0.15f, 4, t, col);
        L(-0.75f, -0.35f, -0.25f, -0.35f); L(-0.75f, 0.0f, -0.25f, 0.0f); L(0.25f, -0.35f, 0.75f, -0.35f); L(0.25f, 0.0f, 0.75f, 0.0f);
        break;
    case Icon::Quiz:
        for (int i = 0; i < 3; i++) {
            float y = -0.6f + i * 0.6f;
            DrawCircleLinesV(Vector2{ c.x - h * 0.65f, c.y + y * h }, h * 0.18f, col);
            DrawCircleV(Vector2{ c.x - h * 0.65f, c.y + y * h }, h * (i == 1 ? 0.12f : 0.0f), col);
            L(-0.3f, y, 0.85f, y);
        }
        break;
    case Icon::Quill:
        DrawTriangle(Vector2{ c.x + h * 0.9f, c.y - h * 0.95f }, Vector2{ c.x - h * 0.35f, c.y + h * 0.35f }, Vector2{ c.x - h * 0.05f, c.y + h * 0.55f }, col);
        DrawTriangle(Vector2{ c.x + h * 0.9f, c.y - h * 0.95f }, Vector2{ c.x - h * 0.55f, c.y + h * 0.15f }, Vector2{ c.x - h * 0.35f, c.y + h * 0.35f }, util::WithAlpha(col, 0.6f));
        L(-0.35f, 0.4f, -0.85f, 0.9f);
        break;
    case Icon::Code:
        L(-0.35f, -0.6f, -0.9f, 0.0f); L(-0.9f, 0.0f, -0.35f, 0.6f);
        L(0.35f, -0.6f, 0.9f, 0.0f); L(0.9f, 0.0f, 0.35f, 0.6f);
        L(0.15f, -0.75f, -0.15f, 0.75f);
        break;
    case Icon::Target:
        DrawRing(c, h * 0.78f, h * 0.78f + t, 0, 360, 32, col);
        DrawRing(c, h * 0.45f, h * 0.45f + t, 0, 360, 24, col);
        DrawCircleV(c, h * 0.16f, col);
        break;
    case Icon::Ledger:
        L(-0.85f, 0.85f, 0.85f, 0.85f); L(-0.85f, 0.85f, -0.85f, -0.85f);
        DrawRectangleV(Vector2{ c.x - h * 0.6f, c.y + h * 0.2f }, Vector2{ h * 0.3f, h * 0.55f }, col);
        DrawRectangleV(Vector2{ c.x - h * 0.15f, c.y - h * 0.25f }, Vector2{ h * 0.3f, h * 1.0f }, col);
        DrawRectangleV(Vector2{ c.x + h * 0.3f, c.y - h * 0.65f }, Vector2{ h * 0.3f, h * 1.4f }, col);
        break;
    case Icon::Gear: {
        DrawRing(c, h * 0.32f, h * 0.62f, 0, 360, 28, col);
        for (int i = 0; i < 8; i++) {
            float a = i * PI / 4.f;
            Vector2 p{ c.x + cosf(a) * h * 0.72f, c.y + sinf(a) * h * 0.72f };
            DrawPoly(p, 4, h * 0.22f, a * RAD2DEG + 45.f, col);
        }
        break;
    }
    case Icon::Help:
        DrawRing(c, h * 0.82f, h * 0.82f + t, 0, 360, 32, col);
        Text(FontId::SerifBold, "?", c.x - h * 0.28f, c.y - h * 0.6f, h * 1.2f, col);
        break;
    case Icon::Close: L(-0.65f, -0.65f, 0.65f, 0.65f); L(0.65f, -0.65f, -0.65f, 0.65f); break;
    case Icon::ArrowLeft: L(0.7f, 0, -0.7f, 0); L(-0.7f, 0, -0.15f, -0.55f); L(-0.7f, 0, -0.15f, 0.55f); break;
    case Icon::ArrowRight: L(-0.7f, 0, 0.7f, 0); L(0.7f, 0, 0.15f, -0.55f); L(0.7f, 0, 0.15f, 0.55f); break;
    case Icon::Check: L(-0.75f, 0.0f, -0.2f, 0.55f); L(-0.2f, 0.55f, 0.8f, -0.6f); break;
    case Icon::Cross: L(-0.6f, -0.6f, 0.6f, 0.6f); L(0.6f, -0.6f, -0.6f, 0.6f); break;
    case Icon::Play: DrawTriangle(Vector2{ c.x - h * 0.5f, c.y - h * 0.7f }, Vector2{ c.x - h * 0.5f, c.y + h * 0.7f }, Vector2{ c.x + h * 0.75f, c.y }, col); break;
    case Icon::Bulb:
        DrawRing(Vector2{ c.x, c.y - h * 0.2f }, h * 0.5f, h * 0.5f + t, 0, 360, 24, col);
        L(-0.25f, 0.45f, 0.25f, 0.45f); L(-0.2f, 0.7f, 0.2f, 0.7f);
        break;
    case Icon::Clock:
        DrawRing(c, h * 0.8f, h * 0.8f + t, 0, 360, 32, col);
        L(0, 0, 0, -0.55f); L(0, 0, 0.4f, 0.2f);
        break;
    case Icon::Lock:
        DrawRectangleRounded(Rectangle{ c.x - h * 0.6f, c.y - h * 0.1f, h * 1.2f, h * 0.9f }, 0.2f, 4, col);
        DrawRing(Vector2{ c.x, c.y - h * 0.15f }, h * 0.32f, h * 0.32f + t, 180, 360, 16, col);
        break;
    case Icon::Star: {
        Vector2 pts[10];
        for (int i = 0; i < 10; i++) {
            float a = -PI / 2 + i * PI / 5.f;
            float rr = (i % 2 == 0) ? h * 0.9f : h * 0.38f;
            pts[i] = Vector2{ c.x + cosf(a) * rr, c.y + sinf(a) * rr };
        }
        for (int i = 0; i < 10; i++) DrawTriangle(c, pts[(i + 1) % 10], pts[i], col);
        break;
    }
    case Icon::Bell:
        DrawCircleSector(Vector2{ c.x, c.y + h * 0.3f }, h * 0.75f, 180, 360, 16, col);
        DrawRectangleV(Vector2{ c.x - h * 0.85f, c.y + h * 0.3f }, Vector2{ h * 1.7f, t * 1.2f }, col);
        DrawCircleV(Vector2{ c.x, c.y - h * 0.55f }, h * 0.12f, col);
        break;
    case Icon::Refresh:
        DrawRing(c, h * 0.6f, h * 0.6f + t, 30, 320, 24, col);
        DrawTriangle(Vector2{ c.x + h * 0.55f, c.y - h * 0.65f }, Vector2{ c.x + h * 0.25f, c.y - h * 0.1f }, Vector2{ c.x + h * 0.9f, c.y - h * 0.15f }, col);
        break;
    case Icon::Eye:
        DrawRing(c, h * 0.3f, h * 0.3f + t, 0, 360, 20, col);
        DrawSplineSegmentBezierQuadratic(Vector2{ c.x - h * 0.9f, c.y }, Vector2{ c.x, c.y - h * 0.9f }, Vector2{ c.x + h * 0.9f, c.y }, t, col);
        DrawSplineSegmentBezierQuadratic(Vector2{ c.x - h * 0.9f, c.y }, Vector2{ c.x, c.y + h * 0.9f }, Vector2{ c.x + h * 0.9f, c.y }, t, col);
        break;
    case Icon::Flag:
        L(-0.6f, -0.85f, -0.6f, 0.9f);
        DrawTriangle(Vector2{ c.x - h * 0.6f, c.y - h * 0.85f }, Vector2{ c.x - h * 0.6f, c.y + h * 0.05f }, Vector2{ c.x + h * 0.8f, c.y - h * 0.4f }, col);
        break;
    case Icon::Dot: DrawCircleV(c, h * 0.4f, col); break;
    }
}

} // namespace ui
