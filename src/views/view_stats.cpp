// view_stats.cpp - "Problem Areas" analysis and the "Past Results" ledger
#include "view.h"
#include <algorithm>

using namespace ui;

namespace {

std::string ModeName(const std::string& m) {
    if (m == "mcq") return "Multiple choice";
    if (m == "written") return "Written test";
    if (m == "coding") return "Coding exercise";
    if (m == "mock") return "Mock coding exam";
    if (m == "drill") return "Weak-spot drill";
    if (m == "reading") return "Reading";
    return m;
}

class StatsView : public BookView {
public:
    explicit StatsView(bool areas) : problemAreas(areas) {}

    std::string Remark() override {
        return problemAreas ? "Knowing where you're weak is half the battle. The other half is homework."
                            : "The ledger never lies. It occasionally exaggerates, but it never lies.";
    }

    void Draw(bool interactive) override {
        page::DrawPaper();
        page::RunningHeader(problemAreas ? "Problem Areas" : "The Ledger", problemAreas ? "Mr. Malloc's assessment" : "Past results");
        if (page::ReturnRibbon(interactive)) { wantClose = true; return; }
        if (problemAreas) DrawAreas(interactive); else DrawLedger(interactive);
    }

private:
    bool problemAreas;
    float scroll = 0;

    void DrawAreas(bool interactive) {
        Rectangle L = page::Left(), R = page::Right();
        const Content& C = Data();
        std::vector<TopicMastery> m = Prog().Mastery();
        std::vector<TopicMastery> assessed, fresh;
        for (auto& t : m) (t.score >= 0 ? assessed : fresh).push_back(t);
        std::sort(assessed.begin(), assessed.end(), [](const TopicMastery& a, const TopicMastery& b) { return a.score < b.score; });

        float y = L.y + 6;
        Text(FontId::SerifBold, "Where you stand", L.x, y, 32, pal::Ink);
        y += 50;
        if (assessed.empty()) {
            y += Wrap(FontId::Serif, "I haven't seen you answer anything yet, so my ledger is blank. Take a multiple choice quiz or a written test and I'll start keeping score.", L.x, y, L.width, 21, pal::InkSoft, 1.45f);
        }
        for (size_t i = 0; i < assessed.size(); i++) {
            const TopicMastery& t = assessed[i];
            const Topic* tp = C.FindTopic(t.topic);
            if (!tp) continue;
            bool weak = i < 3 && t.score < 0.75f;
            if (weak) DrawIcon(Icon::Flag, Vector2{ L.x + 8, y + 12 }, 18, pal::Red);
            Text(weak ? FontId::SerifBold : FontId::Serif, tp->name, L.x + 26, y, 19, pal::Ink);
            std::string detail = TextFormat("%d%%", (int)(t.score * 100 + 0.5f));
            TextRight(FontId::SerifBold, detail, L.x + L.width, y, 19, util::Mix(pal::Red, pal::Green, t.score));
            ProgressBar(Rectangle{ L.x + 26, y + 26, L.width - 26, 8 }, t.score, util::Mix(pal::Red, pal::Green, t.score), pal::PaperEdge);
            std::string sub = TextFormat("%d answers", t.attempts);
            if (t.codeTotal > 0) sub += TextFormat("  -  %d/%d exercises solved", t.codeSolved, t.codeTotal);
            Text(FontId::SerifItalic, sub, L.x + 26, y + 38, 14, pal::InkFaint);
            y += 62;
            if (y > L.y + L.height - 80) break;
        }
        if (!fresh.empty() && y < L.y + L.height - 70) {
            y += 6;
            Text(FontId::SerifBold, "Not yet assessed", L.x, y, 18, pal::InkSoft);
            y += 28;
            std::string names;
            for (auto& t : fresh) { const Topic* tp = C.FindTopic(t.topic); if (tp) { if (!names.empty()) names += ", "; names += tp->name; } }
            Wrap(FontId::SerifItalic, names, L.x, y, L.width, 16, pal::InkFaint);
        }

        // ---------- right page: recommendations ----------
        float ry = R.y + 6;
        Text(FontId::SerifBold, "My prescription", R.x, ry, 32, pal::Ink);
        ry += 52;
        std::vector<std::string> weakTopics;
        for (size_t i = 0; i < assessed.size() && weakTopics.size() < 3; i++) if (assessed[i].score < 0.8f) weakTopics.push_back(assessed[i].topic);
        if (weakTopics.empty() && !assessed.empty()) weakTopics.push_back(assessed[0].topic);
        if (!weakTopics.empty()) {
            const Topic* w = C.FindTopic(weakTopics[0]);
            std::string advice = "Your weakest area is **" + std::string(w ? w->name : weakTopics[0]) + "**. ";
            advice += "Re-read the chapter, then drill the questions you missed - I'll serve those first.";
            ry += page::Callout("Diagnosis", advice, R.x, ry, R.width, pal::Red, util::WithAlpha(pal::RedSoft, 0.55f), Icon::Target, true, 19) + 16;
            Rectangle b1{ R.x, ry, R.width, 54 };
            if (Button(b1, "Drill my weak spots (10 questions)", BtnPrimary, interactive, 20)) { requestAction = "drill"; wantClose = true; }
            ry += 66;
            Rectangle b2{ R.x, ry, R.width, 50 };
            if (Button(b2, "Read: " + std::string(w ? w->name : weakTopics[0]), BtnWood, interactive, 19)) { requestAction = "read"; requestArg = weakTopics[0]; wantClose = true; }
            ry += 64;
        } else {
            ry += page::Callout("Diagnosis", "Nothing to diagnose yet. Come back after a quiz or two.", R.x, ry, R.width, pal::Blue, util::WithAlpha(pal::BlueSoft, 0.6f), Icon::Help, true, 19) + 16;
        }
        // stubborn questions
        struct Miss { std::string id; float acc; int att; };
        std::vector<Miss> misses;
        for (auto& kv : Prog().items) {
            if (kv.second.attempts == 0) continue;
            float acc = (float)kv.second.correct / kv.second.attempts;
            if (!kv.second.lastCorrect || acc < 0.5f) misses.push_back(Miss{ kv.first, acc, kv.second.attempts });
        }
        std::sort(misses.begin(), misses.end(), [](const Miss& a, const Miss& b) { return a.acc < b.acc || (a.acc == b.acc && a.att > b.att); });
        if (!misses.empty() && ry < R.y + R.height - 100) {
            Text(FontId::SerifBold, "Questions that keep catching you", R.x, ry, 20, pal::Ink);
            ry += 34;
            for (size_t i = 0; i < misses.size() && ry < R.y + R.height - 30; i++) {
                std::string text;
                for (auto& q : C.mcqs) if (q.id == misses[i].id) text = q.question;
                if (text.empty()) for (auto& w : C.written) if (w.id == misses[i].id) text = w.question;
                if (text.empty()) continue;
                text = util::ReplaceAll(util::ReplaceAll(util::ReplaceAll(text, "`", ""), "**", ""), "\n", " ");
                while (text.size() > 4 && MeasureWidth(FontId::Serif, text, 16) > R.width - 70) text = text.substr(0, text.size() - 4) + "...";
                DrawIcon(Icon::Cross, Vector2{ R.x + 10, ry + 10 }, 14, pal::Red);
                Text(FontId::Serif, text, R.x + 28, ry, 16, pal::Ink);
                TextRight(FontId::SerifItalic, TextFormat("%d/%d", (int)(misses[i].acc * misses[i].att + 0.5f), misses[i].att), R.x + R.width, ry, 15, pal::InkSoft);
                ry += 28;
            }
        }
    }

    void DrawLedger(bool interactive) {
        Rectangle L = page::Left(), R = page::Right();
        const auto& ss = Prog().sessions;
        float y = L.y + 6;
        Text(FontId::SerifBold, "Session history", L.x, y, 32, pal::Ink);
        y += 52;
        // table header
        Text(FontId::SerifBold, "Date", L.x, y, 16, pal::InkSoft);
        Text(FontId::SerifBold, "Activity", L.x + 170, y, 16, pal::InkSoft);
        TextRight(FontId::SerifBold, "Score", L.x + L.width, y, 16, pal::InkSoft);
        y += 26;
        DrawLineEx(Vector2{ L.x, y }, Vector2{ L.x + L.width, y }, 1.5f, pal::InkFaint);
        y += 8;
        Rectangle list{ L.x - 4, y, L.width + 8, L.y + L.height - y };
        if (ss.empty()) Wrap(FontId::SerifItalic, "No entries yet. Every quiz, test, reading session and coding attempt is written here in my best copperplate.", L.x, y + 10, L.width, 19, pal::InkSoft);
        PushClip(list);
        float rowH = 44;
        float contentH = ss.size() * rowH;
        if (interactive && Hover(list)) scroll -= Wheel() * 60;
        scroll = std::max(0.f, std::min(scroll, std::max(0.f, contentH - list.height)));
        for (int i = 0; i < (int)ss.size(); i++) {
            const Session& s = ss[ss.size() - 1 - (size_t)i];
            float ry = y + i * rowH - scroll;
            if (ry < y - rowH || ry > list.y + list.height) continue;
            if (i % 2 == 0) DrawRectangleRec(Rectangle{ L.x - 4, ry - 4, L.width + 8, rowH }, util::WithAlpha(pal::PaperDark, 0.35f));
            Text(FontId::Serif, util::FormatDate(s.time), L.x, ry + 6, 14, pal::InkSoft);
            std::string act = ModeName(s.mode) + ": " + s.title;
            while (act.size() > 4 && MeasureWidth(FontId::Serif, act, 16) > L.width - 260) act = act.substr(0, act.size() - 4) + "...";
            Text(FontId::Serif, act, L.x + 170, ry + 5, 16, pal::Ink);
            std::string sc = s.mode == "reading" ? TextFormat("%d pages", s.score) : TextFormat("%d/%d", s.score, s.total);
            float p = s.total ? (float)s.score / s.total : 0;
            TextRight(FontId::SerifBold, sc, L.x + L.width, ry + 5, 16, s.mode == "reading" ? pal::InkSoft : util::Mix(pal::Red, pal::Green, p));
        }
        PopClip();

        // ---------- right page: summary ----------
        float ry = R.y + 6;
        Text(FontId::SerifBold, "Summary", R.x, ry, 32, pal::Ink);
        ry += 56;
        int quizzes = 0, totalQ = 0, correctQ = 0;
        for (auto& s : ss) if (s.mode == "mcq" || s.mode == "written" || s.mode == "drill") { quizzes++; totalQ += s.total; correctQ += s.score; }
        const Content& C = Data();
        auto stat = [&](const std::string& label, const std::string& value, Color c) {
            Text(FontId::Serif, label, R.x, ry, 19, pal::InkSoft);
            TextRight(FontId::SerifBold, value, R.x + R.width, ry, 21, c);
            ry += 36;
        };
        stat("Quizzes & tests taken", std::to_string(quizzes), pal::Ink);
        stat("Questions answered", std::to_string(totalQ), pal::Ink);
        stat("Accuracy", totalQ ? TextFormat("%d%%", (int)(100.f * correctQ / totalQ + 0.5f)) : "-", totalQ ? util::Mix(pal::Red, pal::Green, (float)correctQ / totalQ) : pal::Ink);
        for (const ProblemSet& set : C.sets) {
            int solved = 0;
            for (int pi : set.problems) if (Prog().Code(C.problems[(size_t)pi].id).solved) solved++;
            stat(set.title, TextFormat("%d / %d solved", solved, (int)set.problems.size()), solved == (int)set.problems.size() && solved > 0 ? pal::Green : pal::Ink);
        }
        // study streak (distinct days)
        std::vector<long long> days;
        for (auto& s : ss) days.push_back(s.time / 86400);
        std::sort(days.begin(), days.end());
        days.erase(std::unique(days.begin(), days.end()), days.end());
        stat("Days of study", std::to_string(days.size()), pal::Ink);
        ry += 10;
        // trend chart of the last 12 scored sessions
        std::vector<float> pts;
        for (auto& s : ss) if (s.total > 0 && s.mode != "reading") pts.push_back((float)s.score / s.total);
        if (pts.size() > 12) pts.erase(pts.begin(), pts.end() - 12);
        Rectangle chart{ R.x, ry + 48, R.width, std::min(340.f, std::max(120.f, R.y + R.height - ry - 70)) };
        Text(FontId::SerifBold, "Recent scores", R.x, ry, 19, pal::Ink);
        DrawRectangleRec(chart, util::WithAlpha(pal::PaperDark, 0.4f));
        for (int g = 0; g <= 4; g++) {
            float gy = chart.y + chart.height - chart.height * g / 4.f;
            DrawLineEx(Vector2{ chart.x, gy }, Vector2{ chart.x + chart.width, gy }, 1.f, util::WithAlpha(pal::InkFaint, 0.35f));
            Text(FontId::Serif, TextFormat("%d%%", g * 25), chart.x + 4, gy - 16, 12, pal::InkFaint);
        }
        if (pts.size() >= 1) {
            float step = pts.size() > 1 ? (chart.width - 60) / (pts.size() - 1) : 0;
            Vector2 prev{};
            for (size_t i = 0; i < pts.size(); i++) {
                Vector2 p{ chart.x + 40 + step * i, chart.y + chart.height - pts[i] * (chart.height - 14) - 7 };
                if (i) DrawLineEx(prev, p, 3, pal::Blue);
                prev = p;
            }
            for (size_t i = 0; i < pts.size(); i++) {
                Vector2 p{ chart.x + 40 + step * i, chart.y + chart.height - pts[i] * (chart.height - 14) - 7 };
                DrawCircleV(p, 6, pal::Paper);
                DrawCircleV(p, 4.5f, util::Mix(pal::Red, pal::Green, pts[i]));
            }
        } else {
            TextCentered(FontId::SerifItalic, "Your score curve will appear here", chart, 17, pal::InkFaint);
        }
    }
};

} // namespace

std::unique_ptr<BookView> MakeStatsView(bool problemAreas) { return std::make_unique<StatsView>(problemAreas); }
