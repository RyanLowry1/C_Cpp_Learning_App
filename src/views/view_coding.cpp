// view_coding.cpp - LeetCode-style coding workbook: problems, editor, compile & test, mock exams
#include "view.h"
#include "../audio.h"
#include "../code_editor.h"
#include "../code_runner.h"
#include "../harness.h"
#include <algorithm>
#include <random>

using namespace ui;

namespace {

enum class Screen { List, Problem, MockResult };

class CodingView : public BookView {
public:
    CodingView(int set, bool isMock, const std::string& openId) : setIdx(set), mock(isMock) {
        const ProblemSet& ps = Data().sets[(size_t)setIdx];
        probs = ps.problems;
        if (mock) PickMockProblems();
        runner::DetectAsync();
        if (!openId.empty()) {
            for (int i = 0; i < (int)probs.size(); i++) if (Data().problems[(size_t)probs[(size_t)i]].id == openId) OpenProblem(i);
        }
    }

    std::string SessionKey() const override { return Data().sets[(size_t)setIdx].id; }

    void Tick(float dt) override {
        time += dt;
        if (saveTimer > 0) { saveTimer -= dt; if (saveTimer <= 0) SaveDraft(); }
        if (mock && screen != Screen::MockResult) {
            mockLeft -= dt;
            if (mockLeft <= 0) { mockLeft = 0; FinishMock(); }
        }
        if (job && job->done && !jobHandled) HandleJobDone();
    }

    void OnClose() override {
        SaveDraft();
        if (mock && screen != Screen::MockResult) FinishMock();
        if (!mock && attemptedThisVisit > 0) {
            Session s;
            s.time = util::NowEpoch(); s.mode = "coding"; s.topic = SessionKey();
            s.title = Data().sets[(size_t)setIdx].title;
            s.score = solvedThisVisit; s.total = attemptedThisVisit;
            for (auto& kv : visitResults) s.items.push_back({ kv.first, kv.second });
            Prog().RecordSession(s);
        }
        Prog().Save();
    }

    std::string Remark() override {
        if (mock) {
            int p = mockTotalAll ? (int)(100.f * mockPassedAll / mockTotalAll + 0.5f) : 0;
            if (p >= 80) return TextFormat("Mock exam: %d%% of tests passed. If the real one goes like that, you've nothing to fear.", p);
            if (p >= 50) return TextFormat("Mock exam: %d%%. A respectable pass - now polish the ones that got away.", p);
            return TextFormat("Mock exam: %d%%. Under pressure the basics slip. Practise those exercises untimed first.", p);
        }
        if (attemptedThisVisit == 0) return "No submissions today? Reading a problem is the first step. Typing is the second.";
        if (solvedThisVisit == attemptedThisVisit) return TextFormat("%d solved, every test green. The compiler and I are both impressed.", solvedThisVisit);
        return TextFormat("You solved %d of the %d problems you submitted. The rest will keep - your drafts are saved.", solvedThisVisit, attemptedThisVisit);
    }

    void DebugCommand(const std::string& c) override {
        if (util::StartsWith(c, "open ")) OpenProblem(atoi(c.c_str() + 5));
        else if (c == "solution" && cur >= 0) ed.SetText(P().solution);
        else if (c == "starter" && cur >= 0) ed.SetText(P().starter);
        else if (c == "run") StartJob(false);
        else if (c == "submit") StartJob(true);
        else if (c == "unfocus") ed.focused = false;
        else if (util::StartsWith(c, "tab ")) resultTab = atoi(c.c_str() + 4);
        else if (util::StartsWith(c, "type ")) { ed.SetText(util::Unescape(c.substr(5))); }
    }

    bool HandleEscape() override {
        if (ed.focused) { ed.focused = false; return true; }
        if (showSolution) { showSolution = false; return true; }
        if (screen == Screen::Problem) { SaveDraft(); screen = Screen::List; return true; }
        return false;
    }

    void Draw(bool interactive) override {
        page::DrawPaper();
        const ProblemSet& ps = Data().sets[(size_t)setIdx];
        std::string right = mock ? ("Time left " + util::FormatDuration(mockLeft)) : ps.title;
        page::RunningHeader(mock ? "Mock Coding Exam" : "Coding Workbook", right);
        if (page::ReturnRibbon(interactive)) { wantClose = true; return; }
        switch (screen) {
            case Screen::List: DrawList(interactive); break;
            case Screen::Problem: DrawProblem(interactive); break;
            case Screen::MockResult: DrawMockResult(interactive); break;
        }
    }

private:
    int setIdx;
    bool mock;
    std::vector<int> probs;
    Screen screen = Screen::List;
    int cur = -1;
    CodeEditor ed;
    std::shared_ptr<runner::Job> job;
    bool jobHandled = true, jobSubmit = false;
    BuiltProgram built;
    std::vector<std::string> ruleMsgs;
    bool hasResults = false;
    int resultTab = 0;
    bool hintShown = false, showSolution = false, confirmSolution = false, confirmReset = false;
    float descScroll = 0, listScroll = 0, resultScroll = 0;
    float saveTimer = 0;
    float runStarted = 0;
    // mock exam
    float mockLeft = 3600.f;
    std::map<int, int> mockPassed, mockTotal;
    int mockPassedAll = 0, mockTotalAll = 0;
    // visit stats
    int attemptedThisVisit = 0, solvedThisVisit = 0;
    std::map<std::string, bool> visitResults;

    const Problem& P() const { return Data().problems[(size_t)probs[(size_t)cur]]; }

    void PickMockProblems() {
        std::mt19937 rng((unsigned)util::NowEpoch());
        std::vector<int> easy, med, hard;
        for (int p : probs) {
            std::string d = util::ToLower(Data().problems[(size_t)p].diff);
            (d == "easy" ? easy : (d == "medium" ? med : hard)).push_back(p);
        }
        std::shuffle(easy.begin(), easy.end(), rng);
        std::shuffle(med.begin(), med.end(), rng);
        std::shuffle(hard.begin(), hard.end(), rng);
        std::vector<int> pick;
        for (int i = 0; i < 2 && i < (int)easy.size(); i++) pick.push_back(easy[(size_t)i]);
        for (int i = 0; i < 2 && i < (int)med.size(); i++) pick.push_back(med[(size_t)i]);
        for (int i = 0; i < 1 && i < (int)hard.size(); i++) pick.push_back(hard[(size_t)i]);
        std::vector<int> rest;
        for (int p : probs) if (std::find(pick.begin(), pick.end(), p) == pick.end()) rest.push_back(p);
        std::shuffle(rest.begin(), rest.end(), rng);
        while (pick.size() < 5 && !rest.empty()) { pick.push_back(rest.back()); rest.pop_back(); }
        probs = pick;
        for (int p : probs) { mockPassed[p] = 0; mockTotal[p] = (int)Data().problems[(size_t)p].tests.size(); }
    }

    void OpenProblem(int i) {
        SaveDraft();
        cur = i;
        screen = Screen::Problem;
        const Problem& p = P();
        std::string draft = mock ? std::string() : Prog().LoadDraft(p.id);
        ed = CodeEditor();
        ed.SetText(draft.empty() ? p.starter : draft);
        ed.focused = true;
        hasResults = false; ruleMsgs.clear(); hintShown = false; showSolution = false; confirmSolution = false; confirmReset = false;
        descScroll = 0; resultScroll = 0; resultTab = 0;
        if (job && !job->done) jobHandled = true;   // ignore results of a job for another problem
        sfx::Play(sfx::Page, 0.6f);
    }

    void SaveDraft() {
        if (cur < 0 || mock) return;
        Prog().SaveDraft(P().id, ed.GetText());
        saveTimer = 0;
    }

    void StartJob(bool submit) {
        if (cur < 0 || (job && !job->done)) return;
        SaveDraft();
        const Problem& p = P();
        std::string code = ed.GetText();
        ruleMsgs = CheckRules(p, code);
        built = BuildProgram(p, code, !submit);
        job = std::make_shared<runner::Job>();
        job->source = built.source;
        job->testCount = (int)built.testIndex.size();
        job->perTestTimeout = 3.0f;
        jobSubmit = submit;
        jobHandled = false;
        runStarted = time;
        ed.markers.clear();
        runner::Run(job);
        sfx::Play(sfx::Click);
    }

    void HandleJobDone() {
        jobHandled = true;
        hasResults = true;
        resultScroll = 0;
        ed.markers.clear();
        for (auto& d : job->diags)
            if (d.file == "your_code.cpp" && d.line > 0) ed.markers.push_back(CodeEditor::Marker{ d.line, d.message, d.error });
        if (!job->compiled || !job->fatal.empty()) { resultTab = 2; sfx::Play(sfx::Wrong, 0.7f); return; }
        resultTab = 0;
        int passed = 0;
        for (auto& r : job->results) if (r.status == runner::TestStatus::Pass) passed++;
        int total = (int)job->results.size();
        bool rulesOk = ruleMsgs.empty();
        if (jobSubmit) {
            const Problem& p = P();
            int recPassed = rulesOk ? passed : std::min(passed, total - 1);
            if (mock) {
                int pi = probs[(size_t)cur];
                mockPassed[pi] = std::max(mockPassed[pi], recPassed);
            } else {
                bool wasSolved = Prog().Code(p.id).solved;
                Prog().RecordCode(p.id, recPassed, total);
                if (!visitResults.count(p.id)) attemptedThisVisit++;
                bool nowSolved = recPassed == total;
                if (nowSolved && (!visitResults.count(p.id) || !visitResults[p.id])) solvedThisVisit++;
                visitResults[p.id] = nowSolved || (visitResults.count(p.id) && visitResults[p.id]);
                (void)wasSolved;
            }
            Prog().RecordAnswer(p.id, recPassed == total);
        }
        bool all = passed == total && total > 0 && rulesOk;
        sfx::Play(all ? (jobSubmit ? sfx::Success : sfx::Correct) : sfx::Wrong, 0.75f);
    }

    void FinishMock() {
        if (screen == Screen::MockResult) return;
        SaveDraft();
        mockPassedAll = 0; mockTotalAll = 0;
        Session s;
        s.time = util::NowEpoch(); s.mode = "mock"; s.topic = SessionKey();
        s.title = "Mock: " + Data().sets[(size_t)setIdx].title;
        for (int p : probs) {
            mockPassedAll += mockPassed[p]; mockTotalAll += mockTotal[p];
            s.items.push_back({ Data().problems[(size_t)p].id, mockPassed[p] == mockTotal[p] });
        }
        s.score = mockPassedAll; s.total = mockTotalAll; s.seconds = 3600.f - mockLeft;
        Prog().RecordSession(s);
        screen = Screen::MockResult;
        sfx::Play(sfx::Bell, 0.6f);
    }

    // ------------------------------------------------------------------ list
    void DrawList(bool interactive) {
        const ProblemSet& ps = Data().sets[(size_t)setIdx];
        Rectangle L = page::Left(), R = page::Right();
        float y = L.y + 6;
        y += Wrap(FontId::SerifBold, mock ? "Mock exam: " + ps.title : ps.title, L.x, y, L.width, 34, pal::Ink, 1.15f) + 6;
        if (!ps.subtitle.empty()) y += Wrap(FontId::SerifItalic, ps.subtitle, L.x, y, L.width, 19, pal::InkSoft) + 12;
        DrawRectangle((int)L.x, (int)y, 110, 4, ps.color);
        y += 22;
        if (mock) {
            y += Rich("You have **60 minutes** for these " + std::to_string(probs.size()) + " problems - exactly like the lab exam. Only **Submit** counts. Hidden tests decide the mark, so read each description carefully.", L.x, y, L.width, 19, pal::Ink) + 14;
            Text(FontId::SerifBold, util::FormatDuration(mockLeft), L.x, y, 64, mockLeft < 300 ? pal::Red : pal::Ink);
            y += 84;
            int done = 0;
            for (int p : probs) if (mockPassed[p] == mockTotal[p] && mockTotal[p] > 0) done++;
            Text(FontId::Serif, TextFormat("%d of %d problems fully passing", done, (int)probs.size()), L.x, y, 19, pal::InkSoft);
            y += 40;
            Rectangle fin{ L.x, y, 260, 50 };
            if (Button(fin, "Finish exam now", BtnDanger, interactive, 19)) FinishMock();
            y += 70;
        } else {
            y += Rich(ps.desc, L.x, y, L.width, 18.5f, pal::Ink, 1.45f) + 16;
            int solved = 0;
            for (int p : probs) if (Prog().Code(Data().problems[(size_t)p].id).solved) solved++;
            Text(FontId::SerifBold, TextFormat("%d / %d solved", solved, (int)probs.size()), L.x, y, 24, pal::Ink);
            y += 36;
            ProgressBar(Rectangle{ L.x, y, L.width, 10 }, probs.empty() ? 0 : (float)solved / probs.size(), pal::Green, pal::PaperEdge);
            y += 34;
            // per-topic progress
            std::map<std::string, std::pair<int, int>> byTopic;
            for (int p : probs) {
                const Problem& pr = Data().problems[(size_t)p];
                auto& e = byTopic[pr.topic];
                e.second++;
                if (Prog().Code(pr.id).solved) e.first++;
            }
            for (auto& kv : byTopic) {
                const Topic* t = Data().FindTopic(kv.first);
                Text(FontId::Serif, t ? t->name : kv.first, L.x, y, 17, pal::InkSoft);
                TextRight(FontId::Serif, TextFormat("%d / %d", kv.second.first, kv.second.second), L.x + L.width, y, 17, pal::InkSoft);
                ProgressBar(Rectangle{ L.x, y + 24, L.width, 5 }, (float)kv.second.first / kv.second.second, t ? t->color : pal::Wood, util::WithAlpha(pal::PaperEdge, 0.7f));
                y += 40;
            }
            if (ps.examNo > 0) {
                y += 10;
                Rectangle mb{ L.x, y, 330, 50 };
                if (Button(mb, "Sit a timed mock exam", BtnDanger, interactive, 19)) { requestAction = "mock"; requestArg = ps.id; wantClose = true; }
                Text(FontId::SerifItalic, "5 problems, 60 minutes", L.x + 346, y + 15, 16, pal::InkSoft);
            }
        }
        // compiler status
        runner::CompilerInfo ci = runner::Compiler();
        std::string status = ci.detecting ? "Looking for a C++ compiler..." : (ci.kind == runner::CompilerKind::None ? ci.error : "Compiler: " + ci.name);
        Color sc = ci.detecting ? pal::Amber : (ci.kind == runner::CompilerKind::None ? pal::Red : pal::Green);
        float sy = L.y + L.height - 64;
        DrawIcon(ci.kind == runner::CompilerKind::None && !ci.detecting ? Icon::Cross : Icon::Check, Vector2{ L.x + 10, sy + 12 }, 16, sc);
        Wrap(FontId::SerifItalic, status, L.x + 26, sy, L.width - 26, 16, sc);
        Wrap(FontId::SerifItalic, "Your code is compiled as C++17 and run against the tests on this machine.", L.x, sy + 30, L.width, 14, pal::InkFaint);

        // right page: problems
        Text(FontId::SerifBold, mock ? "Exam paper" : "Problems", R.x, R.y + 4, 28, pal::Ink);
        float ly = R.y + 52;
        Rectangle area{ R.x - 6, ly, R.width + 12, R.height - 52 };
        float rowH = 66;
        float contentH = probs.size() * rowH;
        if (interactive && Hover(area)) listScroll -= Wheel() * 70;
        listScroll = std::max(0.f, std::min(listScroll, std::max(0.f, contentH - area.height)));
        PushClip(area);
        for (int i = 0; i < (int)probs.size(); i++) {
            const Problem& p = Data().problems[(size_t)probs[(size_t)i]];
            float ry = ly + i * rowH - listScroll;
            if (ry < ly - rowH || ry > area.y + area.height) continue;
            Rectangle row{ R.x, ry, R.width, rowH - 8 };
            bool hov = interactive && Hover(row);
            DrawRectangleRounded(row, 0.18f, 6, hov ? util::Shade(pal::PaperDark, 1.02f) : util::WithAlpha(pal::PaperDark, 0.45f));
            if (hov) DrawRectangleRoundedLinesEx(row, 0.18f, 6, 1.5f, pal::Wood);
            CodeStat cs = Prog().Code(p.id);
            bool solved = mock ? (mockPassed[probs[(size_t)i]] == mockTotal[probs[(size_t)i]] && mockTotal[probs[(size_t)i]] > 0) : cs.solved;
            bool tried = mock ? mockPassed[probs[(size_t)i]] > 0 : cs.attempts > 0;
            Color stc = solved ? pal::Green : (tried ? pal::Amber : pal::InkFaint);
            DrawCircleV(Vector2{ row.x + 24, row.y + row.height * 0.5f }, 13, util::WithAlpha(stc, 0.2f));
            DrawIcon(solved ? Icon::Check : (tried ? Icon::Refresh : Icon::Dot), Vector2{ row.x + 24, row.y + row.height * 0.5f }, 16, stc);
            Text(FontId::SerifBold, TextFormat("%d. %s", i + 1, p.title.c_str()), row.x + 50, row.y + 8, 19, pal::Ink);
            const Topic* t = Data().FindTopic(p.topic);
            std::string sub = t ? t->name : p.topic;
            if (!mock && cs.attempts > 0) sub += TextFormat("  -  best %d/%d tests", cs.bestPassed, cs.total);
            if (mock) sub += TextFormat("  -  %d/%d tests", mockPassed[probs[(size_t)i]], mockTotal[probs[(size_t)i]]);
            Text(FontId::SerifItalic, sub, row.x + 50, row.y + 33, 15, pal::InkSoft);
            float cw = MeasureWidth(FontId::SerifBold, p.diff, 14) + 18;
            page::Chip(p.diff, row.x + row.width - cw - 10, row.y + 16, page::DiffColor(p.diff), 14);
            if (interactive && Pressed(row)) OpenProblem(i);
        }
        PopClip();
    }

    // --------------------------------------------------------------- problem
    void DrawProblem(bool interactive) {
        const Problem& p = P();
        Rectangle L = page::Left(), R = page::Right();
        if (interactive && !KeyboardCaptured() && IsKeyPressed(KEY_BACKSPACE)) { SaveDraft(); screen = Screen::List; return; }
        Rectangle back{ L.x - 6, L.y - 8, 150, 30 };
        if (Button(back, "< All problems", BtnLink, interactive, 16)) { SaveDraft(); screen = Screen::List; return; }

        // ---- left page: description (scrollable) ----
        Rectangle area{ L.x - 4, L.y + 26, L.width + 8, L.height - 26 };
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (interactive && Hover(area)) descScroll -= Wheel() * 60;
        PushClip(area);
        float y0 = area.y + 6 - descScroll;
        float y = y0;
        y += Wrap(FontId::SerifBold, p.title, L.x, y, L.width, 32, pal::Ink, 1.15f) + 8;
        float cx = L.x, w = 0;
        page::Chip(p.diff, cx, y, page::DiffColor(p.diff), 15, &w); cx += w + 8;
        const Topic* t = Data().FindTopic(p.topic);
        if (t) { page::Chip(t->name, cx, y, t->color, 15, &w); cx += w + 8; }
        CodeStat cs = Prog().Code(p.id);
        if (!mock && cs.solved) page::Chip("Solved", cx, y, pal::Green, 15);
        y += 42;
        y += Rich(p.desc, L.x, y, L.width, 19, pal::Ink, 1.45f) + 12;
        if (!p.example.empty()) {
            Text(FontId::SerifBold, "Example", L.x, y, 20, pal::Wood);
            y += 30;
            y += syntax::DrawCode(p.example, L.x, y, L.width, 16) + 14;
        }
        for (auto& f : p.forbid) {
            y += page::Callout("Exam rule", f.message, L.x, y, L.width, pal::Red, util::WithAlpha(pal::RedSoft, 0.55f), Icon::Lock, true, 17) + 10;
        }
        if (!p.support.empty()) {
            Text(FontId::SerifBold, "Code provided for you", L.x, y, 20, pal::Wood);
            y += 30;
            y += syntax::DrawCode(p.support, L.x, y, L.width, 15) + 14;
        }
        // visible tests
        Text(FontId::SerifBold, "Sample tests (Run)", L.x, y, 20, pal::Wood);
        y += 30;
        int hidden = 0;
        for (auto& tc : p.tests) {
            if (!tc.sample) { hidden++; continue; }
            DrawIcon(Icon::Play, Vector2{ L.x + 8, y + 11 }, 12, pal::Wood);
            y += Rich(tc.label, L.x + 22, y, L.width - 22, 17, pal::Ink) + 2;
            std::string code = util::Trim(tc.code);
            if (!code.empty()) y += syntax::DrawCode(code, L.x + 22, y, L.width - 22, 14, true, false, 10) + 8;
        }
        if (hidden) y += Wrap(FontId::SerifItalic, TextFormat("+ %d hidden tests run when you Submit.", hidden), L.x, y, L.width, 16, pal::InkSoft) + 12;
        if (!p.hint.empty()) {
            if (hintShown) y += page::Callout("Hint", p.hint, L.x, y, L.width, pal::Green, util::WithAlpha(pal::GreenSoft, 0.55f), Icon::Bulb, true, 17) + 10;
            else {
                Rectangle hb{ L.x, y, 180, 38 };
                if (Button(hb, "Show a hint", BtnGhost, interactive, 17)) hintShown = true;
                y += 50;
            }
        }
        float contentH = y - y0 + 20;
        PopClip();
        descScroll = std::max(0.f, std::min(descScroll, std::max(0.f, contentH - area.height)));
        if (contentH > area.height) {
            float t2 = descScroll / std::max(1.f, contentH - area.height);
            float th = std::max(30.f, area.height * area.height / contentH);
            DrawRectangleRounded(Rectangle{ L.x + L.width + 14, area.y + (area.height - th) * t2, 5, th }, 0.8f, 4, util::WithAlpha(pal::InkFaint, 0.6f));
        }

        // ---- right page: toolbar ----
        bool running = job && !job->done && !jobHandled;
        bool locked = mock && screen == Screen::MockResult;
        float bx = R.x;
        Rectangle runB{ bx, R.y - 6, 150, 44 };
        if (Button(runB, "Run", BtnWood, interactive && !running && !locked, 18) || (interactive && ctrl && !IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_ENTER) && !running)) StartJob(false);
        DrawIcon(Icon::Play, Vector2{ runB.x + 24, runB.y + 22 }, 14, pal::GoldSoft);
        Rectangle subB{ bx + 160, R.y - 6, 150, 44 };
        if (Button(subB, "Submit", BtnPrimary, interactive && !running && !locked, 18) || (interactive && ctrl && IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_ENTER) && !running)) StartJob(true);
        DrawIcon(Icon::Check, Vector2{ subB.x + 24, subB.y + 22 }, 15, pal::GoldSoft);
        Rectangle resB{ bx + 320, R.y - 6, 110, 44 };
        if (Button(resB, confirmReset ? "Sure?" : "Reset", confirmReset ? BtnDanger : BtnPaper, interactive && !running, 17)) {
            if (confirmReset) { ed.SetText(p.starter); ed.markers.clear(); confirmReset = false; SaveDraft(); }
            else confirmReset = true;
        }
        if (!mock) {
            Rectangle solB{ bx + 440, R.y - 6, R.width - 440, 44 };
            if (Button(solB, showSolution ? "Hide solution" : "Solution", BtnGhost, interactive && !p.solution.empty(), 17)) {
                if (showSolution) showSolution = false;
                else if (confirmSolution) { showSolution = true; confirmSolution = false; }
                else confirmSolution = true;
            }
        }
        if (interactive && ctrl && IsKeyPressed(KEY_S)) { SaveDraft(); sfx::Play(sfx::Click); }

        // ---- editor ----
        float edTop = R.y + 50;
        float panelH = 268;
        Rectangle edR{ R.x, edTop, R.width, R.height - 50 - panelH - 12 };
        ed.readOnly = locked;
        ed.Draw(edR, interactive && !showSolution, 16.5f);
        if (ed.changed) { ed.changed = false; saveTimer = 1.5f; if (!running) ed.markers.clear(); confirmReset = false; }

        // ---- results panel ----
        Rectangle panel{ R.x, edR.y + edR.height + 20, R.width, panelH - 8 };
        DrawResults(panel, interactive, running);

        // ---- overlays ----
        if (confirmSolution && !showSolution) {
            Rectangle box{ L.x, L.y + L.height - 130, L.width, 120 };
            Panel(box, pal::AmberSoft, pal::Amber, 0.1f, 2.f);
            Wrap(FontId::Serif, "Peek at the model solution? You learn far more by struggling first - try a hint, then come back. Click Solution again to reveal it.",
                 box.x + 16, box.y + 14, box.width - 32, 18, pal::Ink, 1.35f);
        }
        if (showSolution) {
            Rectangle box{ L.x - 10, L.y + 20, L.width + 20, L.height - 20 };
            DrawRectangleRounded(Rectangle{ box.x + 4, box.y + 6, box.width, box.height }, 0.03f, 6, util::WithAlpha(BLACK, 0.2f));
            Panel(box, pal::Paper, pal::Wood, 0.03f, 2.f);
            Text(FontId::SerifBold, "Model solution", box.x + 20, box.y + 16, 24, pal::Ink);
            float sh = syntax::DrawCode(p.solution, box.x + 20, box.y + 56, box.width - 40, 15, false, true, 10);
            float avail = box.height - 140;
            syntax::DrawCode(p.solution, box.x + 20, box.y + 56, box.width - 40, sh > avail ? std::max(9.f, 15.f * avail / sh) : 15.f, true, true, 9);
            Rectangle cp{ box.x + 20, box.y + box.height - 62, 240, 46 };
            if (Button(cp, "Copy into my editor", BtnWood, interactive, 17)) { ed.SetText(p.solution); SaveDraft(); showSolution = false; }
            Rectangle cl{ box.x + box.width - 140, box.y + box.height - 62, 120, 46 };
            if (Button(cl, "Close", BtnPaper, interactive, 17)) showSolution = false;
        }
    }

    void DrawResults(Rectangle panel, bool interactive, bool running) {
        DrawRectangleRounded(panel, 0.04f, 6, util::WithAlpha(pal::PaperDark, 0.55f));
        DrawRectangleRoundedLinesEx(panel, 0.04f, 6, 1.2f, util::WithAlpha(pal::InkFaint, 0.6f));
        const char* tabs[3] = { "Tests", "Output", "Compiler" };
        for (int i = 0; i < 3; i++) {
            Rectangle tr{ panel.x + 10 + i * 112, panel.y - 16, 104, 30 };
            bool sel = resultTab == i;
            Panel(tr, sel ? pal::Paper : pal::PaperDark, util::WithAlpha(pal::InkSoft, sel ? 0.8f : 0.4f), 0.4f, 1.2f);
            TextCentered(sel ? FontId::SerifBold : FontId::Serif, tabs[i], tr, 15, sel ? pal::Ink : pal::InkSoft);
            if (interactive && Pressed(tr)) { resultTab = i; resultScroll = 0; }
        }
        Rectangle in{ panel.x + 12, panel.y + 22, panel.width - 24, panel.height - 30 };
        if (running) {
            float a = time * 4.f;
            Vector2 c{ in.x + 24, in.y + 30 };
            DrawRing(c, 12, 16, a * RAD2DEG, a * RAD2DEG + 260, 24, pal::Wood);
            runner::CompilerInfo ci = runner::Compiler();
            Text(FontId::Serif, ci.detecting ? "Finding a compiler..." : "Compiling and running your code...", in.x + 52, in.y + 18, 19, pal::Ink);
            Text(FontId::SerifItalic, TextFormat("%.1fs  -  the first compile can take a few seconds", time - runStarted), in.x + 52, in.y + 46, 15, pal::InkSoft);
            return;
        }
        if (!hasResults || !job) {
            float y = in.y + 10;
            y += Rich("Press **Run** to try the sample tests, or **Submit** to run every test (including the hidden ones). Results appear here.", in.x, y, in.width, 17, pal::InkSoft) + 14;
            const char* keys[] = { "Ctrl+Enter  -  Run", "Ctrl+Shift+Enter  -  Submit", "Ctrl+/  -  comment line", "Ctrl+Z / Ctrl+Y  -  undo / redo", "Tab / Shift+Tab  -  indent", "Esc  -  leave the editor" };
            for (int i = 0; i < 6; i++) Text(FontId::Serif, keys[i], in.x + (i % 2) * (in.width * 0.5f), y + (i / 2) * 24, 15, pal::InkFaint);
            return;
        }
        if (interactive && Hover(in)) resultScroll -= Wheel() * 50;
        PushClip(in);
        float y = in.y + 6 - resultScroll;
        float y0 = y;
        if (!job->fatal.empty()) {
            y += Wrap(FontId::SerifBold, job->fatal, in.x, y, in.width, 17, pal::Red) + 6;
        } else if (resultTab == 0) {
            if (!job->compiled) {
                y += Wrap(FontId::SerifBold, "Your code did not compile - see the Compiler tab (errors are also marked in the editor).", in.x, y, in.width, 17, pal::Red) + 6;
            } else {
                int passed = 0;
                for (auto& r : job->results) if (r.status == runner::TestStatus::Pass) passed++;
                int total = (int)job->results.size();
                bool all = passed == total && ruleMsgs.empty();
                Text(FontId::SerifBold, TextFormat("%s: %d / %d tests passed", jobSubmit ? "Submitted" : "Sample run", passed, total), in.x, y, 20, all ? pal::Green : pal::Red);
                TextRight(FontId::SerifItalic, TextFormat("compile %.1fs, run %.1fs", job->compileMs / 1000.0, job->runMs / 1000.0), in.x + in.width, y + 4, 13, pal::InkFaint);
                y += 32;
                for (auto& m : ruleMsgs) { y += Rich("**Rule broken:** " + m, in.x, y, in.width, 16, pal::Red) + 4; }
                if (all && jobSubmit) { y += Wrap(FontId::SerifItalic, mock ? "Full marks for this question." : "Accepted! Marked as solved in your ledger.", in.x, y, in.width, 16, pal::Green) + 6; }
                if (all && !jobSubmit) { y += Wrap(FontId::SerifItalic, "Samples pass - now Submit to face the hidden tests.", in.x, y, in.width, 16, pal::Green) + 6; }
                const Problem& p = P();
                for (size_t i = 0; i < job->results.size(); i++) {
                    const runner::TestResult& r = job->results[i];
                    const TestCase& tc = p.tests[(size_t)built.testIndex[i]];
                    bool ok = r.status == runner::TestStatus::Pass;
                    Color c = ok ? pal::Green : pal::Red;
                    DrawIcon(ok ? Icon::Check : Icon::Cross, Vector2{ in.x + 9, y + 10 }, 14, c);
                    std::string label = (tc.sample ? "" : "[hidden] ") + tc.label;
                    y += Rich(label, in.x + 24, y, in.width - 24, 16, ok ? pal::Ink : pal::Red) + 2;
                    if (!ok) {
                        std::string detail;
                        switch (r.status) {
                            case runner::TestStatus::Fail:
                                detail = "Expected `" + util::ReplaceAll(r.expected, "\n", "\\n") + "` but got `" + util::ReplaceAll(r.got, "\n", "\\n") + "`";
                                if (!r.expr.empty() && r.expr != "(test body)") detail += "  (checking `" + r.expr + "`)";
                                break;
                            case runner::TestStatus::NotRun: detail = "Not run (an earlier test crashed the program)."; break;
                            default: detail = r.message; break;
                        }
                        if (r.status == runner::TestStatus::Fail && (r.expected.find('\n') != std::string::npos || r.got.find('\n') != std::string::npos)) {
                            Text(FontId::SerifItalic, "Expected output:", in.x + 24, y, 14, pal::InkSoft); y += 20;
                            y += syntax::DrawOutput(r.expected, in.x + 24, y, in.width - 24, 14) + 4;
                            Text(FontId::SerifItalic, "Your output:", in.x + 24, y, 14, pal::InkSoft); y += 20;
                            y += syntax::DrawOutput(r.got.empty() ? "(nothing)" : r.got, in.x + 24, y, in.width - 24, 14) + 6;
                        } else {
                            y += Rich(detail, in.x + 24, y, in.width - 24, 15, pal::InkSoft) + 6;
                        }
                    }
                }
            }
        } else if (resultTab == 1) {
            std::string out = util::Trim(job->programOutput);
            if (out.empty()) y += Wrap(FontId::SerifItalic, "Your code didn't print anything outside the tests.", in.x, y, in.width, 16, pal::InkSoft);
            else y += syntax::DrawOutput(out.substr(0, 4000), in.x, y, in.width, 14);
        } else {
            if (job->compiled && job->diags.empty()) y += Wrap(FontId::SerifItalic, "Compiled cleanly - no errors or warnings.", in.x, y, in.width, 16, pal::Green);
            bool foreign = false;
            for (auto& d : job->diags) {
                bool mine = d.file == "your_code.cpp";
                if (!mine && d.error) foreign = true;
                Color c = d.error ? pal::Red : pal::Amber;
                std::string head = mine ? TextFormat("Line %d", d.line) : (d.file + ":" + std::to_string(d.line));
                Rectangle row{ in.x, y, in.width, 22 };
                bool hov = interactive && mine && Hover(row);
                Text(FontId::SerifBold, head, in.x, y, 15, hov ? pal::Blue : c);
                float hw = MeasureWidth(FontId::SerifBold, head, 15) + 10;
                y += Rich(d.message, in.x + hw, y, in.width - hw, 15, pal::Ink) + 4;
                if (hov && Pressed(row)) ed.GotoLine(d.line);
            }
            if (foreign) y += Wrap(FontId::SerifItalic, "Errors outside your code usually mean the tests could not find a function with the exact name and parameters from the starter code - check you haven't renamed or changed it.", in.x, y + 6, in.width, 15, pal::InkSoft) + 10;
            if (!job->compiled && job->diags.empty()) y += syntax::DrawOutput(job->compilerLog.substr(0, 3000), in.x, y, in.width, 13);
        }
        float contentH = y - y0 + 10;
        PopClip();
        resultScroll = std::max(0.f, std::min(resultScroll, std::max(0.f, contentH - in.height)));
    }

    // ------------------------------------------------------------ mock result
    void DrawMockResult(bool interactive) {
        Rectangle L = page::Left(), R = page::Right();
        float p = mockTotalAll ? (float)mockPassedAll / mockTotalAll : 0;
        float y = L.y + 10;
        Text(FontId::SerifItalic, "Pens down!", L.x, y, 24, pal::InkSoft);
        y += 44;
        Text(FontId::SerifBold, TextFormat("%d%%", (int)(p * 100 + 0.5f)), L.x, y, 100, p >= 0.7f ? pal::Green : (p >= 0.4f ? pal::Amber : pal::Red));
        y += 124;
        Text(FontId::Serif, TextFormat("%d of %d hidden tests passed", mockPassedAll, mockTotalAll), L.x, y, 21, pal::InkSoft);
        y += 36;
        Text(FontId::Serif, "Time used: " + util::FormatDuration(3600.f - mockLeft), L.x, y, 21, pal::InkSoft);
        y += 60;
        page::Callout("Next step", "Open the regular workbook to finish the problems you didn't crack - your mock attempts don't overwrite your saved drafts.", L.x, y, L.width,
                      pal::Blue, util::WithAlpha(pal::BlueSoft, 0.6f), Icon::Bulb, true, 18);
        Text(FontId::SerifBold, "Per question", R.x, R.y + 4, 26, pal::Ink);
        float ry = R.y + 54;
        for (int i = 0; i < (int)probs.size(); i++) {
            const Problem& pr = Data().problems[(size_t)probs[(size_t)i]];
            int ps = mockPassed[probs[(size_t)i]], pt = mockTotal[probs[(size_t)i]];
            float f = pt ? (float)ps / pt : 0;
            Text(FontId::SerifBold, TextFormat("%d. %s", i + 1, pr.title.c_str()), R.x, ry, 19, pal::Ink);
            TextRight(FontId::SerifBold, TextFormat("%d/%d", ps, pt), R.x + R.width, ry, 19, util::Mix(pal::Red, pal::Green, f));
            ProgressBar(Rectangle{ R.x, ry + 28, R.width, 8 }, f, util::Mix(pal::Red, pal::Green, f), pal::PaperEdge);
            ry += 58;
        }
        (void)interactive;
    }
};

} // namespace

std::unique_ptr<BookView> MakeCodingView(int setIndex, bool mock, const std::string& openProblem) {
    return std::make_unique<CodingView>(setIndex, mock, openProblem);
}
