// view_written.cpp - typed-answer test: predict the output, fill the blank, fix the bug, explain
#include "view.h"
#include "../audio.h"
#include "../code_editor.h"
#include "../harness.h"

using namespace ui;

namespace {

struct WState {
    int q;
    std::string answer;
    int verdict = 0;        // 0 unanswered, 1 correct, 2 wrong, 3 self-marked correct
};

static bool CheckAnswer(const Written& w, const std::string& given) {
    if (util::Trim(given).empty()) return false;
    for (const std::string& a : w.answers) {
        if (w.kind == "output") {
            if (NormaliseAnswer(given, false) == NormaliseAnswer(a, false)) return true;
            if (NormaliseAnswer(given, true) == NormaliseAnswer(a, true)) return true;
        } else if (w.kind == "concept") {
            std::string g = NormaliseAnswer(given, false), k = NormaliseAnswer(a, false);
            if (!k.empty() && g.find(k) != std::string::npos) return true;
        } else {
            if (NormaliseAnswer(given, true) == NormaliseAnswer(a, true)) return true;
        }
    }
    return false;
}

static const char* KindTitle(const std::string& k) {
    if (k == "output") return "Predict the output";
    if (k == "blank") return "Fill in the blank";
    if (k == "fix") return "Find & fix the bug";
    return "Short answer";
}
static const char* KindHint(const std::string& k) {
    if (k == "output") return "Type exactly what the program prints. Spacing and new lines are forgiven.";
    if (k == "blank") return "Type only the missing code - the part that replaces ____.";
    if (k == "fix") return "Type the corrected line (or the fix) - whitespace is ignored.";
    return "A few words are enough - I'm looking for the key term.";
}

class WrittenView : public BookView {
public:
    WrittenView(const std::vector<int>& qs, const std::string& t, const std::string& topicKey) : title(t), sessionTopic(topicKey) {
        for (int q : qs) states.push_back(WState{ q });
        LoadEditor();
    }
    std::string SessionKey() const override { return sessionTopic; }

    void Tick(float dt) override { time += dt; if (!finished) elapsed += dt; fb = std::min(1.f, fb + dt * 4); }

    void LoadEditor() {
        if (states.empty()) return;
        const Written& w = Data().written[(size_t)states[(size_t)cur].q];
        box = CodeEditor();
        box.plain = true;
        box.singleLine = !w.multiline;
        box.placeholder = w.multiline ? "Type the output here (one line per line of output)" : "Type your answer and press Enter";
        box.SetText(states[(size_t)cur].answer);
        box.focused = states[(size_t)cur].verdict == 0;
    }

    int Score() const { int n = 0; for (auto& s : states) if (s.verdict == 1 || s.verdict == 3) n++; return n; }
    int Done() const { int n = 0; for (auto& s : states) if (s.verdict) n++; return n; }

    void Submit() {
        WState& s = states[(size_t)cur];
        if (s.verdict) return;
        s.answer = box.GetText();
        if (util::Trim(s.answer).empty()) return;
        const Written& w = Data().written[(size_t)s.q];
        bool ok = CheckAnswer(w, s.answer);
        s.verdict = ok ? 1 : 2;
        Prog().RecordAnswer(w.id, ok);
        sfx::Play(ok ? sfx::Correct : sfx::Wrong, 0.8f);
        box.focused = false;
        fb = 0;
    }

    void SelfMark() {
        WState& s = states[(size_t)cur];
        if (s.verdict != 2) return;
        s.verdict = 3;
        // replace the recorded miss with a hit
        ItemStat& st = Prog().items[Data().written[(size_t)s.q].id];
        st.correct++;
        st.lastCorrect = true;
        sfx::Play(sfx::Click);
    }

    void Next() {
        if (cur + 1 < (int)states.size()) { cur++; LoadEditor(); sfx::Play(sfx::Page, 0.6f); }
        else Finish();
    }

    void Finish() {
        if (!finished) { finished = true; Record(); sfx::Play(sfx::Success, 0.6f); }
        summary = true;
    }

    void Record() {
        if (recorded || Done() == 0) return;
        recorded = true;
        Session s;
        s.time = util::NowEpoch(); s.mode = "written"; s.topic = sessionTopic; s.title = title;
        s.score = Score(); s.total = Done(); s.seconds = elapsed;
        for (auto& st : states) if (st.verdict) s.items.push_back({ Data().written[(size_t)st.q].id, st.verdict == 1 || st.verdict == 3 });
        Prog().RecordSession(s);
    }

    void OnClose() override { Record(); Prog().Save(); }

    std::string Remark() override {
        int n = Done();
        if (n == 0) return "Didn't fancy writing today? Fair enough - the quill can be intimidating.";
        float p = (float)Score() / n;
        std::string s = TextFormat("%d of %d written answers correct. ", Score(), n);
        if (p >= 0.85f) return s + "You clearly __read__ code rather than guess at it. Excellent.";
        if (p >= 0.6f) return s + "Good. Tracing output line by line is exactly what the exam rewards.";
        return s + "Tip: trace each variable on scrap paper, one statement at a time - like the memory tables in the Pointers slides.";
    }

    bool HandleEscape() override {
        if (box.focused) { box.focused = false; return true; }
        return false;
    }

    void DebugCommand(const std::string& c) override {
        if (util::StartsWith(c, "type ")) box.SetText(util::Unescape(c.substr(5)));
        else if (c == "submit") Submit();
        else if (c == "next") Next();
        else if (c == "unfocus") box.focused = false;
    }

    void Draw(bool interactive) override {
        page::DrawPaper();
        page::RunningHeader("Written Test  -  " + title, summary ? "Results" : TextFormat("Question %d of %d", cur + 1, (int)states.size()));
        if (page::ReturnRibbon(interactive)) { wantClose = true; return; }
        Rectangle L = page::Left(), R = page::Right();
        if (states.empty()) { page::Heading("No written questions here yet", L.x, L.y + 40, L.width); return; }
        if (summary) { DrawSummary(interactive); return; }

        WState& s = states[(size_t)cur];
        const Written& w = Data().written[(size_t)s.q];
        const Topic* t = Data().FindTopic(w.topic);
        float y = L.y + 6, cw = 0;
        page::Chip(t ? t->name : w.topic, L.x, y, t ? t->color : pal::Wood, 15, &cw);
        page::Chip(KindTitle(w.kind), L.x + cw + 10, y, pal::Blue, 15);
        y += 46;
        Text(FontId::SerifBold, TextFormat("Question %d", cur + 1), L.x, y, 30, pal::Ink);
        y += 46;
        y += Rich(w.question, L.x, y, L.width, 24, pal::Ink, 1.42f) + 14;
        if (!w.code.empty()) {
            float avail = L.y + L.height - 20 - y;
            float h = syntax::DrawCode(w.code, L.x, y, L.width, 18, false, true);
            float size = h > avail ? std::max(11.f, 18.f * avail / h) : 18.f;
            syntax::DrawCode(w.code, L.x, y, L.width, size, true, true);
        }

        // right page
        float ry = R.y + 4;
        Text(FontId::SerifBold, "Your answer", R.x, ry, 26, pal::Ink);
        ry += 38;
        Wrap(FontId::SerifItalic, KindHint(w.kind), R.x, ry, R.width, 17, pal::InkSoft);
        ry += 40;
        float bh = w.multiline ? 220.f : 58.f;
        Rectangle eb{ R.x, ry, R.width, bh };
        box.readOnly = s.verdict != 0;
        box.Draw(eb, interactive, w.multiline ? 19.f : 21.f);
        ry += bh + 16;
        if (s.verdict == 0) {
            Rectangle cb{ R.x, ry, 220, 50 };
            if (Button(cb, "Check answer", BtnPrimary, interactive && !util::Trim(box.GetText()).empty(), 20) || box.submitted) { box.submitted = false; Submit(); }
            if (w.multiline) Text(FontId::SerifItalic, "Tip: press Check when done", R.x + 240, ry + 14, 16, pal::InkFaint);
            Rectangle sk{ R.x + R.width - 130, ry + 6, 130, 38 };
            if (Button(sk, "Skip", BtnGhost, interactive, 17)) {
                s.answer = ""; s.verdict = 2;
                Prog().RecordAnswer(w.id, false);
                fb = 0;
            }
        } else {
            bool ok = s.verdict == 1 || s.verdict == 3;
            float a = util::EaseOutCubic(fb);
            std::string acc = "`" + util::ReplaceAll(w.answers[0], "\n", "` / `") + "`";
            if (w.kind == "output" && w.answers[0].find('\n') != std::string::npos) acc = "(see below)";
            std::string body = (ok ? std::string("") : std::string("Expected: ") + acc + "\n") + (w.why.empty() ? "" : w.why);
            float h = page::Callout(ok ? (s.verdict == 3 ? "Marked correct" : "Correct!") : "Not quite", body, R.x, ry + (1 - a) * 10, R.width,
                                    ok ? pal::Green : pal::Red, ok ? util::WithAlpha(pal::GreenSoft, 0.7f) : util::WithAlpha(pal::RedSoft, 0.7f),
                                    ok ? Icon::Check : Icon::Bulb, true, 19);
            ry += h + 12;
            if (!ok && w.kind == "output" && w.answers[0].find('\n') != std::string::npos) {
                ry += syntax::DrawOutput(w.answers[0], R.x, ry, R.width, 17) + 10;
            }
            if (s.verdict == 2 && !util::Trim(s.answer).empty()) {
                Rectangle sm{ R.x, ry, 300, 30 };
                if (Button(sm, "My answer means the same - mark it correct", BtnLink, interactive, 16)) SelfMark();
            }
            Rectangle nb{ R.x + R.width - 240, R.y + R.height - 54, 240, 52 };
            if (Button(nb, cur + 1 < (int)states.size() ? "Next question" : "See results", BtnPrimary, interactive, 21) ||
                (interactive && !KeyboardCaptured() && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_RIGHT)))) Next();
        }
        // progress dots
        int n = (int)states.size();
        float dot = std::min(26.f, (L.width - 10) / n);
        for (int i = 0; i < n; i++) {
            Vector2 c{ L.x + dot * 0.5f + i * dot, L.y + L.height + 8 };
            int v = states[(size_t)i].verdict;
            Color col = v == 0 ? pal::PaperEdge : (v == 2 ? pal::Red : pal::Green);
            DrawCircleV(c, dot * 0.3f + (i == cur ? 2.f : 0.f), col);
        }
    }

private:
    std::vector<WState> states;
    std::string title, sessionTopic;
    int cur = 0;
    bool finished = false, summary = false, recorded = false;
    float elapsed = 0, fb = 1;
    CodeEditor box;

    void DrawSummary(bool interactive) {
        Rectangle L = page::Left(), R = page::Right();
        int n = Done(), sc = Score();
        float p = n ? (float)sc / n : 0.f;
        float y = L.y + 10;
        Text(FontId::SerifItalic, "Marked in red ink", L.x, y, 22, pal::InkSoft);
        y += 40;
        Text(FontId::SerifBold, TextFormat("%d / %d", sc, n), L.x, y, 92, p >= 0.7f ? pal::Green : (p >= 0.5f ? pal::Amber : pal::Red));
        y += 120;
        Text(FontId::Serif, TextFormat("%d%% correct in %s", (int)(p * 100 + 0.5f), util::FormatDuration(elapsed).c_str()), L.x, y, 21, pal::InkSoft);
        y += 50;
        page::Callout("Exam tip", "In the coding exam you can't run code in your head faster than the compiler - but you **can** spot off-by-one errors, missing `\\0` terminators and uninitialised pointers by tracing. Practise that here.",
                      L.x, y, L.width, pal::Amber, util::WithAlpha(pal::AmberSoft, 0.7f), Icon::Star, true, 18);
        Text(FontId::SerifBold, "Answers", R.x, R.y + 4, 26, pal::Ink);
        float ry = R.y + 50;
        for (int i = 0; i < (int)states.size() && ry < R.y + R.height - 40; i++) {
            const Written& w = Data().written[(size_t)states[(size_t)i].q];
            int v = states[(size_t)i].verdict;
            Color c = v == 0 ? pal::InkFaint : (v == 2 ? pal::Red : pal::Green);
            DrawIcon(v == 0 ? Icon::Dot : (v == 2 ? Icon::Cross : Icon::Check), Vector2{ R.x + 14, ry + 12 }, 18, c);
            std::string qt = util::ReplaceAll(util::ReplaceAll(w.question, "`", ""), "**", "");
            std::string shown = TextFormat("%d. ", i + 1) + qt;
            while (shown.size() > 4 && MeasureWidth(FontId::Serif, shown, 17) > R.width - 50) shown = shown.substr(0, shown.size() - 4) + "...";
            Text(FontId::Serif, shown, R.x + 36, ry + 2, 17, pal::Ink);
            ry += 40;
        }
        (void)interactive;
    }
};

} // namespace

std::unique_ptr<BookView> MakeWrittenView(const std::vector<int>& questions, const std::string& title, const std::string& sessionTopic) {
    return std::make_unique<WrittenView>(questions, title, sessionTopic);
}
