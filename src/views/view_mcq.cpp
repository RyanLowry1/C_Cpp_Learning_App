// view_mcq.cpp - multiple choice quiz book
#include "view.h"
#include "../audio.h"
#include <algorithm>
#include <random>

using namespace ui;

namespace {

struct QState {
    int q;                      // index into Data().mcqs
    std::vector<int> order;     // display order of options
    int chosen = -1;            // display index chosen
};

class McqView : public BookView {
public:
    McqView(const std::vector<int>& qs, const std::string& t, const std::string& topicKey, const std::string& m)
        : title(t), sessionTopic(topicKey), mode(m) {
        std::mt19937 rng((unsigned)util::NowEpoch());
        for (int q : qs) {
            QState s; s.q = q;
            const MCQ& m = Data().mcqs[(size_t)q];
            for (int i = 0; i < (int)m.options.size(); i++) s.order.push_back(i);
            bool fixedOrder = false;
            for (auto& o : m.options) {
                std::string l = util::ToLower(o);
                if (l.find("above") != std::string::npos || l.find("both ") != std::string::npos || l.find("all of") != std::string::npos ||
                    l.find("none of") != std::string::npos || l.find("a and b") != std::string::npos) fixedOrder = true;
            }
            if (!fixedOrder) std::shuffle(s.order.begin(), s.order.end(), rng);
            states.push_back(s);
        }
    }

    std::string SessionKey() const override { return sessionTopic; }

    void Tick(float dt) override {
        time += dt;
        if (!finished) elapsed += dt;
        feedbackT = std::min(1.f, feedbackT + dt * 4.f);
    }

    bool Answered(int i) const { return states[(size_t)i].chosen >= 0; }
    bool Correct(int i) const {
        const QState& s = states[(size_t)i];
        return s.chosen >= 0 && s.order[(size_t)s.chosen] == Data().mcqs[(size_t)s.q].correct;
    }
    int Score() const { int n = 0; for (int i = 0; i < (int)states.size(); i++) if (Correct(i)) n++; return n; }
    int AnsweredCount() const { int n = 0; for (int i = 0; i < (int)states.size(); i++) if (Answered(i)) n++; return n; }

    void Choose(int display) {
        QState& s = states[(size_t)cur];
        if (s.chosen >= 0 || reviewing) return;
        s.chosen = display;
        feedbackT = 0;
        bool ok = Correct(cur);
        Prog().RecordAnswer(Data().mcqs[(size_t)s.q].id, ok);
        sfx::Play(ok ? sfx::Correct : sfx::Wrong, 0.8f);
    }

    void Next() {
        if (cur + 1 < (int)states.size()) { cur++; feedbackT = 1; sfx::Play(sfx::Page, 0.6f); }
        else Finish();
    }

    void Finish() {
        if (!finished) {
            finished = true;
            Record();
            sfx::Play(Score() * 2 >= (int)states.size() ? sfx::Success : sfx::Page, 0.7f);
        }
        showSummary = true;
        reviewing = false;
    }

    void Record() {
        if (recorded || AnsweredCount() == 0) return;
        recorded = true;
        Session s;
        s.time = util::NowEpoch();
        s.mode = mode;
        s.topic = sessionTopic;
        s.title = title;
        s.score = Score();
        s.total = AnsweredCount();
        s.seconds = elapsed;
        for (int i = 0; i < (int)states.size(); i++)
            if (Answered(i)) s.items.push_back({ Data().mcqs[(size_t)states[(size_t)i].q].id, Correct(i) });
        Prog().RecordSession(s);
    }

    void OnClose() override { Record(); Prog().Save(); }

    std::string Remark() override {
        int n = AnsweredCount();
        if (n == 0) return "Not a single question attempted? The book shan't be offended... much.";
        int sc = Score();
        float p = (float)sc / n;
        // find the topic with the most mistakes
        std::map<std::string, int> wrong;
        for (int i = 0; i < (int)states.size(); i++) if (Answered(i) && !Correct(i)) wrong[Data().mcqs[(size_t)states[(size_t)i].q].topic]++;
        std::string worst; int wn = 0;
        for (auto& kv : wrong) if (kv.second > wn) { wn = kv.second; worst = kv.first; }
        const Topic* t = Data().FindTopic(worst);
        std::string score = TextFormat("%d out of %d", sc, n);
        if (p >= 0.9f) return score + "! Outstanding. You may borrow my good pen.";
        if (p >= 0.7f) return score + ". Solid work." + (t ? std::string(" Keep an eye on **") + t->name + "** - that's where you slipped." : std::string());
        if (p >= 0.5f) return score + ". A pass, but " + (t ? std::string("**") + t->name + "** needs another look. I've noted it under Problem Areas." : std::string("there's room to grow."));
        return score + ". Hmm. " + (t ? std::string("I'd suggest a quiet hour with the **") + t->name + "** volume in the Reading Room." : std::string("Try the Reading Room first, then come back."));
    }

    bool HandleEscape() override {
        if (reviewing) { reviewing = false; showSummary = true; return true; }
        return false;
    }

    void DebugCommand(const std::string& c) override {
        if (util::StartsWith(c, "answer ")) Choose(atoi(c.c_str() + 7));
        else if (c == "next") Next();
        else if (c == "finish") Finish();
        else if (c == "correct" && !states.empty()) {   // pick the right answer
            const QState& s = states[(size_t)cur];
            for (int d = 0; d < (int)s.order.size(); d++) if (s.order[(size_t)d] == Data().mcqs[(size_t)s.q].correct) Choose(d);
        }
    }

    void Draw(bool interactive) override {
        page::DrawPaper();
        page::RunningHeader("Multiple Choice  -  " + title, finished ? "Results" : TextFormat("Question %d of %d", cur + 1, (int)states.size()));
        if (page::ReturnRibbon(interactive)) { wantClose = true; return; }
        if (states.empty()) {
            Rectangle L = page::Left();
            page::Heading("This volume is empty", L.x, L.y + 40, L.width);
            Wrap(FontId::Serif, "No questions match this selection yet.", L.x, L.y + 110, L.width, 22, pal::InkSoft);
            return;
        }
        if (showSummary) DrawSummary(interactive);
        else DrawQuestion(interactive);
    }

private:
    std::vector<QState> states;
    std::string title, sessionTopic, mode;
    int cur = 0;
    bool finished = false, showSummary = false, reviewing = false, recorded = false;
    float elapsed = 0, feedbackT = 1;
    float summaryScroll = 0;

    void DrawQuestion(bool interactive) {
        const QState& s = states[(size_t)cur];
        const MCQ& q = Data().mcqs[(size_t)s.q];
        const Topic* t = Data().FindTopic(q.topic);
        Rectangle L = page::Left(), R = page::Right();

        // ---------- left page: the question ----------
        float y = L.y + 6;
        float cw = 0;
        page::Chip(t ? t->name : q.topic, L.x, y, t ? t->color : pal::Wood, 15, &cw);
        std::string diff = q.diff >= 3 ? "Hard" : (q.diff == 2 ? "Medium" : "Easy");
        page::Chip(diff, L.x + cw + 10, y, page::DiffColor(diff), 15);
        y += 44;
        Text(FontId::SerifBold, TextFormat("Question %d", cur + 1), L.x, y, 30, pal::Ink);
        y += 46;
        y += Rich(q.question, L.x, y, L.width, 25, pal::Ink, 1.42f) + 14;
        if (!q.code.empty()) {
            float avail = L.y + L.height - 70 - y;
            float h = syntax::DrawCode(q.code, L.x, y, L.width, 18, false);
            float size = h > avail ? std::max(12.f, 18.f * avail / h) : 18.f;
            syntax::DrawCode(q.code, L.x, y, L.width, size, true);
        }
        // progress trail
        int n = (int)states.size();
        float dot = std::min(26.f, (L.width - 10) / n);
        for (int i = 0; i < n; i++) {
            Vector2 c{ L.x + dot * 0.5f + i * dot, L.y + L.height - 18 };
            Color col = !Answered(i) ? pal::PaperEdge : (Correct(i) ? pal::Green : pal::Red);
            DrawCircleV(c, dot * 0.32f + (i == cur ? 2.f : 0.f), col);
            if (i == cur) DrawRing(c, dot * 0.32f + 4, dot * 0.32f + 6, 0, 360, 20, pal::Ink);
            if (interactive && Answered(i) && Pressed(Rectangle{ c.x - dot * 0.5f, c.y - 12, dot, 24 })) { cur = i; reviewing = finished; }
        }

        // ---------- right page: options ----------
        float oy = R.y + 4;
        Text(FontId::SerifItalic, s.chosen < 0 ? "Choose one answer  (keys 1-4 or A-D)" : "", R.x, oy, 18, pal::InkSoft);
        oy += 36;
        const char* letters = "ABCDEFGH";
        bool answered = s.chosen >= 0;
        int correctDisplay = -1;
        for (int d = 0; d < (int)s.order.size(); d++) if (s.order[(size_t)d] == q.correct) correctDisplay = d;
        for (int d = 0; d < (int)s.order.size(); d++) {
            const std::string& opt = q.options[(size_t)s.order[(size_t)d]];
            bool isCode = opt.find(';') != std::string::npos || (opt.find('*') != std::string::npos && opt.find(' ') == std::string::npos) || opt.find("->") != std::string::npos;
            float textW = R.width - 86;
            float th = isCode ? 22.f * 1.3f : Rich(opt, 0, 0, textW, 21, pal::Ink, 1.35f, false);
            float h = std::max(64.f, th + 30);
            Rectangle card{ R.x, oy, R.width, h };
            bool hov = interactive && !answered && Hover(card);
            Color fill = pal::Paper, border = util::WithAlpha(pal::Ink, 0.35f), badge = pal::Wood;
            if (answered) {
                if (d == correctDisplay) { fill = pal::GreenSoft; border = pal::Green; badge = pal::Green; }
                else if (d == s.chosen) { fill = pal::RedSoft; border = pal::Red; badge = pal::Red; }
                else { fill = util::Mix(pal::Paper, pal::PaperDark, 0.5f); border = util::WithAlpha(pal::Ink, 0.15f); badge = pal::InkFaint; }
            } else if (hov) { fill = util::Shade(pal::Paper, 1.03f); border = pal::Wood; card.x -= 4; }
            DrawRectangleRounded(Rectangle{ card.x + 2, card.y + 3, card.width, card.height }, 0.14f, 6, util::WithAlpha(BLACK, 0.08f));
            Panel(card, fill, border, 0.14f, answered && (d == correctDisplay || d == s.chosen) ? 2.5f : 1.5f);
            Vector2 bc{ card.x + 32, card.y + h * 0.5f };
            DrawCircleV(bc, 18, badge);
            TextCentered(FontId::SerifBold, std::string(1, letters[d]), Rectangle{ bc.x - 18, bc.y - 18, 36, 36 }, 20, pal::Paper);
            if (isCode) Text(FontId::Mono, opt, card.x + 66, card.y + (h - 22) * 0.5f, 20, pal::Ink);
            else Rich(opt, card.x + 66, card.y + (h - th) * 0.5f, textW, 21, pal::Ink, 1.35f);
            if (answered && d == correctDisplay) DrawIcon(Icon::Check, Vector2{ card.x + card.width - 26, card.y + h * 0.5f }, 22, pal::Green);
            if (answered && d == s.chosen && d != correctDisplay) DrawIcon(Icon::Cross, Vector2{ card.x + card.width - 26, card.y + h * 0.5f }, 18, pal::Red);
            if (interactive && !answered && Pressed(card)) Choose(d);
            oy += h + 12;
        }
        if (interactive && !answered && !KeyboardCaptured()) {
            for (int d = 0; d < (int)s.order.size() && d < 6; d++)
                if (IsKeyPressed(KEY_ONE + d) || IsKeyPressed(KEY_A + d) || IsKeyPressed(KEY_KP_1 + d)) Choose(d);
        }
        if (answered) {
            float a = util::EaseOutCubic(feedbackT);
            bool ok = Correct(cur);
            oy += 6;
            std::string label = ok ? "Correct!" : "Not quite.";
            float h = page::Callout(label, q.why.empty() ? "No explanation recorded for this one." : q.why, R.x, oy + (1 - a) * 12, R.width,
                                    ok ? pal::Green : pal::Red, ok ? util::WithAlpha(pal::GreenSoft, 0.7f) : util::WithAlpha(pal::RedSoft, 0.7f),
                                    ok ? Icon::Check : Icon::Bulb, true, 19);
            (void)h;
            Rectangle nb{ R.x + R.width - 240, R.y + R.height - 54, 240, 52 };
            std::string nl = reviewing ? "Back to results" : (cur + 1 < (int)states.size() ? "Next question" : "See results");
            if (Button(nb, nl, BtnPrimary, interactive, 21) || (interactive && !KeyboardCaptured() && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_RIGHT)))) {
                if (reviewing) { reviewing = false; showSummary = true; }
                else Next();
            }
            DrawIcon(Icon::ArrowRight, Vector2{ nb.x + nb.width - 24, nb.y + 26 }, 16, pal::GoldSoft);
        }
        if (!answered && !finished) {
            Rectangle fb{ R.x, R.y + R.height - 44, 220, 40 };
            if (AnsweredCount() > 0 && Button(fb, "Finish early", BtnGhost, interactive, 17)) Finish();
        }
    }

    void DrawSummary(bool interactive) {
        Rectangle L = page::Left(), R = page::Right();
        int n = AnsweredCount(), sc = Score();
        float p = n ? (float)sc / n : 0.f;
        float y = L.y + 10;
        Text(FontId::SerifItalic, "The verdict", L.x, y, 22, pal::InkSoft);
        y += 40;
        std::string big = TextFormat("%d / %d", sc, n);
        Text(FontId::SerifBold, big, L.x, y, 92, p >= 0.7f ? pal::Green : (p >= 0.5f ? pal::Amber : pal::Red));
        y += 112;
        const char* grade = p >= 0.9f ? "Distinction" : p >= 0.7f ? "Merit" : p >= 0.5f ? "Pass" : "Needs more study";
        Text(FontId::SerifBold, grade, L.x, y, 30, pal::Ink);
        y += 46;
        Text(FontId::Serif, TextFormat("%d%% correct  -  %s taken", (int)(p * 100 + 0.5f), util::FormatDuration(elapsed).c_str()), L.x, y, 20, pal::InkSoft);
        y += 50;
        Ornament(L.x + L.width * 0.5f, y, L.width * 0.7f, pal::InkFaint);
        y += 30;
        // per-topic breakdown
        std::map<std::string, std::pair<int, int>> by;
        for (int i = 0; i < (int)states.size(); i++) {
            if (!Answered(i)) continue;
            auto& e = by[Data().mcqs[(size_t)states[(size_t)i].q].topic];
            e.second++;
            if (Correct(i)) e.first++;
        }
        Text(FontId::SerifBold, "By topic", L.x, y, 22, pal::Ink);
        y += 36;
        for (auto& kv : by) {
            const Topic* t = Data().FindTopic(kv.first);
            float tp = (float)kv.second.first / kv.second.second;
            Text(FontId::Serif, t ? t->name : kv.first, L.x, y, 19, pal::Ink);
            TextRight(FontId::Serif, TextFormat("%d/%d", kv.second.first, kv.second.second), L.x + L.width, y, 19, pal::InkSoft);
            ProgressBar(Rectangle{ L.x, y + 26, L.width, 7 }, tp, util::Mix(pal::Red, pal::Green, tp), pal::PaperEdge);
            y += 46;
            if (y > L.y + L.height - 120) break;
        }
        // buttons
        int wrongCount = n - sc;
        Rectangle b1{ L.x, L.y + L.height - 56, 300, 52 };
        if (wrongCount > 0 && Button(b1, TextFormat("Retry the %d I missed", wrongCount), BtnPrimary, interactive, 19)) RetryWrong();
        // ---------- right page: answer list ----------
        Text(FontId::SerifBold, "Your answers", R.x, R.y + 4, 26, pal::Ink);
        Text(FontId::SerifItalic, "Click any question to review it", R.x, R.y + 40, 17, pal::InkSoft);
        float ly = R.y + 80;
        Rectangle listArea{ R.x - 6, ly, R.width + 12, R.height - 90 };
        PushClip(listArea);
        float rowH = 50;
        float contentH = states.size() * rowH;
        if (interactive && Hover(listArea)) { summaryScroll -= Wheel() * 60; }
        summaryScroll = std::max(0.f, std::min(summaryScroll, std::max(0.f, contentH - listArea.height)));
        for (int i = 0; i < (int)states.size(); i++) {
            float ry = ly + i * rowH - summaryScroll;
            if (ry < ly - rowH || ry > listArea.y + listArea.height) continue;
            const MCQ& q = Data().mcqs[(size_t)states[(size_t)i].q];
            Rectangle row{ R.x, ry, R.width, rowH - 6 };
            bool hov = interactive && Hover(row);
            DrawRectangleRounded(row, 0.2f, 6, hov ? util::WithAlpha(pal::PaperDark, 0.8f) : util::WithAlpha(pal::PaperDark, 0.35f));
            Color c = !Answered(i) ? pal::InkFaint : (Correct(i) ? pal::Green : pal::Red);
            DrawIcon(!Answered(i) ? Icon::Dot : (Correct(i) ? Icon::Check : Icon::Cross), Vector2{ row.x + 22, row.y + row.height * 0.5f }, 18, c);
            std::string qt = util::ReplaceAll(util::ReplaceAll(q.question, "`", ""), "**", "");
            qt = util::ReplaceAll(qt, "\n", " ");
            float maxW = row.width - 90;
            std::string shown = TextFormat("%d. ", i + 1) + qt;
            while (shown.size() > 4 && MeasureWidth(FontId::Serif, shown, 17) > maxW) shown = shown.substr(0, shown.size() - 4) + "...";
            Text(FontId::Serif, shown, row.x + 44, row.y + 11, 17, pal::Ink);
            if (Answered(i) && Pressed(row)) { cur = i; reviewing = true; showSummary = false; feedbackT = 1; }
        }
        PopClip();
    }

    void RetryWrong() {
        std::vector<QState> again;
        for (int i = 0; i < (int)states.size(); i++) if (Answered(i) && !Correct(i)) { QState s = states[(size_t)i]; s.chosen = -1; again.push_back(s); }
        if (again.empty()) return;
        states = again;
        cur = 0; finished = false; showSummary = false; reviewing = false; recorded = false; elapsed = 0;
        title = "Retry: " + title;
        sfx::Play(sfx::Page);
    }
};

} // namespace

std::unique_ptr<BookView> MakeMcqView(const std::vector<int>& questions, const std::string& title, const std::string& sessionTopic, const std::string& mode) {
    return std::make_unique<McqView>(questions, title, sessionTopic, mode);
}
