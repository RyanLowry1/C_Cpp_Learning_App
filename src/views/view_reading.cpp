// view_reading.cpp - paginated tutorial chapters with memory diagrams and callouts
#include "view.h"
#include "../audio.h"
#include <algorithm>

using namespace ui;

namespace {

const float kPara = 20.5f;

float MemHeight(const ReadBlock& b, float width) {
    int perRow = std::max(1, (int)((width + 20) / 170.f));
    int rows = ((int)b.mem.size() + perRow - 1) / perRow;
    return rows * 150.f + (b.text.empty() ? 10.f : 40.f);
}

void DrawMem(const ReadBlock& b, float x, float y, float width) {
    int perRow = std::max(1, (int)((width + 20) / 170.f));
    float cellW = 140, cellH = 58;
    std::vector<Rectangle> rects;
    for (size_t i = 0; i < b.mem.size(); i++) {
        int r = (int)i / perRow, c = (int)i % perRow;
        int inRow = std::min(perRow, (int)b.mem.size() - r * perRow);
        float rowW = inRow * cellW + (inRow - 1) * 30;
        float cx = x + (width - rowW) * 0.5f + c * (cellW + 30);
        rects.push_back(Rectangle{ cx, y + 34 + r * 150.f, cellW, cellH });
    }
    // arrows first (behind boxes)
    for (size_t i = 0; i < b.mem.size(); i++) {
        const MemCell& m = b.mem[i];
        if (m.pointsTo.empty()) continue;
        for (size_t j = 0; j < b.mem.size(); j++) {
            if (b.mem[j].name != m.pointsTo) continue;
            Rectangle a = rects[i], t = rects[j];
            // leave and arrive on the facing sides, clear of the centred address labels
            bool toRight = t.x > a.x + 1;
            bool sameCol = fabsf(t.x - a.x) < 1;
            Vector2 p0{ a.x + a.width * (sameCol ? 0.5f : (toRight ? 0.8f : 0.2f)), a.y + a.height };
            Vector2 p2{ t.x + t.width * (sameCol ? 0.2f : (toRight ? 0.2f : 0.8f)), t.y + t.height };
            float dip = 46 + fabsf(p0.x - p2.x) * 0.12f;
            Vector2 c{ (p0.x + p2.x) * 0.5f, std::max(p0.y, p2.y) + dip };
            DrawSplineSegmentBezierQuadratic(p0, c, p2, 3.f, pal::Red);
            Vector2 dir = Vector2Normalize(Vector2Subtract(p2, c));
            Vector2 nrm{ -dir.y, dir.x };
            Vector2 tip = p2;
            Vector2 b1 = Vector2Subtract(tip, Vector2Add(Vector2Scale(dir, 14), Vector2Scale(nrm, 7)));
            Vector2 b2 = Vector2Subtract(tip, Vector2Subtract(Vector2Scale(dir, 14), Vector2Scale(nrm, 7)));
            DrawTriangle(tip, b1, b2, pal::Red);
            DrawTriangle(tip, b2, b1, pal::Red);
        }
    }
    for (size_t i = 0; i < b.mem.size(); i++) {
        const MemCell& m = b.mem[i];
        Rectangle r = rects[i];
        bool ptr = !m.pointsTo.empty() || m.type.find('*') != std::string::npos;
        Text(FontId::SerifBold, m.name, r.x, r.y - 28, 19, pal::Ink);
        if (!m.type.empty()) TextRight(FontId::Mono, m.type, r.x + r.width, r.y - 25, 15, pal::Blue);
        DrawRectangleRec(Rectangle{ r.x + 3, r.y + 4, r.width, r.height }, util::WithAlpha(BLACK, 0.1f));
        DrawRectangleRec(r, ptr ? Color{ 246, 232, 210, 255 } : Color{ 252, 248, 236, 255 });
        DrawRectangleLinesEx(r, 2, ptr ? pal::Red : pal::Ink);
        float fs = 22;
        while (fs > 12 && MeasureWidth(FontId::MonoBold, m.value, fs) > r.width - 12) fs -= 1;
        TextCentered(FontId::MonoBold, m.value, r, fs, ptr ? pal::Red : pal::Ink);
        if (!m.addr.empty()) {
            std::string a = "@" + m.addr;
            float aw = MeasureWidth(FontId::Mono, a, 14);
            float ax = r.x + (r.width - aw) * 0.5f;
            // a paper-coloured chip keeps the address readable where an arrow passes underneath
            DrawRectangleRounded(Rectangle{ ax - 5, r.y + r.height + 4, aw + 10, 19 }, 0.5f, 4, Color{ 244, 236, 214, 240 });
            Text(FontId::Mono, a, ax, r.y + r.height + 6, 14, pal::InkFaint);
        }
    }
    if (!b.text.empty()) {
        float rows = (float)((b.mem.size() + perRow - 1) / perRow);
        Wrap(FontId::SerifItalic, b.text, x, y + rows * 150.f + 6, width, 16, pal::InkSoft);
    }
}

float BlockHeight(const ReadBlock& b, float w, bool draw, float x, float y) {
    switch (b.type) {
        case ReadBlock::Heading: {
            float h = Wrap(FontId::SerifBold, b.text, x, y + 8, w, 30, pal::Ink, 1.15f, draw);
            if (draw) DrawLineEx(Vector2{ x, y + 14 + h }, Vector2{ x + 90, y + 14 + h }, 2.5f, pal::Red);
            return h + 30;
        }
        case ReadBlock::Sub: return Wrap(FontId::SerifBold, b.text, x, y + 6, w, 23, pal::Wood, 1.2f, draw) + 12;
        case ReadBlock::Para: return Rich(b.text, x, y, w, kPara, pal::Ink, 1.45f, draw) + 12;
        case ReadBlock::Bullet: {
            if (draw) DrawCircleV(Vector2{ x + 9, y + kPara * 0.62f }, 4, pal::Red);
            return Rich(b.text, x + 26, y, w - 26, kPara, pal::Ink, 1.42f, draw) + 6;
        }
        case ReadBlock::Code: return syntax::DrawCode(b.text, x, y, w, 16.5f, draw, false, 11.f) + 14;
        case ReadBlock::Output: return syntax::DrawOutput(b.text, x, y, w, 16, draw) + 14;
        case ReadBlock::Tip: return page::Callout("Tip", b.text, x, y, w, pal::Green, util::WithAlpha(pal::GreenSoft, 0.55f), Icon::Bulb, draw, 18.5f) + 14;
        case ReadBlock::Warn: return page::Callout("Careful", b.text, x, y, w, pal::Red, util::WithAlpha(pal::RedSoft, 0.6f), Icon::Flag, draw, 18.5f) + 14;
        case ReadBlock::Exam: return page::Callout("Exam focus", b.text, x, y, w, pal::Amber, util::WithAlpha(pal::AmberSoft, 0.7f), Icon::Star, draw, 18.5f) + 14;
        case ReadBlock::Mem: { float h = MemHeight(b, w); if (draw) DrawMem(b, x, y, w); return h + 12; }
        case ReadBlock::Break: return 0;
    }
    return 0;
}

class ReadingView : public BookView {
public:
    explicit ReadingView(const std::string& topicId) : topic(topicId) {
        ch = Data().ChapterFor(topicId);
        tp = Data().FindTopic(topicId);
        Paginate();
        auto it = Prog().chaptersRead.find(topic);
        furthest = it == Prog().chaptersRead.end() ? 0 : it->second;
    }
    std::string SessionKey() const override { return topic; }

    void Paginate() {
        pages.clear();
        pages.push_back({});   // title page
        if (!ch) return;
        Rectangle L = page::Left();
        float avail = L.height - 24;
        std::vector<ReadBlock> blocks;
        // split very long code blocks so each piece fits on a page
        for (const ReadBlock& b : ch->blocks) {
            if (b.type == ReadBlock::Code && BlockHeight(b, L.width, false, 0, 0) > avail * 0.92f) {
                std::vector<std::string> lines = util::SplitLines(b.text);
                size_t per = (size_t)std::max(6.f, (avail * 0.85f) / (16.5f * 1.34f));
                for (size_t s = 0; s < lines.size(); s += per) {
                    std::string part;
                    for (size_t k = s; k < std::min(lines.size(), s + per); k++) { part += lines[k]; part += '\n'; }
                    part.pop_back();
                    blocks.push_back(ReadBlock{ ReadBlock::Code, part, {} });
                }
            } else blocks.push_back(b);
        }
        flat = blocks;
        std::vector<int> cur;
        float used = 0;
        for (int i = 0; i < (int)flat.size(); i++) {
            const ReadBlock& b = flat[(size_t)i];
            // "===" is a soft break: start a new page unless this one is still mostly empty
            // (which happens when the previous section just overflowed onto it)
            if (b.type == ReadBlock::Break) { if (!cur.empty() && used > avail * 0.4f) { pages.push_back(cur); cur.clear(); used = 0; } continue; }
            float h = BlockHeight(b, L.width, false, 0, 0);
            // keep headings with the following block
            float need = h;
            if ((b.type == ReadBlock::Heading || b.type == ReadBlock::Sub) && i + 1 < (int)flat.size())
                need += std::min(BlockHeight(flat[(size_t)i + 1], L.width, false, 0, 0), 140.f);
            if (!cur.empty() && used + need > avail) { pages.push_back(cur); cur.clear(); used = 0; }
            cur.push_back(i);
            used += h;
        }
        if (!cur.empty()) pages.push_back(cur);
        pages.push_back({ -1 });   // end-of-chapter page
        if (pages.size() % 2) pages.push_back({});   // keep spreads full
    }

    int SpreadCount() const { return (int)(pages.size() + 1) / 2; }

    void Turn(int dir) {
        int ns = spread + dir;
        if (ns < 0 || ns >= SpreadCount() || turnT > 0) return;
        prevSpread = spread;
        spread = ns;
        turnDir = dir;
        turnT = 0.0001f;
        sfx::Play(sfx::Page, 0.7f, 0.95f + 0.1f * (float)(spread % 3) / 3.f);
        furthest = std::max(furthest, spread * 2 + 1);
    }

    void Tick(float dt) override {
        time += dt;
        if (turnT > 0) { turnT += dt / 0.42f; if (turnT >= 1) turnT = 0; }
    }

    void OnClose() override {
        int& f = Prog().chaptersRead[topic];
        f = std::max(f, furthest);
        if (furthest >= 2) {
            Session s;
            s.time = util::NowEpoch(); s.mode = "reading"; s.topic = topic;
            s.title = tp ? tp->name : topic;
            s.score = std::min((int)pages.size(), furthest + 1); s.total = (int)pages.size();
            Prog().RecordSession(s);
        }
        Prog().Save();
    }

    void DebugCommand(const std::string& c) override {
        if (c == "next") Turn(1);
        else if (c == "prev") Turn(-1);
        else if (util::StartsWith(c, "spread ")) { spread = std::max(0, std::min(SpreadCount() - 1, atoi(c.c_str() + 7))); furthest = std::max(furthest, spread * 2 + 1); }
    }

    std::string Remark() override {
        if (furthest + 1 >= (int)pages.size() - 1) return "All the way to the end! Shall I quiz you on it while it's fresh?";
        if (furthest >= 2) return "A good start. I've marked your page - the bookmark will be waiting.";
        return "Just a peek? The best books reward a second visit.";
    }

    void Draw(bool interactive) override {
        page::DrawPaper();
        std::string right = tp ? tp->name : topic;
        page::RunningHeader("The Reading Room", right);
        if (page::ReturnRibbon(interactive)) { wantClose = true; return; }
        if (!ch) {
            Rectangle L = page::Left();
            page::Heading("This chapter is still being written", L.x, L.y + 40, L.width);
            return;
        }
        int show = spread;
        if (turnT > 0 && turnT < 0.5f) show = prevSpread;
        // during a forward turn the right page underneath is the new one; backward: left page is the new one
        int leftSpread = show, rightSpread = show;
        if (turnT > 0 && turnT < 0.5f) { if (turnDir > 0) rightSpread = spread; else leftSpread = spread; }
        DrawPage(leftSpread * 2, page::Left(), interactive && turnT <= 0);
        DrawPage(rightSpread * 2 + 1, page::Right(), interactive && turnT <= 0);
        page::PageNumbers(leftSpread * 2 + 1, rightSpread * 2 + 2);
        if (turnT > 0) DrawTurningSheet();

        // navigation
        Rectangle prev{ page::Outer - 20, page::H - 64, 150, 44 }, next{ page::W - page::Outer - 130, page::H - 64, 150, 44 };
        if (spread > 0 && Button(prev, "< Previous", BtnLink, interactive, 18)) Turn(-1);
        if (spread + 1 < SpreadCount() && Button(next, "Next >", BtnLink, interactive, 18)) Turn(1);
        if (interactive && !KeyboardCaptured()) {
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_PAGE_DOWN) || IsKeyPressed(KEY_SPACE)) Turn(1);
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_PAGE_UP)) Turn(-1);
            float w = Wheel();
            wheelAcc += w;
            if (wheelAcc <= -1.5f) { Turn(1); wheelAcc = 0; }
            if (wheelAcc >= 1.5f) { Turn(-1); wheelAcc = 0; }
        }
    }

private:
    std::string topic;
    const Chapter* ch = nullptr;
    const Topic* tp = nullptr;
    std::vector<ReadBlock> flat;
    std::vector<std::vector<int>> pages;
    int spread = 0, prevSpread = 0, turnDir = 1, furthest = 0;
    float turnT = 0, wheelAcc = 0;

    void DrawTurningSheet() {
        float t = turnT;
        float k = t < 0.5f ? cosf(t * PI) : -cosf(t * PI);    // 1 -> 0 -> 1
        float w = page::PW * k;
        bool overRight = (turnDir > 0) == (t < 0.5f);
        Rectangle r = overRight ? Rectangle{ page::PW, 0, w, page::H } : Rectangle{ page::PW - w, 0, w, page::H };
        float alpha = t < 0.5f ? 1.f : util::Clamp01((1.f - t) * 3.f);
        // cast shadow onto the page underneath
        Rectangle sh = overRight ? Rectangle{ page::PW + w, 0, 60 * (1 - k) + 10, page::H } : Rectangle{ page::PW - w - (60 * (1 - k) + 10), 0, 60 * (1 - k) + 10, page::H };
        if (overRight) DrawRectangleGradientH((int)sh.x, 0, (int)sh.width, (int)page::H, util::WithAlpha(BLACK, 0.22f * alpha), BLANK);
        else DrawRectangleGradientH((int)sh.x, 0, (int)sh.width, (int)page::H, BLANK, util::WithAlpha(BLACK, 0.22f * alpha));
        Color paper = util::WithAlpha(util::Shade(pal::Paper, 0.96f), alpha);
        DrawRectangleRec(r, paper);
        Color shade = util::WithAlpha(Color{ 120, 90, 50, 255 }, 0.35f * (1 - k) * alpha);
        if (overRight) DrawRectangleGradientH((int)r.x, 0, (int)r.width, (int)page::H, shade, util::WithAlpha(WHITE, 0.15f * alpha));
        else DrawRectangleGradientH((int)r.x, 0, (int)r.width, (int)page::H, util::WithAlpha(WHITE, 0.15f * alpha), shade);
        // faint ghost lines of text on the flipping sheet
        for (float ly = 110; ly < page::H - 110; ly += 32) {
            float x0 = overRight ? r.x + w * 0.1f : r.x + w * 0.12f;
            DrawRectangle((int)x0, (int)ly, (int)(w * 0.78f), 3, util::WithAlpha(pal::Ink, 0.07f * alpha));
        }
    }

    void DrawPage(int idx, Rectangle area, bool interactive) {
        if (idx < 0 || idx >= (int)pages.size()) return;
        if (idx == 0) { DrawTitlePage(area, interactive); return; }
        const std::vector<int>& blocks = pages[(size_t)idx];
        if (blocks.size() == 1 && blocks[0] == -1) { DrawEndPage(area, interactive); return; }
        if (blocks.empty()) { DrawNotesPage(area); return; }
        float y = area.y + 8;
        for (int bi : blocks) y += BlockHeight(flat[(size_t)bi], area.width, true, area.x, y);
    }

    // the spare page that keeps the last spread full: ruled lines for notes and a library stamp
    void DrawNotesPage(Rectangle a) {
        Text(FontId::SerifItalic, "Notes", a.x, a.y + 30, 24, pal::InkSoft);
        for (float ly = a.y + 96; ly < a.y + a.height - 170; ly += 36)
            DrawLineEx(Vector2{ a.x, ly }, Vector2{ a.x + a.width, ly }, 1.f, util::WithAlpha(pal::Blue, 0.16f));
        DrawLineEx(Vector2{ a.x + 44, a.y + 80 }, Vector2{ a.x + 44, a.y + a.height - 190 }, 1.f, util::WithAlpha(pal::Red, 0.18f));
        Color ink = util::WithAlpha(tp ? tp->color : pal::Wood, 0.38f);
        Vector2 c{ a.x + a.width - 96, a.y + a.height - 90 };
        DrawRing(c, 66, 69, 0, 360, 72, ink);
        DrawRing(c, 56, 57.5f, 0, 360, 72, ink);
        TextCentered(FontId::SerifBold, "MALLOC'S", Rectangle{ c.x - 60, c.y - 34, 120, 22 }, 16, ink);
        TextCentered(FontId::SerifBold, "LIBRARY", Rectangle{ c.x - 60, c.y - 12, 120, 22 }, 16, ink);
        TextCentered(FontId::SerifItalic, "ex libris", Rectangle{ c.x - 60, c.y + 12, 120, 20 }, 14, ink);
    }

    void DrawTitlePage(Rectangle a, bool interactive) {
        Color accent = tp ? tp->color : pal::Wood;
        float y = a.y + 40;
        Text(FontId::SerifItalic, "Chapter", a.x, y, 24, pal::InkSoft);
        y += 40;
        y += Wrap(FontId::SerifBold, ch->title.empty() ? (tp ? tp->name : topic) : ch->title, a.x, y, a.width, 52, pal::Ink, 1.1f) + 8;
        DrawRectangle((int)a.x, (int)y, 120, 5, accent);
        y += 26;
        if (!ch->subtitle.empty()) y += Wrap(FontId::SerifItalic, ch->subtitle, a.x, y, a.width, 24, pal::InkSoft, 1.3f) + 16;
        if (tp) {
            std::string ex = tp->exam == 1 ? "Coding Exam 1 material" : (tp->exam == 2 ? "Coding Exam 2 material" : "C++ / STL material");
            page::Chip(ex, a.x, y, tp->exam == 1 ? pal::Blue : (tp->exam == 2 ? pal::Red : pal::Green), 16);
            y += 50;
        }
        Text(FontId::SerifBold, "In this chapter", a.x, y, 22, pal::Ink);
        y += 38;
        for (int p = 1; p < (int)pages.size(); p++) {
            for (int bi : pages[(size_t)p]) {
                if (bi < 0 || flat[(size_t)bi].type != ReadBlock::Heading) continue;
                if (y > a.y + a.height - 40) break;
                Rectangle row{ a.x, y - 2, a.width, 30 };
                bool hov = interactive && Hover(row);
                std::string label = flat[(size_t)bi].text;
                Text(FontId::Serif, label, a.x + 14, y, 19, hov ? pal::Red : pal::Ink);
                std::string pn = std::to_string(p + 1);
                TextRight(FontId::SerifItalic, pn, a.x + a.width, y, 19, pal::InkSoft);
                float lw = MeasureWidth(FontId::Serif, label, 19);
                for (float dx = a.x + 24 + lw; dx < a.x + a.width - 30; dx += 9) DrawCircleV(Vector2{ dx, y + 15 }, 1.2f, pal::InkFaint);
                if (interactive && Pressed(row)) { int target = p / 2; if (target != spread) { prevSpread = spread; spread = target; turnDir = 1; turnT = 0.0001f; sfx::Play(sfx::Page); furthest = std::max(furthest, p); } }
                y += 32;
            }
        }
        int fr = std::min(furthest, (int)pages.size() - 1);
        if (fr >= 2 && spread == 0) {
            Rectangle bm{ a.x, a.y + a.height - 50, 260, 46 };
            if (Button(bm, TextFormat("Continue from page %d", fr + 1), BtnPrimary, interactive, 18)) {
                prevSpread = spread; spread = fr / 2; turnDir = 1; turnT = 0.0001f; sfx::Play(sfx::Page);
            }
        }
    }

    void DrawEndPage(Rectangle a, bool interactive) {
        float y = a.y + 60;
        Ornament(a.x + a.width * 0.5f, y, a.width * 0.6f, pal::InkFaint);
        y += 40;
        std::string done = "You've reached the end of **" + std::string(tp ? tp->name : topic) + "**.";
        y += Rich(done, a.x, y, a.width, 24, pal::Ink) + 10;
        y += Wrap(FontId::SerifItalic, "Knowledge that isn't tested tends to evaporate. Shall I fetch something to check it stuck?", a.x, y, a.width, 20, pal::InkSoft) + 30;
        Rectangle b1{ a.x, y, a.width, 58 };
        if (Button(b1, "Quiz me: multiple choice", BtnPrimary, interactive, 21)) { requestAction = "mcq"; requestArg = topic; wantClose = true; }
        y += 72;
        Rectangle b2{ a.x, y, a.width, 58 };
        if (Button(b2, "Written test on this chapter", BtnWood, interactive, 21)) { requestAction = "written"; requestArg = topic; wantClose = true; }
        y += 72;
        Rectangle b3{ a.x, y, a.width, 58 };
        if (Button(b3, "Coding practice for this topic", BtnPaper, interactive, 21)) { requestAction = "coding"; requestArg = topic; wantClose = true; }
        y += 90;
        Ornament(a.x + a.width * 0.5f, y, a.width * 0.6f, pal::InkFaint);
    }
};

} // namespace

std::unique_ptr<BookView> MakeReadingView(const std::string& topic) { return std::make_unique<ReadingView>(topic); }
