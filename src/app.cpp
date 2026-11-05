// app.cpp - Malloc's Library (2D edition): the director that runs the whole show
#include "app.h"
#include "art.h"
#include "pixel.h"
#include "ui.h"
#include "audio.h"
#include "content.h"
#include "progress.h"
#include "code_runner.h"
#include "rlgl.h"
#include <algorithm>
#include <fstream>
#include <sstream>

using namespace util;
namespace pal = ui::pal;

static const char* kTabNames[6] = { "Reading Room", "Multiple Choice", "Written Test", "Coding Exams", "Problem Areas", "Past Results" };
static const char* kTabSubs[6] = { "Tutorials from lectures", "Quick-fire quizzes", "Predict, fill in, fix", "Lab exam practice", "Your weak spots", "Ledger of results" };
static const Icon kTabIcons[6] = { Icon::Book, Icon::Quiz, Icon::Quill, Icon::Code, Icon::Target, Icon::Ledger };
static const Color kTabColors[6] = { { 46, 98, 70, 255 }, { 44, 74, 120, 255 }, { 120, 60, 40, 255 }, { 92, 40, 96, 255 }, { 150, 44, 40, 255 }, { 120, 90, 40, 255 } };

static const char* kQuips[] = {
    "Why did the pointer go to therapy? It had too many unresolved references.",
    "A tip: always initialise your pointers. `int *p = nullptr;` costs nothing and saves hours.",
    "Remember, `new` without `delete` is a memory leak. And a leak in a library means soggy books.",
    "Arrays start at zero. I start at nine. Opening hours, you understand.",
    "`sizeof(arr) / sizeof(arr[0])` gives the element count - but only where the array was declared, not inside a function it was passed to.",
    "In C++, a `struct` is just a `class` whose members are public by default. Same animal, friendlier manners.",
    "If `p` points to `x`, then `*p` IS `x`. Change one and you've changed the other.",
    "The prefix `++c` returns the new value; postfix `c++` hands back a copy of the old one. Mind the difference in your operator overloads.",
    "`vector` iterators can be invalidated by `push_back`. If it reallocates, your old iterator is pointing at rubble.",
    "A dangling pointer is like a library card for a book that's been pulped. Don't use it.",
    "My cat is called Segfault. He wanders into memory he doesn't own.",
    "A linked list is just a treasure hunt: every node holds a clue to the next one, and `nullptr` means the treasure's gone.",
    "`map` keeps its keys sorted. `unordered_map` doesn't - but it's usually faster.",
    "The coding exam marks you on hidden tests. Try the edge cases: empty arrays, zero, negative numbers, one element.",
    "Templates let you write the function once and let the compiler write the rest. Lazy? No - efficient.",
    "Pass big objects by `const` reference. Copying a whole object just to read it is like photocopying a book to check its title.",
};

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void App::Init() {
    Prog().Load();
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Malloc's Library (8-bit) - an interactive C & C++ study hall");
    int mon = GetCurrentMonitor();
    int mw = GetMonitorWidth(mon), mh = GetMonitorHeight(mon);
    if (mw > 0 && mh > 0) {
        int w = (int)(mw * 0.86f), h = (int)(mh * 0.86f);
        if (w * 9 > h * 16) w = h * 16 / 9; else h = w * 9 / 16;
        SetWindowSize(w, h);
        SetWindowPosition((mw - w) / 2, (mh - h) / 2);
    }
    SetWindowMinSize(960, 540);
    SetExitKey(0);
    SetTargetFPS(60);

    ui::LoadFonts();
    pixel::Init();
    sfx::Init();
    art::Init();
    Data().Load();
    stage.Build();
    bk.Init();
    lib.carriedBook = &bk;
    lib.onFootstep = [this]() { sfx::Play(sfx::Footstep, 0.32f, 0.9f + 0.2f * fmodf(time * 7.f, 1.f)); };
    pageRT = LoadRenderTexture(780, 500);
    SetTextureFilter(pageRT.texture, TEXTURE_FILTER_POINT);
    Image v = GenImageGradientRadial(640, 360, 0.25f, Color{ 0, 0, 0, 0 }, Color{ 0, 0, 0, 255 });
    vignette = LoadTextureFromImage(v);
    UnloadImage(v);
    SetTextureFilter(vignette, TEXTURE_FILTER_BILINEAR);
    runner::DetectAsync();
    stage.pileCount = std::min(9, Prog().booksReturned);
    ApplySettings();
    if (Prog().settings.fullscreen) ToggleBorderlessWindowed();
    SetPhase(Phase::Intro);
}

void App::ApplySettings() {
    sfx::SetMasterVolume(Prog().settings.volume);
    sfx::SetVoiceEnabled(Prog().settings.voice);
}

void App::Shutdown() {
    if (view) { view->OnClose(); view.reset(); }
    Prog().Save();
    UnloadRenderTexture(pageRT);
    UnloadTexture(vignette);
    bk.Shutdown();
    stage.Shutdown();
    art::Shutdown();
    pixel::Shutdown();
    ui::UnloadFonts();
    sfx::Shutdown();
    CloseWindow();
}

int App::Run(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--script" && i + 1 < argc) LoadScript(argv[++i]);
    }
    Init();
    while (!WindowShouldClose() && !autoQuit) {
        float dt = std::min(GetFrameTime(), 1.f / 20.f);
        Update(dt);
        Render();
    }
    Shutdown();
    return 0;
}

// ---------------------------------------------------------------------------
// choices / requests
// ---------------------------------------------------------------------------
static std::vector<std::string> MixTopics(const std::string& key) {
    std::vector<std::string> t;
    for (const Topic& tp : Data().topics) {
        if (key == "all" || (key == "exam1" && tp.exam == 1) || (key == "exam2" && tp.exam == 2) || (key == "cpp" && tp.exam == 3)) t.push_back(tp.id);
    }
    return t;
}
static bool IsMix(const std::string& k) { return k == "exam1" || k == "exam2" || k == "cpp" || k == "all"; }
static std::string MixName(const std::string& k) {
    if (k == "exam1") return "Exam 1 Mix";
    if (k == "exam2") return "Exam 2 Mix";
    if (k == "cpp") return "C++ Mix";
    return "Everything";
}
static const char* kMixes[4] = { "exam1", "exam2", "cpp", "all" };

std::vector<Choice> App::TopicChoices(bool withMixes) const {
    std::vector<Choice> out;
    std::vector<TopicMastery> m = Prog().Mastery();
    if (withMixes) {
        out.push_back(Choice{ "Exam 1 Mix", "Basics, loops, arrays, functions", pal::Blue, -1, "EXAM 1", Icon::Star });
        out.push_back(Choice{ "Exam 2 Mix", "Pointers, memory, lists, classes", pal::Red, -1, "EXAM 2", Icon::Star });
        out.push_back(Choice{ "C++ Mix", "Overloading, templates, STL", pal::Green, -1, "C++", Icon::Star });
        out.push_back(Choice{ "Everything", "The complete paper", pal::Wood, -1, "ALL", Icon::Star });
    }
    for (size_t i = 0; i < Data().topics.size(); i++) {
        const Topic& t = Data().topics[i];
        Choice c;
        c.label = t.name;
        c.sub = t.blurb;
        c.accent = t.color;
        c.mastery = i < m.size() ? m[i].score : -1.f;
        c.badge = t.exam == 1 ? "EX 1" : (t.exam == 2 ? "EX 2" : "C++");
        c.icon = Icon::Book;
        out.push_back(c);
    }
    return out;
}

void App::AskCount(const std::string& kind, const std::string& key) {
    std::vector<Choice> c = {
        Choice{ kind == "mcq" ? "A quick five" : "A quick four", "Five minutes, tops", pal::Green, -1, "", Icon::Clock },
        Choice{ kind == "mcq" ? "The standard ten" : "The standard eight", "A proper sitting", pal::Blue, -1, "", Icon::Quiz },
        Choice{ kind == "mcq" ? "A marathon twenty" : "A dozen", "Bring a flask of tea", pal::Red, -1, "", Icon::Flag },
    };
    const int counts[3][2] = { { 5, 4 }, { 10, 8 }, { 20, 12 } };
    dlg.Ask("And how many questions can you stomach?", c, [this, kind, key, counts](int i) {
        if (i < 0) { dlg.Say("Another time, then."); return; }
        Request r; r.kind = kind; r.key = key; r.count = counts[i][kind == "mcq" ? 0 : 1];
        StartRequest(r);
    }, 3);
}

void App::OnTab(int i) {
    sfx::Play(sfx::Click);
    settingsOpen = helpOpen = false;
    switch (i) {
    case 0:
        dlg.Ask("The Reading Room! Every chapter is drawn from your lecture notes, with worked examples and exam tips. Which subject shall I fetch?",
                TopicChoices(false), [this](int c) {
                    if (c < 0) { dlg.Say("No rush. The books aren't going anywhere."); return; }
                    Request r; r.kind = "read"; r.key = Data().topics[(size_t)c].id;
                    StartRequest(r);
                }, 4);
        break;
    case 1:
    case 2: {
        std::string kind = i == 1 ? "mcq" : "written";
        dlg.Ask(i == 1 ? "Multiple choice, splendid. A mixed exam paper, or a single subject?"
                       : "A written test - my favourite. You'll predict outputs, fill in blanks and fix bugs. Which section?",
                TopicChoices(true), [this, kind](int c) {
                    if (c < 0) { dlg.Say("Very well. I'll be here."); return; }
                    std::string key = c < 4 ? kMixes[c] : Data().topics[(size_t)c - 4].id;
                    AskCount(kind, key);
                }, 4);
        break;
    }
    case 3: {
        std::vector<Choice> ch;
        const Content& C = Data();
        for (const ProblemSet& s : C.sets) {
            int solved = 0;
            for (int p : s.problems) if (Prog().Code(C.problems[(size_t)p].id).solved) solved++;
            ch.push_back(Choice{ s.title, TextFormat("%d problems  -  %d solved", (int)s.problems.size(), solved), s.color,
                                 s.problems.empty() ? 0.f : (float)solved / s.problems.size(), s.examNo ? TextFormat("EXAM %d", s.examNo) : "C++", Icon::Code });
        }
        std::vector<int> mockSets;
        for (int k = 0; k < (int)C.sets.size(); k++) {
            if (C.sets[(size_t)k].examNo == 0) continue;
            mockSets.push_back(k);
            ch.push_back(Choice{ TextFormat("Mock Exam %d", C.sets[(size_t)k].examNo), "5 random problems, 60 minutes", pal::Red, -1, "TIMED", Icon::Clock });
        }
        int nsets = (int)C.sets.size();
        dlg.Ask("The coding exams. Like the lab exam, you'll write the functions and my tests will judge them - honestly, but fairly. Which workbook?",
                ch, [this, nsets, mockSets](int c) {
                    if (c < 0) { dlg.Say("Coding can wait. Coffee cannot. Wait - no drinks near the books!"); return; }
                    Request r;
                    if (c < nsets) { r.kind = "coding"; r.key = Data().sets[(size_t)c].id; }
                    else { r.kind = "mock"; r.key = Data().sets[(size_t)mockSets[(size_t)(c - nsets)]].id; }
                    StartRequest(r);
                }, 3);
        break;
    }
    case 4: { Request r; r.kind = "areas"; StartRequest(r); break; }
    case 5: { Request r; r.kind = "stats"; StartRequest(r); break; }
    }
}

std::string App::ShelfKeyFor(const Request& r) const {
    if (r.kind == "coding" || r.kind == "mock") return "workbook_" + r.key;
    if (IsMix(r.key)) return r.key == "exam1" ? "paper_exam1" : (r.key == "exam2" ? "paper_exam2" : "paper_all");
    return r.key;
}

static std::string Ordinal(int n) {
    const char* w[] = { "first", "second", "third", "fourth", "fifth", "sixth", "seventh" };
    return n >= 0 && n < 7 ? w[n] : "next";
}

std::string App::FetchLine(const Request& r) const {
    int si = stage.FindShelfBook(ShelfKeyFor(r));
    int bay = 0;
    if (si >= 0) bay = std::max(0, (int)floorf((stage.ShelfBooks()[(size_t)si].rect.x - world::Bay0X) / world::BayW));
    std::string where = "bay " + std::to_string(bay + 1);
    if (r.kind == "coding" || r.kind == "mock") {
        const ProblemSet* s = nullptr;
        for (auto& set : Data().sets) if (set.id == r.key) s = &set;
        std::string name = s ? s->title : r.key;
        if (r.kind == "mock") return "A mock exam! I'll fetch the **" + name + "** and pick five problems at random. The clock starts when you open it.";
        return "The **" + name + "**. Bottom shelf of " + where + " - mind your toes.";
    }
    if (IsMix(r.key)) return "The **" + MixName(r.key) + "** paper... lower shelf, " + where + ". Won't be a moment.";
    const Topic* t = Data().FindTopic(r.key);
    std::string name = t ? t->name : r.key;
    if (r.kind == "read") return "**" + name + "**... " + Ordinal(bay) + " bay, at chest height. One moment!";
    if (r.kind == "drill") return "Your weak spots, eh? I'll start with **" + name + "** and serve up the questions you missed first.";
    return "A " + std::string(r.kind == "mcq" ? "quiz" : "written test") + " on **" + name + "**. That volume lives in " + where + ".";
}

book::Look App::LookFor(const Request& r) const {
    book::Look l;
    if (r.kind == "areas" || r.kind == "stats") {
        l.title = "The Ledger"; l.subtitle = "Being a true record of one diligent student"; l.emblem = "#"; l.volume = "RECORDS";
        l.cover = Color{ 34, 60, 46, 255 };
        return l;
    }
    if (r.kind == "coding" || r.kind == "mock") {
        for (auto& s : Data().sets) if (s.id == r.key) { l.title = s.title; l.subtitle = s.subtitle; l.cover = s.color; }
        l.emblem = "</>"; l.volume = r.kind == "mock" ? "MOCK EXAMINATION" : "WORKBOOK";
        return l;
    }
    if (IsMix(r.key)) {
        l.title = MixName(r.key) + " Paper";
        l.subtitle = r.key == "exam1" ? "C basics, loops, arrays and functions" : r.key == "exam2" ? "Pointers, memory, linked lists and classes" :
                     r.key == "cpp" ? "Overloading, templates and the STL" : "Questions from every chapter";
        l.emblem = r.key == "exam1" ? "I" : (r.key == "exam2" ? "II" : "ALL");
        l.volume = "EXAMINATION PAPER";
        int si = stage.FindShelfBook(ShelfKeyFor(r));
        l.cover = si >= 0 ? stage.ShelfBooks()[(size_t)si].color : Color{ 120, 100, 80, 255 };
        return l;
    }
    const Topic* t = Data().FindTopic(r.key);
    int idx = Data().TopicIndex(r.key);
    const char* roman[] = { "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII" };
    l.title = t ? t->name : r.key;
    l.subtitle = t ? t->blurb : "";
    l.emblem = t ? t->emblem : "?";
    l.volume = idx >= 0 && idx < 12 ? std::string("VOLUME ") + roman[idx] : "VOLUME";
    l.cover = t ? t->color : Color{ 90, 40, 40, 255 };
    return l;
}

std::unique_ptr<BookView> App::MakeView(const Request& r) {
    const Content& C = Data();
    if (r.kind == "read") return MakeReadingView(r.key);
    if (r.kind == "areas") return MakeStatsView(true);
    if (r.kind == "stats") return MakeStatsView(false);
    if (r.kind == "coding" || r.kind == "mock") {
        for (int i = 0; i < (int)C.sets.size(); i++) if (C.sets[(size_t)i].id == r.key) return MakeCodingView(i, r.kind == "mock", r.problem);
        return MakeStatsView(false);
    }
    std::vector<std::string> topics = IsMix(r.key) ? MixTopics(r.key) : std::vector<std::string>{ r.key };
    std::string title = IsMix(r.key) ? MixName(r.key) : (C.FindTopic(r.key) ? C.FindTopic(r.key)->name : r.key);
    if (r.kind == "drill") {
        std::vector<TopicMastery> m = Prog().Mastery();
        std::sort(m.begin(), m.end(), [](const TopicMastery& a, const TopicMastery& b) {
            float sa = a.score < 0 ? 2.f : a.score, sb = b.score < 0 ? 2.f : b.score;
            return sa < sb;
        });
        std::vector<std::string> weak;
        for (auto& t : m) if (t.score >= 0 && weak.size() < 3) weak.push_back(t.topic);
        if (weak.empty()) weak = MixTopics("all");
        return MakeMcqView(PickMcqs(weak, 10, true), "Weak-spot drill", "drill", "drill");
    }
    if (r.kind == "written") return MakeWrittenView(PickWritten(topics, r.count, false), title, r.key);
    return MakeMcqView(PickMcqs(topics, r.count, false), title, r.key, "mcq");
}

void App::StartRequest(const Request& rq) {
    Request r = rq;
    if (r.kind == "drill") {
        std::vector<TopicMastery> m = Prog().Mastery();
        float best = 2.f;
        r.key = "all";
        for (auto& t : m) if (t.score >= 0 && t.score < best) { best = t.score; r.key = t.topic; }
    }
    if (r.kind == "coding" && !r.problem.empty() && Data().ProblemIndex(r.problem) < 0) r.problem.clear();
    current = r;
    bk.SetLook(LookFor(r));
    bk.visible = false;
    bookAsSpread = false;
    bookOpen = 0.f;
    bool queueLine = dlg.Visible() && phase == Phase::Hub && !pendingRemark.empty();
    pendingRemark.clear();
    if (r.kind == "areas" || r.kind == "stats") {
        shelfIndex = -1;
        std::string line = r.kind == "areas" ? "Problem areas? Let me consult my ledger... I keep notes on everything you get wrong. Purely academic, of course."
                                             : "The ledger of past results. I keep it in the drawer, away from prying eyes.";
        queueLine ? dlg.Queue(line) : dlg.Say(line);
        BuildDrawerFetch();
    } else {
        shelfIndex = stage.FindShelfBook(ShelfKeyFor(r));
        if (shelfIndex < 0) shelfIndex = 0;
        std::string line = FetchLine(r);
        queueLine ? dlg.Queue(line) : dlg.Say(line);
        BuildShelfFetch();
    }
    SetPhase(Phase::Fetching);
}

// ---------------------------------------------------------------------------
// choreography
// ---------------------------------------------------------------------------
App::Step App::StandUp() {
    Step s;
    s.dur = 1.1f;
    s.enter = [this]() { lib.seated = false; lib.walk = 0.f; };
    s.tick = [this](float t, float) {
        Pose p = LerpPose(poses::Sit(), poses::Stand(), Smooth(t));
        p.bend = sinf(t * PI) * 0.55f;
        lib.base = p;
        stage.chairBack = Smooth(t * 1.4f);
    };
    s.exit = [this]() { lib.base = poses::Stand(); };
    return s;
}

App::Step App::SitDown() {
    Step s;
    s.dur = 1.1f;
    s.tick = [this](float t, float dt) {
        lib.walk = Damp(lib.walk, 0.f, 8.f, dt);
        Pose p = LerpPose(poses::Stand(), poses::Sit(), Smooth(t));
        p.bend = sinf(t * PI) * 0.45f;
        lib.base = p;
        stage.chairBack = 1.f - Smooth((t - 0.2f) / 0.8f);
    };
    s.exit = [this]() { lib.seated = true; lib.base = poses::Sit(); lib.walk = 0.f; stage.chairBack = 0.f; lib.feet = Vector2{ 800.f, world::SeatFeetY }; };
    return s;
}

App::Step App::WalkTo(Vector2 target, float speed) {
    Step s;
    auto from = std::make_shared<Vector2>();
    s.enter = [this, from, target, speed]() {
        *from = lib.feet;
        float d = Vector2Distance(*from, target);
        steps[(size_t)stepIdx].dur = std::max(0.2f, d / speed);
    };
    s.tick = [this, from, target](float t, float dt) {
        Vector2 prev = lib.feet;
        lib.feet = Vector2Lerp(*from, target, t);
        lib.walk = Damp(lib.walk, 1.f, 7.f, dt);
        lib.walkPhase += Vector2Distance(prev, lib.feet) / (40.f * lib.Scale()) * PI;
        lib.base = lib.carrying ? poses::Carry() : poses::Stand();
        float edgeR = 1260.f + (lib.feet.y - 680.f) * 1.05f, edgeL = 340.f - (lib.feet.y - 680.f) * 1.05f;
        lib.behindDesk = lib.feet.y >= 700.f && lib.feet.x < edgeR - 8.f && lib.feet.x > edgeL + 8.f;
    };
    s.exit = [this, target]() { lib.feet = target; };
    return s;
}

App::Step App::Turn(bool toBack) {
    Step s;
    s.dur = 0.36f;
    s.tick = [this, toBack](float t, float dt) {
        lib.walk = Damp(lib.walk, 0.f, 8.f, dt);
        if (t < 0.5f) lib.turn = 1.f - Smooth(t * 2.f);
        else { lib.back = toBack; lib.turn = Smooth((t - 0.5f) * 2.f); }
        lib.base = lib.carrying ? poses::Carry() : poses::Stand();
    };
    s.exit = [this, toBack]() { lib.turn = 1.f; lib.back = toBack; };
    return s;
}

// Hands the book across the desk: it rides in his hands, then slides the rest of the way.
App::Step App::PlaceBook(bool seated) {
    Step s;
    s.dur = 1.7f;
    auto thud = std::make_shared<bool>(false);
    auto rel = std::make_shared<Vector2>();
    auto relSet = std::make_shared<bool>(false);
    s.enter = [this, thud, relSet]() {
        *thud = false; *relSet = false;
        lib.carrying = false;
        bk.visible = true;
        bookAsSpread = false;
        bookW = 82.f * lib.Scale(); bookSquash = 1.f; bookRot = 0.f;
        bookPos = lib.CarryCenter();
    };
    s.tick = [this, thud, rel, relSet, seated](float t, float dt) {
        lib.walk = Damp(lib.walk, 0.f, 8.f, dt);
        Pose carry = poses::Carry(), place = poses::Place(), rest = poses::Stand();
        if (seated) {
            carry.drop = 120.f; carry.hand[0].y += 120.f; carry.hand[1].y += 120.f;
            place.drop = 120.f; place.bend = 0.5f;
            rest = poses::Sit();
        }
        Pose p;
        if (t < 0.45f) p = LerpPose(carry, place, Smooth(t / 0.45f));
        else if (t < 0.65f) p = place;
        else p = LerpPose(place, rest, Smooth((t - 0.65f) / 0.35f));
        lib.base = p;
        float sc = lib.Scale();
        if (t < 0.5f) {
            float k = Smooth(t / 0.5f);
            Vector2 m = lib.HandMid();
            bookPos = Vector2{ m.x, m.y - 4.f * sc };
            bookW = Lerp(82.f * sc, 130.f, k);
            bookSquash = Lerp(1.f, 0.8f, k);
        } else {
            if (!*relSet) { *relSet = true; *rel = bookPos; }
            float u = EaseOutCubic((t - 0.5f) / 0.45f);
            bookPos = Vector2Lerp(*rel, world::BookOnDesk, u);
            bookW = Lerp(130.f, 210.f, u);
            bookSquash = Lerp(0.8f, 0.62f, u);
            if (u > 0.82f && !*thud) { *thud = true; sfx::Play(sfx::Thud, 0.8f); }
        }
        lib.lookAt = bookPos;
    };
    s.exit = [this]() {
        bookPos = world::BookOnDesk; bookW = 210.f; bookSquash = 0.62f;
        dlg.Queue("There we are. Let's see what you make of it."); dlg.Finish();
    };
    return s;
}

void App::BuildShelfFetch() {
    steps.clear();
    stepIdx = 0; stepT = 0; stepStarted = false;
    ShelfBook& sb = stage.Shelf(shelfIndex);
    float wallY = world::WallFeetY;
    Vector2 beside{ 1338.f, world::SeatFeetY }, corner{ 1408.f, 690.f }, aisle{ 1408.f, wallY }, at{ sb.standX, wallY };
    Vector2 bookCentre{ sb.rect.x + sb.rect.width * 0.5f, sb.rect.y + sb.rect.height * 0.5f };
    steps.push_back(StandUp());
    steps.push_back(WalkTo(beside, 300.f));
    steps.push_back(Turn(true));
    steps.push_back(WalkTo(corner, 260.f));
    steps.push_back(WalkTo(aisle, 230.f));
    steps.push_back(WalkTo(at, 330.f));
    {   // reach for the book and take it
        Step s;
        s.dur = 1.8f;
        s.enter = [this]() { lib.carrying = false; };
        s.tick = [this, bookCentre](float t, float dt) {
            ShelfBook& b = stage.Shelf(shelfIndex);
            lib.walk = Damp(lib.walk, 0.f, 8.f, dt);
            Vector2 target = lib.FromScene(bookCentre);
            Pose reach = poses::Reach(target, b.low ? 0.35f : 0.1f);
            Pose p;
            if (t < 0.38f) p = LerpPose(poses::Stand(), reach, Smooth(t / 0.38f));
            else if (t < 0.7f) p = reach;
            else p = LerpPose(reach, poses::Carry(), Smooth((t - 0.7f) / 0.3f));
            lib.base = p;
            lib.lookAt = bookCentre;
            if (t >= 0.4f && b.present) {
                b.present = false;
                pullShow = true; pullRect = b.rect; pullColor = b.color; pullMark = b.mark; pullAlpha = 1.f;
                sfx::Play(sfx::Page, 0.35f, 0.8f);
            }
            if (pullShow) {
                Rectangle base = b.rect;
                if (t < 0.62f) {
                    float u = Smooth((t - 0.4f) / 0.22f);
                    float k = 1.f + 0.55f * u;
                    pullRect = Rectangle{ base.x + base.width * 0.5f - base.width * k * 0.5f, base.y + base.height * 0.5f - base.height * k * 0.5f + 16.f * u, base.width * k, base.height * k };
                    pullAlpha = 1.f;
                } else {
                    float u = Smooth((t - 0.62f) / 0.38f);
                    float k = 1.55f * (1.f - 0.35f * u);
                    pullRect = Rectangle{ base.x + base.width * 0.5f - base.width * k * 0.5f, base.y + base.height * 0.5f - base.height * k * 0.5f + 16.f + 40.f * u, base.width * k, base.height * k };
                    pullAlpha = 1.f - u;
                }
            }
        };
        s.exit = [this]() { pullShow = false; lib.carrying = true; lib.base = poses::Carry(); dlg.Queue("Ah - here it is."); dlg.Finish(); };
        steps.push_back(s);
    }
    steps.push_back(Turn(false));
    steps.push_back(WalkTo(aisle, 330.f));
    steps.push_back(WalkTo(corner, 230.f));
    steps.push_back(WalkTo(beside, 260.f));
    steps.push_back(WalkTo(Vector2{ 800.f, world::SeatFeetY }, 300.f));
    steps.push_back(PlaceBook(false));
    steps.push_back(SitDown());
}

void App::BuildDrawerFetch() {
    steps.clear();
    stepIdx = 0; stepT = 0; stepStarted = false;
    {
        Step s;
        s.dur = 1.0f;
        s.enter = []() { sfx::Play(sfx::Drawer, 0.7f); };
        s.tick = [this](float t, float) {
            lib.base = LerpPose(poses::Sit(), poses::Drawer(), Smooth(t));
            lib.lookAt = Vector2{ 880.f, 760.f };
        };
        steps.push_back(s);
    }
    {
        Step s;
        s.dur = 0.9f;
        s.enter = [this]() { lib.carrying = true; };
        s.tick = [this](float t, float) {
            Pose carry = poses::Carry();
            carry.drop = 120.f; carry.hand[0].y += 120.f; carry.hand[1].y += 120.f;
            lib.base = LerpPose(poses::Drawer(), carry, Smooth(t));
        };
        steps.push_back(s);
    }
    steps.push_back(PlaceBook(true));
}

void App::RunSteps(float dt) {
    if (stepIdx >= (int)steps.size()) return;
    while (dt > 0 && stepIdx < (int)steps.size()) {
        Step& s = steps[(size_t)stepIdx];
        if (!stepStarted) {
            stepStarted = true;
            stepT = 0;
            if (s.enter) s.enter();
        }
        Step& st = steps[(size_t)stepIdx];   // enter() may have changed dur
        float remain = st.dur - stepT;
        float use = std::min(dt, remain);
        stepT += use;
        dt -= use;
        if (st.tick) st.tick(Clamp01(stepT / std::max(0.001f, st.dur)), std::max(use, 0.0001f));
        if (stepT >= st.dur - 1e-5f) {
            if (st.exit) st.exit();
            stepIdx++;
            stepStarted = false;
        } else break;
    }
}

void App::BeginPresenting() {
    ui::SetScaleHint(0.5f);
    view = MakeView(current);
    bk.visible = true;
    bookAsSpread = true;
    bookOpen = 0.f;
    dlg.Hide();
    SetPhase(Phase::Presenting);
    sfx::Play(sfx::Open, 0.7f);
}

void App::BeginClosing() {
    if (!view) return;
    view->OnClose();
    pendingRemark = view->Remark();
    hasQueued = false;
    if (!view->requestAction.empty()) {
        queued = Request{};
        queued.kind = view->requestAction;
        queued.key = view->requestArg;
        queued.count = 10;
        if (queued.kind == "coding") {
            // pick the workbook that covers this topic and its first unsolved problem
            const Content& C = Data();
            const Topic* t = C.FindTopic(view->requestArg);
            std::string setId = t ? (t->exam == 1 ? "exam1" : (t->exam == 2 ? "exam2" : "cpp")) : "exam1";
            queued.key = setId;
            for (auto& p : C.problems) if (p.set == setId && p.topic == view->requestArg && !Prog().Code(p.id).solved) { queued.problem = p.id; break; }
        }
        if (queued.kind == "read" || queued.kind == "mcq" || queued.kind == "written" || queued.kind == "drill" || queued.kind == "coding" || queued.kind == "mock")
            hasQueued = true;
    }
    SetPhase(Phase::Closing);
    steps.clear();
    stepIdx = 0; stepT = 0; stepStarted = false;
    {
        Step s;
        s.dur = 1.15f;
        s.tick = [this](float t, float) {
            bookBlend = 1.f - EaseInOutCubic(t);
            bookOpen = 1.f - Smooth((t - 0.35f) / 0.6f);
        };
        s.exit = [this]() {
            bookOpen = 0.f; bookBlend = 0.f;
            bookAsSpread = false;
            bookPos = world::BookOnDesk; bookW = 210.f; bookSquash = 0.62f; bookRot = 0.f;
            sfx::Play(sfx::Thud, 0.4f, 1.3f);
        };
        steps.push_back(s);
    }
    {
        Step s;
        s.dur = 1.6f;
        auto start = std::make_shared<Vector2>();
        s.enter = [this, start]() { *start = bookPos; dlg.Say(pendingRemark); };
        s.tick = [this, start](float t, float) {
            Pose lean = poses::Sit();
            lean.bend = 0.55f; lean.hand[0] = Vector2{ -52.f, -64.f }; lean.hand[1] = Vector2{ 52.f, -64.f };
            Vector2 slot = stage.PileSlot(std::min(stage.pileCount, 8));
            if (t < 0.3f) {
                lib.base = LerpPose(poses::Sit(), lean, Smooth(t / 0.3f));
                bookPos = *start;
            } else if (t < 0.8f) {
                float u = Smooth((t - 0.3f) / 0.5f);
                lib.base = lean;
                bookPos = Vector2Lerp(*start, Vector2{ slot.x, slot.y - 30.f * sinf(u * PI) }, u);
                bookW = Lerp(210.f, 150.f, u);
                bookSquash = Lerp(0.62f, 0.22f, u);
            } else {
                lib.base = LerpPose(lean, poses::Sit(), Smooth((t - 0.8f) / 0.2f));
                bookPos = slot;
                bookW = 150.f; bookSquash = 0.22f;
            }
            lib.lookAt = bookPos;
        };
        s.exit = [this]() { FinishClosing(); };
        steps.push_back(s);
    }
}

void App::FinishClosing() {
    if (stage.pileCount < 9) {
        stage.pileColors[(size_t)stage.pileCount % stage.pileColors.size()] = bk.GetLook().cover;
        stage.pileCount++;
    }
    Prog().booksReturned++;
    Prog().Save();
    sfx::Play(sfx::Thud, 0.35f, 1.4f);
    bk.visible = false;
    if (shelfIndex >= 0) stage.Shelf(shelfIndex).present = true;
    view.reset();
    lib.base = poses::Sit();
    SetPhase(Phase::Hub);
    if (hasQueued) { hasQueued = false; StartRequest(queued); }
}

void App::SetPhase(Phase p) {
    phase = p;
    phaseT = 0;
}

// ---------------------------------------------------------------------------
// update
// ---------------------------------------------------------------------------
Vector2 App::MouseScene() const {
    Vector2 m = GetMousePosition();
    return Vector2{ (m.x - (hudW - world::W) * 0.5f * hudScale) / hudScale, m.y / hudScale };
}

void App::UpdateLibrarianIdle(float dt) {
    (void)dt;
    Vector2 m = MouseScene();
    if (!lib.seated) {
        if (phase == Phase::Fetching && !lib.back) lib.lookAt = Vector2{ 800.f, 900.f };
        return;
    }
    if (phase == Phase::Intro) {
        lib.lookAt = phaseT < 1.2f ? Vector2{ 880.f, 800.f } : m;
    } else if (phase == Phase::Hub) {
        Vector2 look = m;
        if (dlg.ChoicesShowing()) look = Vector2{ 800.f, 900.f };
        if (hoverTab >= 0) look = Vector2{ hoverTab < 3 ? 40.f : 1560.f, 300.f + (hoverTab % 3) * 140.f };
        lib.lookAt = look;
    } else if (phase == Phase::Reading || phase == Phase::Presenting) {
        bool glance = fmodf(time, 9.f) < 1.6f;
        lib.lookAt = glance ? Vector2{ 800.f, 900.f } : Vector2{ 800.f, 800.f };
    }
}

void App::Update(float dt) {
    time += dt;
    phaseT += dt;
    ui::NewFrame();
    pixel::UpdateMouseMapping();
    hudScale = (float)pixel::CanvasH / 900.f;
    hudW = (float)pixel::CanvasW() / hudScale;
    hudH = 900.f;

    if (IsKeyPressed(KEY_F11)) { ToggleBorderlessWindowed(); Prog().settings.fullscreen = !Prog().settings.fullscreen; }
    if (IsKeyPressed(KEY_F12)) SaveShot(TextFormat("screenshot_%lld", NowEpoch()));
    bool hurry = (IsKeyDown(KEY_SPACE) && !ui::KeyboardCaptured()) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    float baseSpeed = Prog().settings.quickFetch ? 2.0f : 1.0f;
    timeScale = baseSpeed * (hurry ? 3.5f : 1.f);

    RunAutopilot(dt);
    stage.Update(dt, time);
    dlg.Update(dt);
    lib.talk = dlg.Mouth();

    // Escape key
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (settingsOpen || helpOpen) { settingsOpen = helpOpen = false; }
        else if (phase == Phase::Reading && view) { if (!view->HandleEscape()) BeginClosing(); }
        else if (phase == Phase::Hub && dlg.HasChoices()) dlg.Pick(-1);
    }

    switch (phase) {
    case Phase::Intro:
        fade = 1.f - Smooth(phaseT / 1.8f);
        if (phaseT > 1.3f && !dlg.Visible()) {
            std::string greet;
            if (Prog().sessions.empty())
                greet = "Ah, a new reader! Welcome to the library. I'm **Mr. Malloc** - I allocate the books, and you, dear reader, must remember to free them. Choose a service from the cards at the edges and I'll fetch what you need.";
            else {
                const Session& last = Prog().sessions.back();
                greet = "Welcome back! Last time it was **" + last.title + "**.";
                std::vector<TopicMastery> m = Prog().Mastery();
                const TopicMastery* weak = nullptr;
                for (auto& t : m) if (t.score >= 0 && (!weak || t.score < weak->score)) weak = &t;
                if (weak && weak->score < 0.75f) {
                    const Topic* tp = Data().FindTopic(weak->topic);
                    if (tp) greet += " Your weakest area is **" + tp->name + "** - perhaps we should do something about that?";
                } else greet += " What shall it be today?";
            }
            dlg.Say(greet);
        }
        if (phaseT > 2.0f) SetPhase(Phase::Hub);
        break;
    case Phase::Hub:
        fade = 0;
        break;
    case Phase::Fetching:
        RunSteps(dt * timeScale);
        if (stepIdx >= (int)steps.size()) BeginPresenting();
        break;
    case Phase::Presenting: {
        float sp = hurry ? 2.5f : 1.f;
        phaseT += dt * (sp - 1.f);
        bookOpen = Smooth((phaseT - 0.1f) / 0.85f);
        bookBlend = Clamp01((phaseT - 0.3f) / 1.15f);
        if (view) view->Tick(dt);
        if (phaseT > 1.5f) { bookBlend = 1; bookOpen = 1; SetPhase(Phase::Reading); }
        break;
    }
    case Phase::Reading:
        bookBlend = 1;
        bookOpen = 1;
        if (view) {
            view->Tick(dt);
            if (view->wantClose) BeginClosing();
        }
        break;
    case Phase::Closing:
        RunSteps(dt * timeScale);
        break;
    }

    UpdateLibrarianIdle(dt);
    lib.Update(dt, time);
    UpdatePan(dt);
}

void App::UpdatePan(float dt) {
    float sw = (float)pixel::CanvasW(), sh = (float)pixel::CanvasH;
    Vector2 m = GetMousePosition();
    Vector2 want{ Clamp((m.x / sw - 0.5f) * 2.f, -1.f, 1.f), Clamp((m.y / sh - 0.5f) * 2.f, -1.f, 1.f) };
    bool allow = (phase == Phase::Hub || phase == Phase::Intro) && Prog().settings.mouseLook && !settingsOpen && !helpOpen;
    if (!allow) want = Vector2{ 0, 0 };
    parallax.x = Damp(parallax.x, want.x, 2.5f, dt);
    parallax.y = Damp(parallax.y, want.y, 2.5f, dt);
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------
void App::RenderPagesToTexture() {
    if (!view) return;
    BeginTextureMode(pageRT);
    ClearBackground(pal::Paper);
    ui::Begin(Vector2{ 0, 0 }, 0.5f, false);
    view->Draw(false);
    ui::End();
    EndTextureMode();
}

App::Cam App::ComputeCam(float e) const {
    float sw = (float)pixel::CanvasW(), sh = (float)pixel::CanvasH;
    Cam c;
    c.spreadScale = std::min(0.5f, 0.97f * sw / 1560.f);
    float zFinal = c.spreadScale * (float)book::PageW / bookW;     // scale at which the desk book is as large as one page of the spread
    c.z = hudScale * powf(zFinal / hudScale, e);
    Vector2 a0{ (hudW - world::W) * 0.5f * hudScale + world::BookOnDesk.x * hudScale, world::BookOnDesk.y * hudScale };
    Vector2 a1{ sw * 0.5f, sh * 0.5f };
    c.anchor = Vector2Lerp(a0, a1, e);
    c.off = Vector2{ c.anchor.x - world::BookOnDesk.x * c.z, c.anchor.y - world::BookOnDesk.y * c.z };
    return c;
}

void App::DrawScene(const Cam& cam) {
    ui::Begin(cam.off, cam.z, false);
    stage.DrawBack(time, parallax);
    Vector2 s0 = stage.Shift(0, parallax), s1 = stage.Shift(1, parallax), s2 = stage.Shift(2, parallax);
    if (pullShow) {
        rlPushMatrix(); rlTranslatef(s0.x, s0.y, 0);
        Rectangle r = pullRect;
        art::Shadow(Vector2{ r.x + r.width * 0.5f + 6.f, r.y + r.height * 0.5f + 8.f }, r.width * 0.9f, r.height * 0.7f, 0.5f * pullAlpha);
        book::DrawSpine(r, WithAlpha(pullColor, pullAlpha), pullMark, true);
        rlPopMatrix();
    }
    bool libFar = lib.feet.y < 700.f;
    auto drawLib = [&]() { rlPushMatrix(); rlTranslatef(s1.x, s1.y, 0); lib.Draw(); rlPopMatrix(); };
    if (libFar) drawLib();
    stage.DrawChair(parallax);
    if (!libFar) drawLib();
    stage.DrawDesk(time, parallax);
    if (lib.behindDesk && lib.turn > 0.02f) {
        ui::PushClip(Rectangle{ -4000.f, world::RimY, 9000.f, 3000.f });
        rlPushMatrix(); rlTranslatef(s1.x, s1.y, 0);
        lib.DrawHandsOverDesk();
        rlPopMatrix();
        ui::PopClip();
    }
    if (bk.visible && !bookAsSpread && !lib.carrying) {
        rlPushMatrix(); rlTranslatef(s2.x, s2.y, 0);
        bk.DrawClosed(bookPos, bookW, bookSquash, bookRot, bookSquash < 0.95f);
        rlPopMatrix();
    }
    stage.DrawAtmosphere(time);
    ui::End();
}

void App::DrawDeskBackdrop(float alpha) {
    float sw = (float)pixel::CanvasW(), sh = (float)pixel::CanvasH;
    DrawRectangleGradientV(0, 0, (int)sw, (int)sh, WithAlpha(Color{ 40, 88, 64, 255 }, alpha), WithAlpha(Color{ 26, 62, 44, 255 }, alpha));
    // faint tooled border so the leather doesn't read as flat colour
    float m = 26.f * hudScale;
    DrawRectangleLinesEx(Rectangle{ m, m, sw - 2 * m, sh - 2 * m }, 2.f, WithAlpha(Color{ 202, 160, 74, 255 }, 0.25f * alpha));
    DrawRectangleLinesEx(Rectangle{ m + 8, m + 8, sw - 2 * m - 16, sh - 2 * m - 16 }, 1.f, WithAlpha(Color{ 202, 160, 74, 255 }, 0.15f * alpha));
}

void App::Render() {
    bool needRT = view && (phase == Phase::Presenting || phase == Phase::Closing);
    if (needRT) RenderPagesToTexture();
    stage.BuildLightmap(time, lib.HeadPos(), lib.seated ? 0.22f : 1.f);

    BeginDrawing();
    ClearBackground(BLACK);
    pixel::BeginCanvas();
    ClearBackground(Color{ 12, 9, 7, 255 });
    float sw = (float)pixel::CanvasW(), sh = (float)pixel::CanvasH;
    float e = EaseInOutCubic(bookBlend);
    Cam cam = ComputeCam(e);
    if (e < 0.995f) DrawScene(cam);
    if (e > 0.8f) DrawDeskBackdrop(Smooth((e - 0.8f) / 0.2f));
    DrawTexturePro(vignette, Rectangle{ 0, 0, (float)vignette.width, (float)vignette.height }, Rectangle{ 0, 0, sw, sh }, Vector2{ 0, 0 }, 0,
                   WithAlpha(WHITE, 0.62f - 0.3f * bookBlend));

    if (bookAsSpread && bk.visible) {
        float s = bookW * cam.z / (float)book::PageW;
        if (phase == Phase::Reading) bk.DrawBoard(cam.anchor, s);
        else bk.DrawSpread(cam.anchor, s, Lerp(bookSquash, 1.f, e), bookOpen, pageRT.texture);
    }
    if (phase == Phase::Reading && view) {
        float s = cam.spreadScale;
        pageRect = Rectangle{ sw * 0.5f - 780.f * s, sh * 0.5f - 500.f * s, 1560.f * s, 1000.f * s };
        ui::Begin(Vector2{ pageRect.x, pageRect.y }, pageRect.width / page::W, !settingsOpen && !helpOpen);
        view->Draw(!settingsOpen && !helpOpen);
        ui::End();
    }

    DrawHud();
    DrawOverlays();
    if (fade > 0.001f) DrawRectangle(0, 0, (int)sw, (int)sh, WithAlpha(BLACK, fade));
    pixel::EndCanvas();
    ui::EndFrameCursor();
    EndDrawing();
}

void App::DrawTabs(bool interactive) {
    hoverTab = -1;
    float tw = 330, th = 112, gap = 22;
    float total = 3 * th + 2 * gap;
    float y0 = (hudH - total) * 0.5f - 30;
    for (int i = 0; i < 6; i++) {
        bool left = i < 3;
        int row = i % 3;
        float slideIn = Smooth(hudVisible * 1.4f - row * 0.12f);
        Rectangle base{ left ? 18.f : hudW - 18.f - tw, y0 + row * (th + gap), tw, th };
        float off = (1.f - slideIn) * (tw + 40);
        base.x += left ? -off : off;
        bool hov = interactive && ui::Hover(base);
        if (hov) hoverTab = i;
        tabSlide[i] = Damp(tabSlide[i], hov ? 1.f : 0.f, 14.f, GetFrameTime());
        Rectangle r = base;
        r.x += (left ? 1.f : -1.f) * tabSlide[i] * 14.f;
        Color accent = kTabColors[i];
        DrawRectangleRounded(Rectangle{ r.x + 5, r.y + 7, r.width, r.height }, 0.16f, 8, WithAlpha(BLACK, 0.45f * slideIn));
        ui::Panel(r, WithAlpha(pal::Wood, slideIn), WithAlpha(pal::Gold, slideIn), 0.16f, 2.5f);
        Rectangle inner{ r.x + 8, r.y + 8, r.width - 16, r.height - 16 };
        ui::Panel(inner, WithAlpha(Mix(pal::Paper, pal::GoldSoft, tabSlide[i] * 0.25f), slideIn), WithAlpha(Shade(pal::Wood, 1.2f), slideIn), 0.14f, 1.5f);
        Vector2 ic{ left ? inner.x + 46 : inner.x + inner.width - 46, inner.y + inner.height * 0.5f };
        DrawCircleV(ic, 32, WithAlpha(accent, slideIn));
        DrawRing(ic, 32, 35, 0, 360, 40, WithAlpha(pal::Gold, slideIn));
        ui::DrawIcon(kTabIcons[i], ic, 30, WithAlpha(pal::GoldSoft, slideIn));
        float tx = left ? inner.x + 92 : inner.x + 16;
        ui::Text(FontId::SerifBold, kTabNames[i], tx, inner.y + 18, 25, WithAlpha(pal::Ink, slideIn));
        ui::Text(FontId::SerifItalic, kTabSubs[i], tx, inner.y + 52, 16, WithAlpha(pal::InkSoft, slideIn));
        // tiny mastery hint on the problem areas tab
        if (i == 4 && Prog().TotalAnswered() > 0) {
            ui::Text(FontId::Serif, TextFormat("Overall accuracy %d%%", (int)(Prog().OverallAccuracy() * 100 + 0.5f)), tx, inner.y + 72, 14, WithAlpha(pal::Red, slideIn));
        }
        if (i == 3) {
            runner::CompilerInfo ci = runner::Compiler();
            if (!ci.detecting && ci.kind == runner::CompilerKind::None) ui::Text(FontId::Serif, "No compiler found", tx, inner.y + 72, 14, WithAlpha(pal::Red, slideIn));
        }
        if (hov && ui::Pressed(base)) OnTab(i);
    }
}

void App::DrawHud() {
    bool modal = settingsOpen || helpOpen;
    hudVisible = Damp(hudVisible, (phase == Phase::Hub) ? 1.f : 0.f, 6.f, GetFrameTime());
    bool interactive = !modal;
    ui::Begin(Vector2{ 0, 0 }, hudScale, interactive);
    if (phase == Phase::Hub || hudVisible > 0.01f) DrawTabs(interactive && phase == Phase::Hub);
    // title plate
    float ta = Clamp01(1.f - bookBlend * 3.f);
    if (ta > 0) {
        ui::Text(FontId::SerifBold, "Malloc's Library", 30 + 2, 22 + 3, 38, WithAlpha(BLACK, 0.6f * ta));
        ui::Text(FontId::SerifBold, "Malloc's Library", 30, 22, 38, WithAlpha(pal::Gold, ta));
        ui::Text(FontId::SerifItalic, "an interactive C & C++ study hall", 32, 66, 17, WithAlpha(pal::GoldSoft, 0.85f * ta));
    }
    // top-right buttons
    if (phase == Phase::Hub) {
        Rectangle help{ hudW - 132, 22, 48, 48 }, gear{ hudW - 76, 22, 48, 48 };
        if (ui::IconButton(help, Icon::Help, ui::BtnWood, interactive)) { helpOpen = !helpOpen; settingsOpen = false; sfx::Play(sfx::Click); }
        if (ui::IconButton(gear, Icon::Gear, ui::BtnWood, interactive)) { settingsOpen = !settingsOpen; helpOpen = false; confirmReset = false; sfx::Play(sfx::Click); }
    }
    // dialogue
    if (phase != Phase::Reading && phase != Phase::Presenting) dlg.Draw(hudW, hudH, interactive && (phase == Phase::Hub || phase == Phase::Fetching || phase == Phase::Closing));
    // hints
    if (phase == Phase::Fetching || phase == Phase::Closing) {
        std::string h = "Hold SPACE (or the right mouse button) to hurry Mr. Malloc along";
        float w = ui::MeasureWidth(FontId::SerifItalic, h, 17);
        float a = 0.55f + 0.25f * sinf(time * 2.f);
        ui::Text(FontId::SerifItalic, h, hudW - w - 34, 34, 17, WithAlpha(pal::GoldSoft, a * ta));
    }
    if (phase == Phase::Reading && pageRect.width > 0) {
        float bottom = (pageRect.y + pageRect.height) / hudScale;
        if (hudH - bottom > 24) ui::Text(FontId::SerifItalic, "Esc returns the book to Mr. Malloc   -   F11 full screen", 30, hudH - 30, 15, WithAlpha(pal::GoldSoft, 0.6f));
    }
    if (phase == Phase::Hub && interactive) HandleWorldClicks();
    ui::End();
}

void App::HandleWorldClicks() {
    if (!ui::AnyPress() || dlg.ChoicesShowing()) return;
    Vector2 m = MouseScene();
    Vector2 near = stage.Shift(2, parallax), mid = stage.Shift(1, parallax);
    if (stage.HitCat(Vector2{ m.x - near.x, m.y - near.y })) {
        ui::ConsumePress();
        stage.catPet = 2.5f;
        sfx::Play(sfx::Hover, 0.8f, 0.6f);
        dlg.Say("That's **Segfault**, the library cat. He sleeps on the warm end of the desk and wanders into memory he doesn't own. Don't dereference him.");
        return;
    }
    if (stage.HitBell(Vector2{ m.x - near.x, m.y - near.y })) {
        ui::ConsumePress();
        stage.bellRing = 1.2f;
        sfx::Play(sfx::Bell, 0.7f);
        dlg.Say("Yes, yes, I'm right here! No need to ring. What can I fetch for you?");
        return;
    }
    if (lib.HitTest(Vector2{ m.x - mid.x, m.y - mid.y })) {
        ui::ConsumePress();
        dlg.Say(kQuips[funIndex % (int)(sizeof(kQuips) / sizeof(kQuips[0]))]);
        funIndex++;
        lib.excitement = 0.6f;
    }
}

void App::DrawOverlays() {
    overlayAnim = Damp(overlayAnim, (settingsOpen || helpOpen) ? 1.f : 0.f, 12.f, GetFrameTime());
    lib.excitement = Damp(lib.excitement, 0.f, 2.f, GetFrameTime());
    if (overlayAnim < 0.01f) return;
    ui::Begin(Vector2{ 0, 0 }, hudScale, true);
    DrawRectangle(0, 0, (int)hudW + 2, (int)hudH, WithAlpha(BLACK, 0.5f * overlayAnim));
    if (settingsOpen) DrawSettings();
    if (helpOpen) DrawHelp();
    ui::End();
}

void App::DrawSettings() {
    Rectangle r{ hudW * 0.5f - 330, 150 + (1 - overlayAnim) * 30, 660, 560 };
    DrawRectangleRounded(Rectangle{ r.x + 6, r.y + 8, r.width, r.height }, 0.06f, 8, WithAlpha(BLACK, 0.4f));
    ui::Panel(r, pal::Paper, pal::Wood, 0.06f, 3.f);
    ui::Text(FontId::SerifBold, "Settings", r.x + 34, r.y + 26, 34, pal::Ink);
    Settings& s = Prog().settings;
    float y = r.y + 96;
    ui::Text(FontId::Serif, "Volume", r.x + 34, y, 22, pal::Ink);
    if (ui::Slider(Rectangle{ r.x + 200, y + 2, 340, 26 }, &s.volume)) ApplySettings();
    y += 56;
    if (ui::Checkbox(Rectangle{ r.x + 34, y, 520, 34 }, "Mr. Malloc's mumbling voice", &s.voice)) ApplySettings();
    y += 48;
    ui::Checkbox(Rectangle{ r.x + 34, y, 520, 34 }, "Quick fetch (he walks twice as fast)", &s.quickFetch);
    y += 48;
    ui::Checkbox(Rectangle{ r.x + 34, y, 520, 34 }, "Look around with the mouse", &s.mouseLook);
    y += 60;
    if (ui::Button(Rectangle{ r.x + 34, y, 250, 48 }, IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) ? "Leave full screen" : "Full screen (F11)", ui::BtnWood, true, 18)) {
        ToggleBorderlessWindowed();
        s.fullscreen = !s.fullscreen;
    }
    if (ui::Button(Rectangle{ r.x + 300, y, 266, 48 }, confirmReset ? "Really erase everything?" : "Reset all progress", ui::BtnDanger, true, 17)) {
        if (confirmReset) {
            Prog().Reset();
            stage.pileCount = 0;
            confirmReset = false;
            dlg.Say("Done - a fresh ledger, a clean slate. I've even dusted the desk.");
            settingsOpen = false;
        } else confirmReset = true;
    }
    y += 72;
    ui::Text(FontId::SerifItalic, "Progress is saved in the save/ folder next to the program.", r.x + 34, y, 16, pal::InkSoft);
    if (ui::Button(Rectangle{ r.x + r.width - 160, r.y + r.height - 70, 126, 48 }, "Done", ui::BtnPrimary, true, 20)) { settingsOpen = false; Prog().Save(); }
}

void App::DrawHelp() {
    Rectangle r{ hudW * 0.5f - 450, 40 + (1 - overlayAnim) * 30, 900, 820 };
    DrawRectangleRounded(Rectangle{ r.x + 6, r.y + 8, r.width, r.height }, 0.05f, 8, WithAlpha(BLACK, 0.4f));
    ui::Panel(r, pal::Paper, pal::Wood, 0.05f, 3.f);
    ui::Text(FontId::SerifBold, "How the library works", r.x + 36, r.y + 26, 34, pal::Ink);
    float y = r.y + 84;
    const char* lines[] = {
        "Pick a service from the cards on either side. **Mr. Malloc** will ask what you need, fetch the right book from the shelves and open it on the desk for you.",
        "**Reading Room** - tutorials written from your lecture notes, with memory diagrams and exam tips.",
        "**Multiple Choice / Written Test** - topic quizzes or mixed exam papers. Wrong answers are remembered and served again in drills.",
        "**Coding Exams** - the Lab 1 and Pointers Lab exercises, LeetCode style. Write the function, press Run for sample tests, Submit for the hidden ones. A timed **Mock Exam** mirrors the real lab exam.",
        "**Problem Areas** - your weakest topics, worked out from every answer you give, plus a targeted drill.",
        "**Past Results** - every session in the ledger, with your score trend.",
    };
    for (const char* l : lines) y += ui::Rich(l, r.x + 36, y, r.width - 72, 19, pal::Ink, 1.4f) + 10;
    y += 8;
    ui::Text(FontId::SerifBold, "Controls", r.x + 36, y, 22, pal::Ink);
    y += 34;
    const char* keys[] = {
        "Esc  -  return the book / close a menu", "Space or right mouse (hold)  -  hurry the librarian", "1-4 / A-D  -  answer multiple choice",
        "Left / Right  -  turn pages", "Ctrl+Enter  -  run code,  Ctrl+Shift+Enter  -  submit", "F11  -  full screen,  F12  -  screenshot",
        "Click the librarian, the bell or the cat...",
    };
    for (int i = 0; i < 7; i++) {
        ui::Text(FontId::Serif, keys[i], r.x + 36, y + i * 28, 16, pal::InkSoft);
    }
    if (ui::Button(Rectangle{ r.x + r.width - 160, r.y + r.height - 70, 126, 48 }, "Close", ui::BtnPrimary, true, 20)) helpOpen = false;
}

// ---------------------------------------------------------------------------
// autopilot - scripted input used to record demos and verification screenshots
// ---------------------------------------------------------------------------
void App::LoadScript(const std::string& path) {
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t sp = line.find(' ');
        AutoCmd c{ sp == std::string::npos ? line : line.substr(0, sp), sp == std::string::npos ? "" : Trim(line.substr(sp + 1)) };
        autoCmds.push_back(c);
    }
}

void App::SaveShot(const std::string& name) {
    Image img = LoadImageFromScreen();
    std::string p = SavePath("shots/" + name + ".png");
    ExportImage(img, p.c_str());
    UnloadImage(img);
    TraceLog(LOG_INFO, "SHOT: %s", p.c_str());
}

void App::RunAutopilot(float dt) {
    if (autoIdx >= autoCmds.size()) return;
    if (autoWait > 0) { autoWait -= dt; return; }
    while (autoIdx < autoCmds.size()) {
        const AutoCmd& c = autoCmds[autoIdx];
        if (c.cmd == "wait") { autoWait = (float)atof(c.arg.c_str()); autoIdx++; return; }
        if (c.cmd == "phase") {
            const char* names[] = { "intro", "hub", "fetching", "presenting", "reading", "closing" };
            if (c.arg != names[(int)phase]) return;
            autoIdx++; continue;
        }
        if (c.cmd == "shot") { SaveShot(c.arg); autoIdx++; autoWait = 0.05f; return; }
        if (c.cmd == "tab") { OnTab(atoi(c.arg.c_str())); dlg.Finish(); autoIdx++; continue; }
        if (c.cmd == "pick") { dlg.Finish(); dlg.Pick(atoi(c.arg.c_str())); autoIdx++; continue; }
        if (c.cmd == "finish") { dlg.Finish(); autoIdx++; continue; }
        if (c.cmd == "debug") { if (view) view->DebugCommand(c.arg); autoIdx++; continue; }
        if (c.cmd == "close") { if (phase == Phase::Reading) BeginClosing(); autoIdx++; continue; }
        if (c.cmd == "mouse") {
            auto p = Split(c.arg, ' ');
            if (p.size() >= 2) SetMousePosition((int)(atof(p[0].c_str()) * GetScreenWidth()), (int)(atof(p[1].c_str()) * GetScreenHeight()));
            autoIdx++; continue;
        }
        if (c.cmd == "say") { dlg.Say(c.arg); dlg.Finish(); autoIdx++; continue; }
        if (c.cmd == "quick") { Prog().settings.quickFetch = c.arg == "1"; autoIdx++; continue; }
        if (c.cmd == "help") { helpOpen = c.arg == "1"; autoIdx++; continue; }
        if (c.cmd == "settings") { settingsOpen = c.arg == "1"; autoIdx++; continue; }
        if (c.cmd == "quit") { autoQuit = true; autoIdx++; return; }
        autoIdx++;
    }
}
