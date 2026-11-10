// page.cpp - paper, page chrome and question pickers shared by all book views
#include "view.h"
#include <algorithm>
#include <random>

using namespace ui;

namespace page {

static Texture2D g_paper{};
static bool g_paperReady = false;

static void MakePaper() {
    // opaque, very low-contrast grain (every pixel stays fully opaque so the desk
    // never shows through the page)
    const int w = 780, h = 1000;
    Color* px = (Color*)MemAlloc((unsigned int)(w * h * sizeof(Color)));
    util::Rng rng(8086);
    // smooth large-scale mottling from a few random blobs
    struct Blob { float x, y, r, k; };
    std::vector<Blob> blobs;
    for (int i = 0; i < 14; i++) blobs.push_back(Blob{ rng.R(0, (float)w), rng.R(0, (float)h), rng.R(120, 320), rng.R(-0.018f, 0.018f) });
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float k = 1.f + (rng.F() - 0.5f) * 0.035f;          // fine grain
            for (const Blob& b : blobs) {
                float dx = x - b.x, dy = y - b.y;
                float d2 = (dx * dx + dy * dy) / (b.r * b.r);
                if (d2 < 1.f) k += b.k * (1.f - d2);
            }
            if ((x * 7 + y * 13) % 97 == 0 && rng.F() < 0.3f) k -= 0.03f;   // occasional fibre fleck
            Color c = pal::Paper;
            c.r = (unsigned char)std::min(255.f, c.r * k);
            c.g = (unsigned char)std::min(255.f, c.g * k);
            c.b = (unsigned char)std::min(255.f, c.b * k * 0.995f);
            c.a = 255;
            px[y * w + x] = c;
        }
    }
    Image img{ px, w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    g_paper = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&g_paper);
    SetTextureFilter(g_paper, TEXTURE_FILTER_TRILINEAR);
    g_paperReady = true;
}

void DrawPaper() {
    if (!g_paperReady) MakePaper();
    DrawTexturePro(g_paper, Rectangle{ 0, 0, (float)g_paper.width, (float)g_paper.height }, Rectangle{ 0, 0, PW, H }, Vector2{ 0, 0 }, 0, WHITE);
    DrawTexturePro(g_paper, Rectangle{ (float)g_paper.width, 0, -(float)g_paper.width, (float)g_paper.height }, Rectangle{ PW, 0, PW, H }, Vector2{ 0, 0 }, 0, WHITE);
    // gutter shadow towards the spine
    DrawRectangleGradientH((int)(PW - 90), 0, 90, (int)H, Color{ 120, 90, 50, 0 }, Color{ 110, 80, 45, 70 });
    DrawRectangleGradientH((int)PW, 0, 90, (int)H, Color{ 110, 80, 45, 70 }, Color{ 120, 90, 50, 0 });
    DrawRectangle((int)PW - 1, 0, 2, (int)H, Color{ 120, 90, 50, 90 });
    // darker outer edges
    DrawRectangleGradientH(0, 0, 26, (int)H, Color{ 150, 120, 80, 45 }, Color{ 150, 120, 80, 0 });
    DrawRectangleGradientH((int)W - 26, 0, 26, (int)H, Color{ 150, 120, 80, 0 }, Color{ 150, 120, 80, 45 });
    DrawRectangleGradientV(0, 0, (int)W, 18, Color{ 150, 120, 80, 35 }, Color{ 150, 120, 80, 0 });
    DrawRectangleGradientV(0, (int)H - 18, (int)W, 18, Color{ 150, 120, 80, 0 }, Color{ 150, 120, 80, 35 });
}

void RunningHeader(const std::string& left, const std::string& right) {
    Color c = pal::InkFaint;
    Text(FontId::SerifItalic, left, Outer, 30, 17, c);
    TextRight(FontId::SerifItalic, right, W - 232, 30, 17, c);   // leave room for the Return ribbon
    DrawLineEx(Vector2{ Outer, 54 }, Vector2{ PW - Inner, 54 }, 1.f, util::WithAlpha(pal::InkFaint, 0.5f));
    DrawLineEx(Vector2{ PW + Inner, 54 }, Vector2{ W - Outer, 54 }, 1.f, util::WithAlpha(pal::InkFaint, 0.5f));
}

void PageNumbers(int l, int r) {
    if (l > 0) { std::string s = "- " + std::to_string(l) + " -"; Text(FontId::SerifItalic, s, PW * 0.5f - MeasureWidth(FontId::SerifItalic, s, 16) * 0.5f, H - 46, 16, pal::InkFaint); }
    if (r > 0) { std::string s = "- " + std::to_string(r) + " -"; Text(FontId::SerifItalic, s, PW * 1.5f - MeasureWidth(FontId::SerifItalic, s, 16) * 0.5f, H - 46, 16, pal::InkFaint); }
}

bool ReturnRibbon(bool interactive) {
    Rectangle r{ W - 214, -6, 120, 70 };
    bool hov = interactive && Hover(r);
    float drop = hov ? 10.f : 0.f;
    Color red = hov ? Color{ 170, 46, 46, 255 } : Color{ 140, 34, 38, 255 };
    float x0 = r.x, x1 = r.x + r.width, yb = r.y + r.height + drop;
    DrawRectangleV(Vector2{ x0 + 3, r.y + 3 }, Vector2{ r.width, r.height + drop - 14 }, util::WithAlpha(BLACK, 0.15f));
    DrawRectangleV(Vector2{ x0, r.y }, Vector2{ r.width, r.height + drop - 14 }, red);
    DrawTriangle(Vector2{ x0, yb - 14 }, Vector2{ x0 + r.width * 0.5f, yb - 2 }, Vector2{ x0 + r.width * 0.5f, yb - 14 }, red);
    DrawTriangle(Vector2{ x0 + r.width * 0.5f, yb - 14 }, Vector2{ x0 + r.width * 0.5f, yb - 2 }, Vector2{ x1, yb - 14 }, red);
    DrawRectangleV(Vector2{ x0, yb - 14 }, Vector2{ 1, 0 }, red);
    DrawLineEx(Vector2{ x0 + 6, r.y }, Vector2{ x0 + 6, yb - 18 }, 1.f, util::WithAlpha(pal::Gold, 0.6f));
    DrawLineEx(Vector2{ x1 - 6, r.y }, Vector2{ x1 - 6, yb - 18 }, 1.f, util::WithAlpha(pal::Gold, 0.6f));
    TextCentered(FontId::SerifBold, "Return", Rectangle{ x0, r.y + 10 + drop * 0.5f, r.width, 24 }, 17, pal::GoldSoft);
    TextCentered(FontId::SerifItalic, "book (Esc)", Rectangle{ x0, r.y + 30 + drop * 0.5f, r.width, 20 }, 13, util::WithAlpha(pal::GoldSoft, 0.85f));
    return interactive && Pressed(r);
}

float Heading(const std::string& s, float x, float y, float w, float size, Color c) {
    float h = Wrap(FontId::SerifBold, s, x, y, w, size, c, 1.18f);
    return h;
}

float Callout(const std::string& label, const std::string& text, float x, float y, float w, Color accent, Color fill, Icon icon, bool draw, float size) {
    float pad = 16.f;
    float textW = w - pad * 2 - 34;
    float th = Rich(text, 0, 0, textW, size, pal::Ink, 1.36f, false);
    float lh = label.empty() ? 0.f : size * 1.1f + 6;
    float h = th + lh + pad * 1.5f;
    if (!draw) return h;
    Rectangle r{ x, y, w, h };
    DrawRectangleRounded(r, 0.08f, 6, fill);
    DrawRectangleRounded(Rectangle{ x, y, 6, h }, 0.6f, 4, accent);
    DrawIcon(icon, Vector2{ x + pad + 12, y + pad + 10 }, 22, accent);
    float ty = y + pad * 0.8f;
    if (!label.empty()) { Text(FontId::SerifBold, label, x + pad + 34, ty, size * 0.9f, accent); ty += lh; }
    Rich(text, x + pad + 34, ty, textW, size, pal::Ink, 1.36f, true);
    return h;
}

void Chip(const std::string& text, float x, float y, Color c, float size, float* outW) {
    float w = MeasureWidth(FontId::SerifBold, text, size) + 18;
    Rectangle r{ x, y, w, size * 1.6f };
    DrawRectangleRounded(r, 0.5f, 6, util::WithAlpha(c, 0.16f));
    DrawRectangleRoundedLinesEx(r, 0.5f, 6, 1.2f, util::WithAlpha(c, 0.7f));
    TextCentered(FontId::SerifBold, text, r, size, c);
    if (outW) *outW = w;
}

Color DiffColor(const std::string& d) {
    std::string l = util::ToLower(d);
    if (l == "easy") return pal::Green;
    if (l == "medium") return pal::Amber;
    return pal::Red;
}

} // namespace page

// ---------------------------------------------------------------------------
// Question pickers. preferWeak puts previously-missed and unseen questions first.
// ---------------------------------------------------------------------------
static std::mt19937& Rng() { static std::mt19937 r((unsigned)util::NowEpoch()); return r; }

template <class GetId, class GetTopic>
static std::vector<int> Pick(int total, const std::vector<std::string>& topics, int count, bool preferWeak, GetId id, GetTopic topic) {
    std::vector<int> pool;
    for (int i = 0; i < total; i++) {
        if (topics.empty() || std::find(topics.begin(), topics.end(), topic(i)) != topics.end()) pool.push_back(i);
    }
    std::shuffle(pool.begin(), pool.end(), Rng());
    if (preferWeak) {
        auto score = [&](int i) {
            ItemStat s = Prog().Item(id(i));
            if (s.attempts == 0) return 1.0f;                      // unseen: medium priority
            float acc = (float)s.correct / s.attempts;
            return (s.lastCorrect ? 0.5f : 2.0f) + (1.f - acc);    // missed last time: top priority
        };
        std::stable_sort(pool.begin(), pool.end(), [&](int a, int b) { return score(a) > score(b); });
    }
    if ((int)pool.size() > count) pool.resize((size_t)count);
    if (!preferWeak) std::shuffle(pool.begin(), pool.end(), Rng());
    return pool;
}

std::vector<int> PickMcqs(const std::vector<std::string>& topics, int count, bool preferWeak) {
    const Content& C = Data();
    return Pick((int)C.mcqs.size(), topics, count, preferWeak, [&](int i) { return C.mcqs[(size_t)i].id; }, [&](int i) { return C.mcqs[(size_t)i].topic; });
}

std::vector<int> PickWritten(const std::vector<std::string>& topics, int count, bool preferWeak) {
    const Content& C = Data();
    return Pick((int)C.written.size(), topics, count, preferWeak, [&](int i) { return C.written[(size_t)i].id; }, [&](int i) { return C.written[(size_t)i].topic; });
}
