// app.cpp - Malloc's Library: the director that runs the whole show
#include "app.h"
#include "gfx.h"
#include "ui.h"
#include "audio.h"
#include "content.h"
#include "progress.h"
#include "code_runner.h"
#include "unlocks.h"
#include "rlgl.h"
#include <algorithm>
#include <fstream>
#include <sstream>

using namespace util;
namespace pal = ui::pal;

static const char* kTabNames[6] = { "Reading Room", "Multiple Choice", "Written Test", "Coding Exams", "Problem Areas", "Past Results" };
static const char* kTabSubs[6] = { "Tutorials from your lectures", "Quick-fire quizzes", "Predict, fill in & fix", "Lab exam practice", "Your weak spots, diagnosed", "The ledger of results" };
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
// each librarian talks in their own voice
// ---------------------------------------------------------------------------
struct Persona {
    float pitch;
    std::vector<std::string> quips;       // said in turn when the librarian is clicked
    std::string bell, cat, select;
};

// the name of whoever is on duty, for hints and labels that mention the librarian
static std::string DutyName() {
    const LibrarianDef* d = unlocks::Find(Prog().librarian);
    return d ? d->name : "the librarian";
}

static const Persona& PersonaFor(const std::string& id) {
    static const Persona malloc_{
        1.0f, std::vector<std::string>(std::begin(kQuips), std::end(kQuips)),
        "Yes, yes, I'm right here! No need to ring. What can I fetch for you?",
        "That's **Segfault**, the library cat. He sleeps on the warm end of the desk and wanders into memory he doesn't own. Don't dereference him.",
        "Back behind my own desk. **Mr. Malloc**, as ever. Shall we?" };
    static const Persona pointer{
        0.78f, {
            "Hark! A pointer is but a scroll bearing the *address* of another scroll. Read the address, and thou shalt find the treasure.",
            "Beware the dangling pointer, young scholar! It points to a spell already unmade. `delete` first, then bind it to `nullptr`.",
            "By my beard! `int *p = &x;` - here `p` holds the WHERE of `x`, and `*p` conjures the WHAT.",
            "Pointer arithmetic is sorcery with limits: `p + 1` leaps not one byte, but one whole `sizeof(*p)`.",
            "Every `new` demands its `delete`, as every summoning demands a banishing. Forget it, and thy memory shall leak like a cracked cauldron.",
            "An array name, cast upon a function, decays into a pointer to its first element. Alas, its length is lost to the mists.",
            "Verily, `nullptr` is the empty scroll. Never dereference it, lest the Segfault Spirits take thee!",
            "A linked list is a procession of wizards, each pointing to the next. The last holds `nullptr`, and bows out.",
            "Mark well: `const int *p` guards the VALUE; `int *const p` guards the POINTER. Read the declaration from the name outwards.",
            "A double pointer is a map to a map. `int **pp` - two stars, two journeys.",
            "Behold: `p->x` is but a courtly bow for `(*p).x`.",
            "My hat is not merely for show. It collects stray memory. ...Also it is slightly leaking.",
            "Ask not whose memory it is, but WHO SHALL FREE IT. Ownership, apprentice - always know the owner.",
        },
        "Ring thy bell not so, apprentice! I am here. What scroll dost thou seek?",
        "Behold **Segfault**, familiar of this library. He wanders into memory he owns not. Do not dereference him.",
        "Ah, a fresh pair of eyes! **Professor Pointer** at thy service. I know where everything lives. Mostly." };
    static const Persona heap{
        1.14f, {
            "Fascinating! Did you know the heap grows when you call `new`? Hypothesis: you will forget to `delete`. Let's test!",
            "Excellent! EXCELLENT! Stack memory vanishes when the function ends; heap memory stays until YOU free it. Science!",
            "Careful - a memory leak is just an experiment you forgot to clean up. Please wear goggles.",
            "Whoa! Did you see that? `vector::push_back` may reallocate and invalidate your iterators. Kaboom!",
            "Today's experiment: `sizeof(int)` on this machine. Result: probably 4. Write it in your lab notebook!",
            "My research shows 90% of bugs are off-by-one errors. The other 10% are also off-by-one errors.",
            "Eureka! `delete[]` for arrays, `delete` for single objects. Mix them up and - undefined behaviour. Thrilling!",
            "Note to self: uninitialised variables hold garbage. Fascinating garbage, but garbage nonetheless.",
            "Yes yes yes! Use RAII - let destructors free your resources while you do something more fun.",
            "Don't touch that! ...Oh, it's only a `const` reference. Touch away.",
            "Ha! A copy constructor copies; an assignment operator assigns. Rule of three, my friend, RULE OF THREE!",
            "The heap is like my lab: huge, messy, and absolutely nobody remembers who allocated what.",
            "Peer review says: pass big objects by `const&`. Peer review is right.",
        },
        "Ding! Eureka - a visitor! What shall we investigate today?",
        "Subject: **Segfault**, feline. Observed sleeping 23 hours a day. Fascinating! Do NOT poke him with a pointer.",
        "**Dr. Heap** reporting for duty! Please keep all hands, feet and dangling pointers inside the library." };
    static const Persona ada{
        1.32f, {
            "ok ok ok - quick tip: compile early, compile often. Errors are way cheaper in small doses.",
            "pro tip: name your variables like someone else has to read them. That someone is you, tomorrow.",
            "hot take: `auto` is great, just not when it hides a type you needed to see.",
            "bug? don't panic. Rubber-duck it. Explain it line by line. Nine times out of ten the duck wins.",
            "`for (int i = 0; i <= n; i++)` - spot the off-by-one? Yeah. It gets everyone. EVERYONE.",
            "ship it! ...after you've tested the edge cases. Empty input, one element, negatives. THEN ship it.",
            "templates are just copy-paste with a brain. The compiler does the copying.",
            "reading compiler errors: start at the FIRST one. The rest are usually just echoes.",
            "oh! `std::vector` over raw arrays whenever you can. Less pain, same speed.",
            "if it compiles first time, be suspicious. Seriously.",
            "fun fact: `i++` returns the old value, `++i` the new one. Overloading them? Don't mix them up!",
            "i name my test cases after my enemies. `test_null_pointer_dave` has failed me twice.",
            "you've got this. Every expert was once stuck on a missing semicolon.",
        },
        "ding ding! I'm here, I'm here. What are we building?",
        "that's **Segfault**. Best QA engineer we have - crashes on everything. Don't wake him.",
        "Hi! **Ada** here. Let's get compiling - and mind your semicolons." };
    static const Persona robot{
        0.62f, {
            "BEEP. QUERY RECEIVED: CLICK DETECTED. EXCELLENT. PLEASE CONTINUE CLICKING.",
            "COMPUTING... 42. THIS IS NOT RELATED TO YOUR EXAM, BUT IT IS ALWAYS 42.",
            "REMINDER: `nullptr` DEREFERENCE = SYSTEM CRASH. I HAVE SEEN THINGS. DO NOT DEREFERENCE.",
            "BZZT. MY MEMORY HAS ZERO LEAKS. ALL ALLOCATIONS HAVE BEEN FREED. I AM VERY PROUD OF THIS.",
            "FACT: AN `int` IS USUALLY 4 BYTES. I AM NAMED AFTER ONE BYTE. THIS IS A COINCIDENCE. (IT IS NOT.)",
            "WARNING: OFF-BY-ONE ERROR DETECTED IN HUMAN. SUGGESTED FIX: RECOUNT FROM ZERO.",
            "BEEP BOOP. A `for` LOOP RUNS WHILE ITS CONDITION IS TRUE. I RUN WHILE MY BATTERY IS CHARGED.",
            "LOADING JOKE.EXE... WHY DID THE FUNCTION RETURN EARLY? IT HAD NOTHING MORE TO GIVE. HA. HA. HA.",
            "TIP: PASS BY `const` REFERENCE. COPYING IS INEFFICIENT. EFFICIENCY IS HAPPINESS.",
            "ANALYSIS: YOU ARE DOING WELL. CONFIDENCE LEVEL: 87%. MARGIN OF ERROR: ONE CAT.",
            "WHIRRR. MY SERVOS RUN ON TEA AND CLEAN CODE. THE TEA IS OPTIONAL.",
            "RECURSION: SEE `RECURSION`.",
            "A `struct` AND A `class` DIFFER ONLY IN DEFAULT ACCESS. I FIND THIS... SATISFYING.",
        },
        "DING DETECTED. ATTENTION ROUTED TO USER. HOW MAY I ASSIST?",
        "FELINE UNIT **SEGFAULT** LOCATED. STATUS: ASLEEP. THREAT LEVEL: ADORABLE.",
        "SYSTEMS ONLINE. **B.Y.T.E.** AT YOUR SERVICE. LET US LEARN, HUMAN." };
    static const Persona captain{
        0.72f, {
            "Ahoy! The stack be a pile of plates, matey: the last one ye PUSH is the first one ye POP. Last in, first out!",
            "Every function call lays a fresh frame on the stack, and when she returns the frame goes overboard. So never hand back the address of a local - it sinks with the ship!",
            "Stack overflow? That be recursion with no base case. She keeps sailing deeper until she hits the bottom of the sea.",
            "Local variables live on the stack and vanish when the function ends. Here one minute, scraped off the hull the next.",
            "The stack tidies up after itself. The heap makes ye swab yer own deck: every `new` needs its `delete`.",
            "A stack frame holds yer locals, yer parameters and the return address. That last one be the map home.",
            "Recursion be a ship in a bottle in a bottle in a bottle. Know when to stop: that be yer base case.",
            "`std::stack` gives ye `push`, `pop` and `top`. No peeking at the bottom, sailor!",
            "Matching brackets be the classic stack puzzle. Push on every open, pop on every close. Empty at the end? Shipshape.",
            "Check yer array bounds, or ye'll sail right off the edge of the map. That be undefined behaviour.",
            "Man overboard? That be a dangling reference. Don't dive in after it.",
            "Steady as she goes: one thing at a time, in order, and ye'll make port before the exam starts.",
        },
        "Ahoy! No need to ring the bell, matey. I heard ye from the crow's nest. What do ye need?",
        "That be **Segfault**, the ship's cat. Best mouser on the seven seas, worst sailor. Keep him away from the heap.",
        "All hands on deck! **Captain Stack** takes the helm. Last in, first out, and I run a tight ship." };
    static const Persona nana{
        1.22f, {
            "Oh, sweetheart. Whatever you `new`, you must `delete`. I always wash my own teacup.",
            "Smart pointers, dear! A `unique_ptr` is a teacup with one owner. A `shared_ptr` is the biscuit tin: the last one out turns off the light.",
            "Never `delete` the same thing twice, love. That's like drying a dish that's already in the cupboard.",
            "A memory leak is a tap left running. Pop upstairs and turn it off.",
            "Initialise your variables, darling. An uninitialised one holds whatever the last tenant left behind.",
            "`const` is just good manners: it says 'I'll look, I won't touch'.",
            "Pass big things by reference, dear. You wouldn't post the whole piano to your sister, you'd just give her the address.",
            "In my day we only had pointers and arrays. And we were grateful!",
            "Check for `nullptr` before you use a pointer, dear. Knock before you enter.",
            "RAII, my darling: grab the resource in the constructor, let go in the destructor, and the house cleans itself.",
            "A copy constructor is like a recipe card: copy the whole card, not just the address of mine.",
            "Mind the rule of three: if you need a destructor, you probably need a copy constructor and a copy assignment too.",
            "You're doing ever so well. Have a biscuit. Then try question seven again.",
        },
        "Ooh, the bell! Hello, dear. I'm right here. Shall I put the kettle on?",
        "That's **Segfault**, dear. He's allowed on the desk because he sleeps so nicely. He's never been dereferenced, I checked.",
        "Hello, my darlings! **Nana Free** here. I always free what I allocate. Now - tea first, then some questions?" };
    static const Persona punk{
        0.9f, {
            "AAAH! ...oh, it's just a `nullptr`. Only dereference it if you want to see me panic. Kernel panic. Get it?",
            "Buffer overflow: you write eleven things into a box that holds ten, and the eleventh lands somewhere it shouldn't. Check your bounds. Always.",
            "Undefined behaviour means ANYTHING can happen. It might work. It might not. It will definitely fail during your demo.",
            "Off-by-one: `i <= n` instead of `i < n`. Guaranteed chaos. I've cried over that one.",
            "Segfault? Congrats, you touched memory that isn't yours. The OS is not impressed.",
            "Uninitialised variable? It holds whatever garbage was there before. Like a locker nobody cleaned.",
            "Stack overflow: infinite recursion eats the call stack until there's none left. Boom. Not the website - the actual thing.",
            "Don't `delete` twice. Double free. The heap just goes: nope.",
            "Dangling pointer: it points at memory you gave back. A ghost holding a house key.",
            "Turn the compiler warnings all the way up. Seriously. It's free therapy.",
            "I only panic when it's `nullptr`. Or when it isn't `nullptr`. Or on Tuesdays.",
            "Magic numbers in your code? Name them. `const int MAX_USERS = 100;` Your future self will thank you.",
            "Breathe in. Check for `nullptr`. Breathe out. Everything's fine.",
        },
        "AAH! ...oh, it's the bell. Don't do that. I'm fine. I'm FINE. What do you need?",
        "That's **Segfault**. He's the only one in here who never panics. Honestly? Goals.",
        "Yo. **Kernel Panic**. Don't panic. ...I'm panicking a bit. Let's learn something." };
    if (id == "stack") return captain;
    if (id == "nana") return nana;
    if (id == "kernel") return punk;
    if (id == "pointer") return pointer;
    if (id == "heap") return heap;
    if (id == "ada") return ada;
    if (id == "byte") return robot;
    return malloc_;
}

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void App::Init() {
    Prog().Load();
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Malloc's Library - an interactive C & C++ study hall");
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
    sfx::Init();
    gfx::Init();
    Data().Load();
    scene.Build();
    lib.Init();
    lib.onFootstep = [this]() { sfx::Play(sfx::Footstep, 0.32f, 0.9f + 0.2f * fmodf(time * 7.f, 1.f)); };
    bk.Init();
    pageRT = LoadRenderTexture(book::PageTexW, book::PageTexH);
    Image v = GenImageGradientRadial(640, 360, 0.25f, Color{ 0, 0, 0, 0 }, Color{ 0, 0, 0, 255 });
    vignette = LoadTextureFromImage(v);
    UnloadImage(v);
    SetTextureFilter(vignette, TEXTURE_FILTER_BILINEAR);
    runner::DetectAsync();
    scene.pileCount = std::min(9, Prog().booksReturned);
    ApplySettings();
    if (Prog().settings.fullscreen) ToggleBorderlessWindowed();

    cam.position = Vector3{ 0, 1.33f, 1.95f };
    cam.target = Vector3{ 0, 1.17f, -0.4f };
    cam.up = Vector3{ 0, 1, 0 };
    cam.fovy = 42.f;
    cam.projection = CAMERA_PERSPECTIVE;
    lib.lookAt = Vector3{ 0.1f, 0.8f, 0.3f };

    // retroactive unlocks from saved history (never taken away once earned), and who is on duty
    std::vector<std::string> fresh = unlocks::Check();
    if (!Prog().sessions.empty()) {
        for (const std::string& id : fresh)
            if (const LibrarianDef* d = unlocks::Find(id)) unlockNote += " **New librarian unlocked: " + d->name + "!** Find them under the star button, top right.";
        newLibBadge = !fresh.empty();
    }
    if (!unlocks::IsUnlocked(Prog().librarian)) Prog().librarian = "malloc";
    ApplyLibrarian();
    SetPhase(Phase::Intro);
}

// puts the librarian on duty: look, desk nameplate, speaker name
void App::ApplyLibrarian() {
    const std::string& id = Prog().librarian;
    const LibrarianDef* d = unlocks::Find(id);
    if (!d) return;
    lib.look = id;
    scene.SetNameplate(d->plate);
    dlg.SetSpeaker(d->name);
    dlg.SetVoicePitch(PersonaFor(id).pitch);
}

void App::SelectLibrarian(const std::string& id) {
    if (!unlocks::IsUnlocked(id) || Prog().librarian == id) return;
    Prog().librarian = id;
    Prog().Save();
    ApplyLibrarian();
    sfx::Play(sfx::Click);
    dlg.Say(PersonaFor(id).select);
    libsOpen = false;
}

void App::ApplySettings() {
    sfx::SetMasterVolume(Prog().settings.volume);
    sfx::SetVoiceEnabled(Prog().settings.voice);
}

void App::Shutdown() {
    if (view) { view->OnClose(); view.reset(); }
    Prog().Save();
    UnloadRenderTexture(pageRT);
    for (auto& kv : portraits) UnloadRenderTexture(kv.second.rt);
    UnloadTexture(vignette);
    bk.Shutdown();
    Librarian::FreeMeshes();
    scene.Shutdown();
    gfx::Shutdown();
    ui::UnloadFonts();
    sfx::Shutdown();
    CloseWindow();
}

int App::Run(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--script" && i + 1 < argc) LoadScript(argv[++i]);
        if (a == "--sandbox") Prog().readOnly = true;   // scripted test runs: never write the save file
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
    settingsOpen = helpOpen = libsOpen = false;
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
    int si = scene.FindShelfBook(ShelfKeyFor(r));
    int bay = 0;
    if (si >= 0) {
        float x = gfx::Origin(scene.ShelfBooks()[(size_t)si].inShelf).x;
        bay = (int)floorf(x / world::BayWidth + 3.0f);
    }
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
        int si = scene.FindShelfBook(ShelfKeyFor(r));
        l.cover = si >= 0 ? scene.ShelfBooks()[(size_t)si].color : Color{ 120, 100, 80, 255 };
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
    bk.open = 0;
    bk.pagesTextured = false;
    bool queueLine = dlg.Visible() && phase == Phase::Hub && !pendingRemark.empty();
    pendingRemark.clear();
    if (r.kind == "areas" || r.kind == "stats") {
        shelfIndex = -1;
        std::string line = r.kind == "areas" ? "Problem areas? Let me consult my ledger... I keep notes on everything you get wrong. Purely academic, of course."
                                             : "The ledger of past results. I keep it in the drawer, away from prying eyes.";
        queueLine ? dlg.Queue(line) : dlg.Say(line);
        BuildDrawerFetch();
    } else {
        shelfIndex = scene.FindShelfBook(ShelfKeyFor(r));
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
    s.enter = [this]() { lib.seated = false; lib.gestureAmount = 0; };
    s.tick = [this](float t, float) {
        float e = Smooth(t);
        Pose p = LerpPose(poses::Sit(), poses::Stand(), e);
        p.spine += sinf(t * PI) * 0.42f;
        lib.base = p;
        lib.pos.z = Lerp(world::SeatPos.z, -0.36f, e);
        scene.chairBack = Smooth(t * 1.4f);
    };
    return s;
}

App::Step App::SitDown() {
    Step s;
    s.dur = 1.1f;
    auto from = std::make_shared<Vector3>();
    auto yaw0 = std::make_shared<float>(0.f);
    s.enter = [this, from, yaw0]() { *from = lib.pos; *yaw0 = lib.yaw; };
    s.tick = [this, from, yaw0](float t, float) {
        float e = Smooth(t);
        Pose p = LerpPose(poses::Stand(), poses::Sit(), e);
        p.spine += sinf(t * PI) * 0.38f;
        lib.base = p;
        lib.pos = Vector3Lerp(*from, world::SeatPos, e);
        lib.yaw = LerpAngle(*yaw0, 0.f, Smooth(t * 2.f));
        scene.chairBack = 1.f - Smooth((t - 0.2f) / 0.8f);
    };
    s.exit = [this]() { lib.seated = true; lib.gestureAmount = 1; lib.yaw = 0; lib.pos = world::SeatPos; scene.chairBack = 0; lib.base = poses::Sit(); };
    return s;
}

App::Step App::WalkTo(Vector3 target, float speed) {
    Step s;
    auto from = std::make_shared<Vector3>();
    s.enter = [this, from, target, speed]() {
        *from = lib.pos;
        float d = Vector3Distance(Vector3{ from->x, 0, from->z }, Vector3{ target.x, 0, target.z });
        steps[(size_t)stepIdx].dur = std::max(0.25f, d / speed);
    };
    s.tick = [this, from, target](float t, float dt) {
        Vector3 prev = lib.pos;
        lib.pos = Vector3Lerp(*from, target, t);
        lib.pos.y = 0;
        Vector3 d = Vector3Subtract(target, *from);
        if (fabsf(d.x) + fabsf(d.z) > 0.01f) lib.yaw = DampAngle(lib.yaw, atan2f(d.x, d.z), 9.f, dt);
        lib.walk = Damp(lib.walk, 1.f, 7.f, dt);
        float moved = Vector3Distance(prev, lib.pos);
        lib.walkPhase += moved / 0.53f * PI;
        lib.base = lib.carrying ? poses::Carry() : poses::Stand();
    };
    return s;
}

App::Step App::TurnTo(float yaw, float dur) {
    Step s;
    s.dur = dur;
    auto y0 = std::make_shared<float>(0.f);
    s.enter = [this, y0]() { *y0 = lib.yaw; };
    s.tick = [this, y0, yaw](float t, float dt) {
        lib.yaw = LerpAngle(*y0, yaw, Smooth(t));
        lib.walk = Damp(lib.walk, 0.3f, 8.f, dt);
        lib.walkPhase += dt * 7.f;
        lib.base = lib.carrying ? poses::Carry() : poses::Stand();
    };
    return s;
}

void App::BuildShelfFetch() {
    steps.clear();
    stepIdx = 0; stepT = 0; stepStarted = false;
    const ShelfBook& sb = scene.Shelf(shelfIndex);
    Vector3 stand = sb.standPos;
    Vector3 w1{ 0.62f, 0, -0.46f }, w2{ 0.62f, 0, -1.25f };
    bool low = sb.reachY < 0.9f;
    steps.push_back(StandUp());
    steps.push_back(WalkTo(w1, 0.9f));
    steps.push_back(WalkTo(w2));
    steps.push_back(WalkTo(stand));
    steps.push_back(TurnTo(PI, 0.55f));
    {   // reach for the book and take it
        Step s;
        s.dur = 1.8f;
        s.enter = [this]() { lib.carrying = false; };
        s.tick = [this, low](float t, float dt) {
            ShelfBook& b = scene.Shelf(shelfIndex);
            lib.walk = Damp(lib.walk, 0.f, 8.f, dt);
            Vector3 grip = Vector3Transform(Vector3{ 0.02f, book::PageY, 0 }, b.inShelf);
            Pose stance = low ? poses::Crouch() : poses::Stand();
            Pose reach = lib.ReachPose(grip, 0, stance);
            Pose p;
            if (t < 0.38f) p = LerpPose(poses::Stand(), reach, Smooth(t / 0.38f));
            else if (t < 0.7f) p = reach;
            else p = LerpPose(reach, poses::Carry(), Smooth((t - 0.7f) / 0.3f));
            lib.base = p;
            lib.lookAt = grip;
            Matrix pulled = MatrixMultiply(b.inShelf, MatrixTranslate(0, 0, 0.26f));
            if (t >= 0.4f && b.present) { b.present = false; bk.visible = true; bk.world = b.inShelf; sfx::Play(sfx::Page, 0.35f, 0.8f); }
            if (t >= 0.4f && t < 0.62f) bk.world = gfx::LerpRigid(b.inShelf, pulled, Smooth((t - 0.4f) / 0.22f));
            else if (t >= 0.62f) bk.world = gfx::LerpRigid(pulled, lib.CarryMatrix(), Smooth((t - 0.62f) / 0.38f));
        };
        s.exit = [this]() { lib.carrying = true; lib.base = poses::Carry(); dlg.Queue("Ah - here it is."); dlg.Finish(); };
        steps.push_back(s);
    }
    steps.push_back(TurnTo(0.f, 0.7f));
    steps.push_back(WalkTo(w2));
    steps.push_back(WalkTo(w1));
    steps.push_back(WalkTo(Vector3{ 0.f, 0.f, -0.38f }, 0.8f));
    {   // set the book down on the desk and slide it across
        Step s;
        s.dur = 1.6f;
        auto thud = std::make_shared<bool>(false);
        auto start = std::make_shared<Matrix>();
        s.enter = [this, thud, start]() { *thud = false; *start = lib.CarryMatrix(); };
        s.tick = [this, thud, start](float t, float dt) {
            lib.walk = Damp(lib.walk, 0.f, 8.f, dt);
            lib.yaw = DampAngle(lib.yaw, 0.f, 8.f, dt);
            Pose p;
            if (t < 0.45f) p = LerpPose(poses::Carry(), poses::PlaceOnDesk(), Smooth(t / 0.45f));
            else if (t < 0.65f) p = poses::PlaceOnDesk();
            else p = LerpPose(poses::PlaceOnDesk(), poses::Stand(), Smooth((t - 0.65f) / 0.35f));
            lib.base = p;
            Matrix desk = MatrixTranslate(world::BookOnDesk.x, world::BookOnDesk.y, world::BookOnDesk.z);
            Matrix mid = MatrixTranslate(0.f, world::DeskTopY + 0.002f, 0.22f);
            Matrix lifted = MatrixTranslate(0.f, world::DeskTopY + 0.08f, 0.16f);
            if (t < 0.25f) bk.world = gfx::LerpRigid(*start, lifted, Smooth(t / 0.25f));
            else if (t < 0.45f) bk.world = gfx::LerpRigid(lifted, mid, Smooth((t - 0.25f) / 0.2f));
            else bk.world = gfx::LerpRigid(mid, desk, EaseOutCubic((t - 0.45f) / 0.5f));
            if (t >= 0.45f && !*thud) { *thud = true; sfx::Play(sfx::Thud, 0.8f); lib.carrying = false; }
            lib.lookAt = gfx::Origin(bk.world);
        };
        s.exit = [this]() { lib.carrying = false; bk.world = MatrixTranslate(world::BookOnDesk.x, world::BookOnDesk.y, world::BookOnDesk.z); dlg.Queue("There we are. Let's see what you make of it."); dlg.Finish(); };
        steps.push_back(s);
    }
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
            lib.base = LerpPose(poses::Sit(), poses::DrawerReach(), Smooth(t));
            scene.drawerOpen = Smooth(t);
            lib.lookAt = Vector3{ world::DrawerPos.x, world::DrawerPos.y, world::DrawerPos.z - 0.2f };
        };
        steps.push_back(s);
    }
    {
        Step s;
        s.dur = 0.8f;
        s.enter = [this]() { bk.visible = true; };
        s.tick = [this](float t, float) {
            Matrix inDrawer = MatrixTranslate(world::DrawerPos.x - book::W * 0.5f, world::DrawerPos.y - 0.06f, world::DrawerPos.z - 0.22f);
            Vector3 h = gfx::Origin(lib.Hand(1));
            Matrix hand = MatrixTranslate(h.x - book::W * 0.5f, h.y - 0.03f, h.z);
            bk.world = gfx::LerpRigid(inDrawer, hand, Smooth(t));
            lib.base = LerpPose(poses::DrawerReach(), poses::Sit(), Smooth(t) * 0.4f);
        };
        steps.push_back(s);
    }
    {
        Step s;
        s.dur = 1.2f;
        auto thud = std::make_shared<bool>(false);
        auto start = std::make_shared<Matrix>();
        s.enter = [this, thud, start]() { *thud = false; *start = bk.world; };
        s.tick = [this, thud, start](float t, float) {
            Pose reach = poses::Sit();
            reach.spine = 0.42f;
            for (int i = 0; i < 2; i++) { reach.shPitch[i] = 1.15f; reach.elbow[i] = 0.25f; reach.shRoll[i] = -0.1f; }
            Pose from = LerpPose(poses::DrawerReach(), poses::Sit(), 0.4f);
            lib.base = t < 0.5f ? LerpPose(from, reach, Smooth(t / 0.5f)) : LerpPose(reach, poses::Sit(), Smooth((t - 0.5f) / 0.5f));
            Matrix desk = MatrixTranslate(world::BookOnDesk.x, world::BookOnDesk.y, world::BookOnDesk.z);
            Matrix over = MatrixTranslate(0.1f, world::DeskTopY + 0.12f, 0.3f);
            bk.world = t < 0.5f ? gfx::LerpRigid(*start, over, Smooth(t / 0.5f)) : gfx::LerpRigid(over, desk, EaseOutCubic((t - 0.5f) / 0.5f));
            scene.drawerOpen = 1.f - Smooth(t * 1.5f);
            if (t > 0.62f && !*thud) { *thud = true; sfx::Play(sfx::Thud, 0.7f); }
            lib.lookAt = gfx::Origin(bk.world);
        };
        s.exit = [this]() { bk.world = MatrixTranslate(world::BookOnDesk.x, world::BookOnDesk.y, world::BookOnDesk.z); scene.drawerOpen = 0; lib.base = poses::Sit(); };
        steps.push_back(s);
    }
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
    view = MakeView(current);
    bk.pagesTextured = true;
    bk.pageTex = pageRT.texture;
    bk.open = 0;
    dlg.Hide();
    SetPhase(Phase::Presenting);
    sfx::Play(sfx::Open, 0.7f);
}

void App::BeginClosing() {
    if (!view) return;
    view->OnClose();
    pendingRemark = view->Remark();
    {   // did that session earn a new librarian?
        std::vector<std::string> fresh = unlocks::Check();
        for (const std::string& id : fresh)
            if (const LibrarianDef* d = unlocks::Find(id)) pendingRemark += " **New librarian unlocked: " + d->name + "!** Find them under the star button, top right.";
        if (!fresh.empty()) { newLibBadge = true; sfx::Play(sfx::Success, 0.8f); }
    }
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
            bk.open = 1.f - Smooth((t - 0.35f) / 0.6f);
            lib.lookAt = cam.position;
        };
        s.exit = [this]() { bk.open = 0; bookBlend = 0; sfx::Play(sfx::Thud, 0.4f, 1.3f); };
        steps.push_back(s);
    }
    {
        Step s;
        s.dur = 1.5f;
        auto start = std::make_shared<Matrix>();
        s.enter = [this, start]() {
            *start = bk.world;
            dlg.Say(pendingRemark);
        };
        s.tick = [this, start](float t, float) {
            Pose reach = poses::Sit();
            reach.spine = 0.4f;
            for (int i = 0; i < 2; i++) { reach.shPitch[i] = 1.1f; reach.elbow[i] = 0.3f; reach.shRoll[i] = -0.12f; }
            lib.base = t < 0.4f ? LerpPose(poses::Sit(), reach, Smooth(t / 0.4f)) : LerpPose(reach, poses::Sit(), Smooth((t - 0.55f) / 0.45f));
            int slot = std::min(scene.pileCount, 8);
            Matrix pile = scene.PileSlot(slot);
            Matrix near = MatrixTranslate(0.05f, world::DeskTopY + 0.002f, 0.28f);
            Matrix up = MatrixMultiply(pile, MatrixTranslate(-0.1f, 0.1f, 0));
            if (t < 0.4f) bk.world = gfx::LerpRigid(*start, near, Smooth(t / 0.4f));
            else if (t < 0.75f) bk.world = gfx::LerpRigid(near, up, Smooth((t - 0.4f) / 0.35f));
            else bk.world = gfx::LerpRigid(up, pile, Smooth((t - 0.75f) / 0.25f));
            lib.lookAt = gfx::Origin(bk.world);
        };
        s.exit = [this]() { FinishClosing(); };
        steps.push_back(s);
    }
}

void App::FinishClosing() {
    if (scene.pileCount < 9) {
        scene.pileColors[(size_t)scene.pileCount % scene.pileColors.size()] = bk.GetLook().cover;
        scene.pileCount++;
    }
    Prog().booksReturned++;
    Prog().Save();
    sfx::Play(sfx::Thud, 0.35f, 1.4f);
    bk.visible = false;
    if (shelfIndex >= 0) scene.Shelf(shelfIndex).present = true;
    view.reset();
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
void App::UpdateLibrarianIdle(float dt) {
    (void)dt;
    if (!lib.seated) return;
    if (phase == Phase::Intro) {
        lib.lookAt = phaseT < 1.2f ? Vector3{ 0.15f, 0.79f, 0.25f } : cam.position;
        return;
    }
    if (phase == Phase::Hub) {
        Vector3 base = cam.position;
        if (dlg.ChoicesShowing()) base = Vector3{ cam.position.x, 0.95f, 1.4f };
        if (hoverTab >= 0) {
            float side = hoverTab < 3 ? -1.f : 1.f;
            base = Vector3{ side * 1.5f, 1.15f - (hoverTab % 3 - 1) * 0.3f, 1.6f };
        }
        base.x += parallax.x * 0.4f;
        lib.lookAt = base;
    } else if (phase == Phase::Reading || phase == Phase::Presenting) {
        // watch the visitor read, glancing up now and then
        bool glance = fmodf(time, 9.f) < 1.6f;
        lib.lookAt = glance ? cam.position : Vector3{ 0, world::DeskTopY, 0.7f };
    }
}

void App::Update(float dt) {
    time += dt;
    phaseT += dt;
    ui::NewFrame();
    hudScale = GetScreenHeight() / 900.f;
    hudW = GetScreenWidth() / hudScale;
    hudH = 900.f;

    if (IsKeyPressed(KEY_F11)) { ToggleBorderlessWindowed(); Prog().settings.fullscreen = !Prog().settings.fullscreen; }
    if (IsKeyPressed(KEY_F12)) SaveShot(TextFormat("screenshot_%lld", NowEpoch()));
    bool hurry = (IsKeyDown(KEY_SPACE) && !ui::KeyboardCaptured()) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    float baseSpeed = Prog().settings.quickFetch ? 2.0f : 1.0f;
    timeScale = baseSpeed * (hurry ? 3.5f : 1.f);

    RunAutopilot(dt);
    scene.Update(dt, time);
    dlg.Update(dt);
    lib.talk = dlg.Mouth();

    // Escape key
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (AnyOverlay()) { settingsOpen = helpOpen = libsOpen = false; }
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
                greet += unlockNote;
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
        bk.open = Smooth((phaseT - 0.1f) / 0.85f);
        bookBlend = Clamp01((phaseT - 0.3f) / 1.15f);
        bk.pageEmissive = Lerp(0.45f, 0.92f, Smooth(bookBlend));
        if (view) view->Tick(dt);
        if (phaseT > 1.5f) { bookBlend = 1; bk.open = 1; SetPhase(Phase::Reading); }
        break;
    }
    case Phase::Reading:
        bookBlend = 1;
        if (view) {
            view->Tick(dt);
            if (view->wantClose) BeginClosing();
        }
        break;
    case Phase::Closing:
        bk.pageEmissive = Lerp(0.45f, 0.92f, Smooth(bookBlend));
        RunSteps(dt * timeScale);
        break;
    }

    UpdateLibrarianIdle(dt);
    lib.Update(dt, time);
    if (lib.carrying) bk.world = lib.CarryMatrix();
    UpdateCamera(dt);
}

void App::UpdateCamera(float dt) {
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    Vector2 m = GetMousePosition();
    Vector2 want{ Clamp((m.x / sw - 0.5f) * 2.f, -1.f, 1.f), Clamp((m.y / sh - 0.5f) * 2.f, -1.f, 1.f) };
    bool allow = (phase == Phase::Hub || phase == Phase::Intro) && Prog().settings.mouseLook && !AnyOverlay();
    if (!allow) want = Vector2{ 0, 0 };
    parallax.x = Damp(parallax.x, want.x, 2.5f, dt);
    parallax.y = Damp(parallax.y, want.y, 2.5f, dt);

    float away = Clamp01((Vector3Distance(Vector3{ lib.pos.x, 0, lib.pos.z }, world::SeatPos) - 0.35f) / 0.8f);
    float wantWatch = (phase == Phase::Fetching) ? away : 0.f;
    watchWeight = Damp(watchWeight, wantWatch, 2.2f, dt);
    zoomWeight = Damp(zoomWeight, (phase == Phase::Fetching && lib.pos.z < -2.0f) ? 1.f : 0.f, 1.4f, dt);
    Vector3 head = lib.HeadPos();
    watchTarget = Damp3(watchTarget, Vector3{ head.x, head.y - 0.25f, head.z }, 3.5f, dt);

    Vector3 deskPos{ parallax.x * 0.07f, 1.33f - parallax.y * 0.035f, 1.95f };
    Vector3 deskTgt{ parallax.x * 0.6f, 1.17f - parallax.y * 0.28f, -0.4f };
    Vector3 tgt = Vector3Lerp(deskTgt, watchTarget, watchWeight * 0.85f);
    float fov = Lerp(42.f, 31.f, zoomWeight);

    Vector3 bc{ world::BookOnDesk.x, world::BookOnDesk.y + book::PageY, world::BookOnDesk.z };
    float aspect = sw / std::max(1.f, sh);
    float tanH = tanf(42.f * DEG2RAD * 0.5f);
    float h = std::max(book::D * 0.5f * 1.06f / tanH, book::W * 1.03f / (tanH * aspect));
    Vector3 bookPos{ bc.x, bc.y + h, bc.z + 0.0005f };
    float tp = EaseInOutCubic(bookBlend), tt = EaseInOutCubic(Clamp01(bookBlend * 1.35f));
    cam.position = Vector3Lerp(deskPos, bookPos, tp);
    cam.target = Vector3Lerp(tgt, bc, tt);
    cam.up = Vector3Normalize(Vector3Lerp(Vector3{ 0, 1, 0 }, Vector3{ 0, 0, -1 }, tp));
    cam.fovy = Lerp(fov, 42.f, tp);
    if (closeup > 0) {   // autopilot only: orbit the librarian for inspection (1 head, 2 bust, 3 whole body, 4 head with hat or hair on top)
        Vector3 hp = lib.HeadPos();
        float dist = closeup == 1 ? 0.62f : closeup == 2 ? 1.3f : closeup == 4 ? 1.1f : 3.9f;
        float drop = closeup == 1 ? 0.01f : closeup == 2 ? 0.16f : closeup == 4 ? -0.1f : 0.74f;
        float a = closeYaw * DEG2RAD, e = closePitch * DEG2RAD;
        cam.target = Vector3{ hp.x, hp.y - drop, hp.z };
        cam.position = Vector3{ cam.target.x + sinf(a) * cosf(e) * dist, cam.target.y + sinf(e) * dist, cam.target.z + cosf(a) * cosf(e) * dist };
        cam.up = Vector3{ 0, 1, 0 };
        cam.fovy = 30.f;
    }
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------
void App::RenderPagesToTexture() {
    if (!view) return;
    BeginTextureMode(pageRT);
    ClearBackground(pal::Paper);
    ui::Begin(Vector2{ 0, 0 }, 1.f, false);
    view->Draw(false);
    ui::End();
    EndTextureMode();
    GenTextureMipmaps(&pageRT.texture);
    SetTextureFilter(pageRT.texture, TEXTURE_FILTER_TRILINEAR);
    bk.pageTex = pageRT.texture;
}

// The Librarians screen shows the head and shoulders of everyone unlocked. Each is the real figure, seated where the
// librarian sits in the hub and lit by the same lamps, drawn into a texture. At most one a frame: a librarian who has
// not been on duty yet has to be sculpted first, which takes a fraction of a second.
void App::RenderPortrait() {
    for (const LibrarianDef& d : unlocks::Roster()) {
        if (!unlocks::IsUnlocked(d.id)) continue;
        Portrait& p = portraits[d.id];
        if (p.drawn) continue;
        if (!p.rt.id) p.rt = LoadRenderTexture(512, 512);   // shown at a quarter of this size or less, so its mipmaps smooth every edge
        Librarian sitter;
        sitter.look = d.id;
        sitter.Init();
        Vector3 hp = sitter.HeadPos();
        Camera3D pc{};
        pc.position = Vector3{ hp.x + 0.34f, hp.y + 0.03f, hp.z + 1.5f };   // a little to one side: shoulders at an angle, face to the camera
        pc.up = Vector3{ 0, 1, 0 };
        pc.fovy = 17.5f;
        pc.projection = CAMERA_PERSPECTIVE;
        sitter.lookAt = pc.position;
        sitter.Update(1.f, 0.f);   // one long step, so the head has finished turning to the camera
        pc.target = sitter.HeadPos();

        BeginTextureMode(p.rt);
        ClearBackground(d.accent);
        DrawRectangleGradientV(0, 0, p.rt.texture.width, p.rt.texture.height, Mix(d.accent, pal::Paper, 0.3f), util::Shade(d.accent, 0.72f));
        BeginMode3D(pc);
        auto lights = scene.Lights(0.f);
        lights[3] = gfx::Light{ Vector3{ pc.position.x - 0.3f, pc.position.y - 0.45f, pc.position.z }, Vector3{ 0.5f, 0.47f, 0.43f }, 6.f };   // the far window becomes a soft fill from below, so no chin sits in shadow
        gfx::BeginFrame(pc, lights.data(), (int)lights.size(), scene.Ambient(), Vector3{ 0, 0, 0 }, 0.f);   // no fog
        sitter.Draw();
        EndMode3D();
        EndTextureMode();
        GenTextureMipmaps(&p.rt.texture);
        SetTextureFilter(p.rt.texture, TEXTURE_FILTER_TRILINEAR);
        p.drawn = true;
        return;
    }
}

// a square texture shown as a disc; render textures are stored bottom row first, hence the flipped v
static void DrawDisc(Texture2D tex, Vector2 c, float r) {
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    rlNormal3f(0, 0, 1);
    const int n = 24;   // each quad is two slices of the pie
    for (int i = 0; i < n; i++) {
        rlTexCoord2f(0.5f, 0.5f);
        rlVertex2f(c.x, c.y);
        for (int k = 0; k <= 2; k++) {
            float a = PI * (float)(2 * i + k) / (float)n;
            rlTexCoord2f(0.5f + 0.5f * cosf(a), 0.5f - 0.5f * sinf(a));
            rlVertex2f(c.x + r * cosf(a), c.y + r * sinf(a));
        }
    }
    rlEnd();
    rlSetTexture(0);
}

void App::Render() {
    bool needRT = view && (phase == Phase::Presenting || phase == Phase::Closing);
    if (needRT) RenderPagesToTexture();
    if (libsOpen) RenderPortrait();

    BeginDrawing();
    ClearBackground(sceneOff ? Color{ 126, 130, 138, 255 } : Color{ 60, 44, 32, 255 });
    BeginMode3D(cam);
    {
        auto lights = scene.Lights(time);
        gfx::BeginFrame(cam, lights.data(), (int)lights.size(), scene.Ambient(), Vector3{ 0.34f, 0.26f, 0.19f }, 0.0055f);
        if (!sceneOff) scene.Draw(cam, time);
        lib.Draw();
        bk.Draw();
        gfx::BeginTransparent();
        lib.DrawShadow();
        if (bk.visible) {
            Vector3 c = Vector3Transform(Vector3{ book::W * (bk.open > 0.5f ? 0.f : 0.5f), 0, 0 }, bk.world);
            float h = c.y - world::DeskTopY;
            if (h > -0.01f && h < 0.4f && c.z > -0.1f && c.z < 1.1f && fabsf(c.x) < 1.25f) {
                float s = 1.f + h * 3.f;
                gfx::BlobShadow(Vector3{ c.x, world::DeskTopY + 0.0015f, c.z }, (bk.open > 0.5f ? 0.85f : 0.48f) * s, 0.58f * s, 0.5f / s);
            }
        }
        gfx::EndTransparent();
        if (!sceneOff) scene.DrawTransparent(cam, time);
    }
    EndMode3D();

    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    if (hudOff) { EndDrawing(); return; }   // autopilot: a clean view of the 3D scene only
    DrawTexturePro(vignette, Rectangle{ 0, 0, (float)vignette.width, (float)vignette.height }, Rectangle{ 0, 0, sw, sh }, Vector2{ 0, 0 }, 0,
                   WithAlpha(WHITE, 0.30f - 0.15f * bookBlend));

    if (phase == Phase::Reading && view) {
        Vector2 a = GetWorldToScreen(bk.SpreadTopLeft(), cam);
        Vector2 b = GetWorldToScreen(bk.SpreadBottomRight(), cam);
        pageRect = Rectangle{ std::min(a.x, b.x), std::min(a.y, b.y), fabsf(b.x - a.x), fabsf(b.y - a.y) };
        // soft shadow on the desk around the open book
        ui::Begin(Vector2{ pageRect.x, pageRect.y }, pageRect.width / page::W, !AnyOverlay());
        view->Draw(!AnyOverlay());
        ui::End();
    }

    DrawHud();
    DrawOverlays();
    if (fade > 0.001f) DrawRectangle(0, 0, (int)sw, (int)sh, WithAlpha(BLACK, fade));
    ui::EndFrameCursor();
    EndDrawing();
}

void App::DrawTabs(bool interactive) {
    hoverTab = -1;
    float tw = 292, th = 112, gap = 22;
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
    bool modal = AnyOverlay();
    bool hubLike = phase == Phase::Hub;
    hudVisible = Damp(hudVisible, hubLike ? 1.f : 0.f, 6.f, GetFrameTime());
    bool interactive = !modal;
    ui::Begin(Vector2{ 0, 0 }, hudScale, interactive);
    if (hubLike || hudVisible > 0.01f) DrawTabs(interactive && hubLike);
    // title plate
    float ta = Clamp01(1.f - bookBlend * 3.f);
    if (ta > 0) {
        ui::Text(FontId::SerifBold, "Malloc's Library", 30 + 2, 22 + 3, 38, WithAlpha(BLACK, 0.6f * ta));
        ui::Text(FontId::SerifBold, "Malloc's Library", 30, 22, 38, WithAlpha(pal::Gold, ta));
        ui::Text(FontId::SerifItalic, "an interactive C & C++ study hall", 32, 66, 17, WithAlpha(pal::GoldSoft, 0.85f * ta));
    }
    // top-right buttons
    if (phase == Phase::Hub) {
        Rectangle people{ hudW - 188, 22, 48, 48 }, help{ hudW - 132, 22, 48, 48 }, gear{ hudW - 76, 22, 48, 48 };
        if (ui::IconButton(people, Icon::Star, ui::BtnWood, interactive)) { libsOpen = !libsOpen; settingsOpen = helpOpen = false; newLibBadge = false; sfx::Play(sfx::Click); }
        if (newLibBadge) {   // a new librarian is waiting
            float pulse = 0.75f + 0.25f * sinf(time * 5.f);
            DrawCircleV(Vector2{ people.x + 42, people.y + 6 }, 9.f, WithAlpha(BLACK, 0.5f));
            DrawCircleV(Vector2{ people.x + 42, people.y + 6 }, 7.f * pulse, pal::Red);
        }
        if (ui::IconButton(help, Icon::Help, ui::BtnWood, interactive)) { helpOpen = !helpOpen; settingsOpen = libsOpen = false; sfx::Play(sfx::Click); }
        if (ui::IconButton(gear, Icon::Gear, ui::BtnWood, interactive)) { settingsOpen = !settingsOpen; helpOpen = libsOpen = false; confirmReset = false; sfx::Play(sfx::Click); }
    }
    // dialogue
    if (phase != Phase::Reading && phase != Phase::Presenting) dlg.Draw(hudW, hudH, interactive && (phase == Phase::Hub || phase == Phase::Fetching || phase == Phase::Closing));
    // hints
    if (phase == Phase::Fetching || phase == Phase::Closing) {
        std::string h = "Hold SPACE (or the right mouse button) to hurry " + DutyName() + " along";
        float w = ui::MeasureWidth(FontId::SerifItalic, h, 17);
        float a = 0.55f + 0.25f * sinf(time * 2.f);
        ui::Text(FontId::SerifItalic, h, hudW - w - 34, 34, 17, WithAlpha(pal::GoldSoft, a * ta));
    }
    if (phase == Phase::Reading && pageRect.width > 0) {
        float bottom = (pageRect.y + pageRect.height) / hudScale;
        if (hudH - bottom > 24) ui::Text(FontId::SerifItalic, "Esc returns the book to " + DutyName() + "   -   F11 full screen", 30, hudH - 30, 15, WithAlpha(pal::GoldSoft, 0.6f));
    }
    if (phase == Phase::Hub && interactive) HandleWorldClicks();
    ui::End();
}

void App::HandleWorldClicks() {
    if (!ui::AnyPress() || dlg.ChoicesShowing()) return;
    Ray r = GetScreenToWorldRay(GetMousePosition(), cam);
    Vector3 cat{ world::CatPos.x, world::CatPos.y + 0.06f, world::CatPos.z };
    Vector3 bell{ world::BellPos.x, world::BellPos.y + 0.04f, world::BellPos.z };
    if (GetRayCollisionSphere(r, cat, 0.14f).hit) {
        ui::ConsumePress();
        scene.catPet = 2.5f;
        sfx::Play(sfx::Hover, 0.8f, 0.6f);
        dlg.Say(PersonaFor(Prog().librarian).cat);
        return;
    }
    if (GetRayCollisionSphere(r, bell, 0.08f).hit) {
        ui::ConsumePress();
        scene.bellRing = 1.2f;
        sfx::Play(sfx::Bell, 0.7f);
        dlg.Say(PersonaFor(Prog().librarian).bell);
        return;
    }
    if (lib.HitTest(r)) {
        ui::ConsumePress();
        const Persona& pr = PersonaFor(Prog().librarian);
        dlg.Say(pr.quips[(size_t)funIndex % pr.quips.size()]);
        funIndex++;
        lib.excitement = 0.6f;
    }
}

void App::DrawOverlays() {
    overlayAnim = Damp(overlayAnim, AnyOverlay() ? 1.f : 0.f, 12.f, GetFrameTime());
    if (overlayAnim < 0.01f) return;
    ui::Begin(Vector2{ 0, 0 }, hudScale, true);
    DrawRectangle(0, 0, (int)hudW + 2, (int)hudH, WithAlpha(BLACK, 0.5f * overlayAnim));
    if (settingsOpen) DrawSettings();
    if (helpOpen) DrawHelp();
    if (libsOpen) DrawLibrarians();
    ui::End();
    lib.excitement = Damp(lib.excitement, 0.f, 2.f, GetFrameTime());
}

void App::DrawSettings() {
    Rectangle r{ hudW * 0.5f - 300, 120 + (1 - overlayAnim) * 30, 600, 640 };
    DrawRectangleRounded(Rectangle{ r.x + 6, r.y + 8, r.width, r.height }, 0.06f, 8, WithAlpha(BLACK, 0.4f));
    ui::Panel(r, pal::Paper, pal::Wood, 0.06f, 3.f);
    ui::Text(FontId::SerifBold, "Settings", r.x + 34, r.y + 26, 34, pal::Ink);
    Settings& s = Prog().settings;
    float y = r.y + 96;
    ui::Text(FontId::Serif, "Volume", r.x + 34, y, 22, pal::Ink);
    if (ui::Slider(Rectangle{ r.x + 200, y + 2, 340, 26 }, &s.volume)) ApplySettings();
    y += 56;
    if (ui::Checkbox(Rectangle{ r.x + 34, y, 520, 34 }, "The librarian's mumbling voice", &s.voice)) ApplySettings();
    y += 48;
    ui::Checkbox(Rectangle{ r.x + 34, y, 520, 34 }, "Quick fetch (the librarian walks twice as fast)", &s.quickFetch);
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
            ApplyLibrarian();   // the reset puts Mr. Malloc back on duty: swap the figure, nameplate and voice too
            newLibBadge = false;
            scene.pileCount = 0;
            confirmReset = false;
            dlg.Say("Done - a fresh ledger, a clean slate. I've even dusted the desk.");
            settingsOpen = false;
        } else confirmReset = true;
    }
    y += 72;
    ui::Text(FontId::SerifItalic, "Progress is saved in the save/ folder next to the program.", r.x + 34, y, 16, pal::InkSoft);
    y += 40;
    DrawRectangleRec(Rectangle{ r.x + 34, y, r.width - 68, 2 }, pal::PaperEdge);
    y += 14;
    ui::Text(FontId::SerifBold, "Developer options", r.x + 34, y, 22, pal::Ink);
    y += 36;
    if (ui::Checkbox(Rectangle{ r.x + 34, y, 520, 34 }, "Unlock every librarian (for demos)", &s.demoUnlock) && !unlocks::IsUnlocked(Prog().librarian))
        SelectLibrarian("malloc");   // switched off while a borrowed librarian was at the desk: Mr. Malloc takes over again
    y += 40;
    ui::Text(FontId::SerifItalic, "What you have really earned is kept. Switch it off to go back to that.", r.x + 34, y, 16, pal::InkSoft);
    if (ui::Button(Rectangle{ r.x + r.width - 160, r.y + r.height - 70, 126, 48 }, "Done", ui::BtnPrimary, true, 20)) { settingsOpen = false; Prog().Save(); }
}

void App::DrawLibrarians() {
    Rectangle r{ hudW * 0.5f - 450, 36 + (1 - overlayAnim) * 30, 900, 830 };
    DrawRectangleRounded(Rectangle{ r.x + 6, r.y + 8, r.width, r.height }, 0.05f, 8, WithAlpha(BLACK, 0.4f));
    ui::Panel(r, pal::Paper, pal::Wood, 0.05f, 3.f);
    ui::Text(FontId::SerifBold, "The Librarians", r.x + 36, r.y + 20, 34, pal::Ink);
    ui::Rich("Pick who sits at the desk. Each librarian has their own look, voice and way of speaking. Librarians you unlock are yours for good.",
             r.x + 36, r.y + 70, r.width - 72, 16, pal::InkSoft, 1.3f);

    const std::vector<LibrarianDef>& roster = unlocks::Roster();
    float cw = (r.width - 72 - 20) * 0.5f, ch = 150;
    for (size_t i = 0; i < roster.size(); i++) {
        const LibrarianDef& d = roster[i];
        float cx = r.x + 36 + (float)(i % 2) * (cw + 20), cy = r.y + 124 + (float)(i / 2) * (ch + 10);
        bool open = unlocks::IsUnlocked(d.id), onDuty = Prog().librarian == d.id;
        Rectangle card{ cx, cy, cw, ch };
        ui::Panel(card, onDuty ? Mix(pal::Paper, pal::GoldSoft, 0.55f) : (open ? pal::Paper : pal::PaperDark), onDuty ? pal::Gold : pal::PaperEdge, 0.08f, onDuty ? 3.f : 2.f);
        Vector2 c{ cx + 54, cy + 54 };
        auto pt = portraits.find(d.id);
        bool face = open && pt != portraits.end() && pt->second.drawn;   // the portrait arrives a frame or so after the screen opens
        if (face) DrawDisc(pt->second.rt.texture, c, 39.5f);                  // tucked under the ring
        else DrawCircleV(c, 38, open ? d.accent : Color{ 150, 140, 128, 255 });
        DrawRing(c, 38, 42, 0, 360, 40, open ? pal::Gold : Color{ 120, 110, 98, 255 });
        if (!open) ui::DrawIcon(Icon::Lock, c, 36, Color{ 236, 230, 218, 255 });
        else if (!face) ui::TextCentered(FontId::SerifBold, d.initial, Rectangle{ c.x - 38, c.y - 30, 76, 60 }, 42, pal::GoldSoft);
        ui::Text(FontId::SerifBold, open ? d.name : std::string("???"), cx + 106, cy + 12, 24, open ? pal::Ink : pal::InkSoft);
        ui::Wrap(FontId::SerifItalic, open ? d.title : std::string("A mystery librarian, waiting to be earned."), cx + 106, cy + 44, cw - 122, 15, pal::InkSoft);
        if (open) ui::Text(FontId::SerifBold, onDuty ? "On duty" : "Unlocked", cx + 18, cy + ch - 34, 17, onDuty ? pal::Amber : pal::Green);
        else ui::Wrap(FontId::Serif, "To unlock: " + d.how, cx + 18, cy + ch - 48, cw - 36, 16, pal::Red);
        if (open && !onDuty && ui::Button(Rectangle{ cx + cw - 142, cy + ch - 48, 124, 36 }, "Call in", ui::BtnPrimary, true, 17)) SelectLibrarian(d.id);
    }
    if (ui::Button(Rectangle{ r.x + r.width - 160, r.y + r.height - 62, 126, 46 }, "Done", ui::BtnPrimary, true, 20)) libsOpen = false;
}

void App::DrawHelp() {
    Rectangle r{ hudW * 0.5f - 420, 110 + (1 - overlayAnim) * 30, 840, 660 };
    DrawRectangleRounded(Rectangle{ r.x + 6, r.y + 8, r.width, r.height }, 0.05f, 8, WithAlpha(BLACK, 0.4f));
    ui::Panel(r, pal::Paper, pal::Wood, 0.05f, 3.f);
    ui::Text(FontId::SerifBold, "How the library works", r.x + 36, r.y + 26, 34, pal::Ink);
    float y = r.y + 84;
    const char* lines[] = {
        "Pick a service from the cards on either side. **The librarian** will ask what you need, fetch the right book from the shelves and open it on the desk for you.",
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
        ui::Text(FontId::Serif, keys[i], r.x + 36 + (i % 2) * 400, y + (i / 2) * 30, 16, pal::InkSoft);
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
    TraceLog(LOG_INFO, "SHOT: %s  (%d fps)", p.c_str(), GetFPS());
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
        if (c.cmd == "click") { ui::FakePress(); autoIdx++; autoWait = 0.05f; return; }   // a left click where `mouse` put the pointer
        if (c.cmd == "say") { dlg.Say(c.arg); dlg.Finish(); autoIdx++; continue; }
        if (c.cmd == "quick") { Prog().settings.quickFetch = c.arg == "1"; autoIdx++; continue; }
        if (c.cmd == "help") { helpOpen = c.arg == "1"; autoIdx++; continue; }
        if (c.cmd == "settings") { settingsOpen = c.arg == "1"; autoIdx++; continue; }
        if (c.cmd == "closeup") {   // closeup <0 off | 1 head | 2 bust | 3 body | 4 head and what is on top of it> [yaw degrees] [pitch degrees]
            auto p = Split(c.arg, ' ');
            closeup = p.empty() ? 0 : atoi(p[0].c_str());
            closeYaw = p.size() > 1 ? (float)atof(p[1].c_str()) : 0.f;
            closePitch = p.size() > 2 ? (float)atof(p[2].c_str()) : 0.f;
            autoIdx++; continue;
        }
        if (c.cmd == "hud") { hudOff = c.arg == "0"; autoIdx++; continue; }
        if (c.cmd == "scene") { sceneOff = c.arg == "0"; autoIdx++; continue; }
        if (c.cmd == "size") {
            auto p = Split(c.arg, ' ');
            if (p.size() >= 2) { SetWindowMinSize(320, 240); SetWindowSize(atoi(p[0].c_str()), atoi(p[1].c_str())); }
            autoIdx++; continue;
        }
        if (c.cmd == "stage") {   // stand the librarian in the open, head straight, so every side can be inspected
            lib.seated = false; lib.pos = Vector3{ 2.0f, 0.f, -1.2f }; lib.yaw = 0.f; lib.lookWeight = 0.f; lib.gestureAmount = 0.f;
            lib.base = poses::Stand();
            autoIdx++; continue;
        }
        if (c.cmd == "pose") {
            Pose p = poses::Stand();
            if (c.arg == "carry") p = poses::Carry();
            else if (c.arg == "sit") p = poses::Sit();
            lib.base = p;
            autoIdx++; continue;
        }
        if (c.cmd == "mouth") { lib.debugMouth = (float)atof(c.arg.c_str()); autoIdx++; continue; }
        if (c.cmd == "blink") { lib.debugBlink = (float)atof(c.arg.c_str()); autoIdx++; continue; }
        if (c.cmd == "libs") { libsOpen = c.arg == "1"; settingsOpen = helpOpen = false; autoIdx++; continue; }
        if (c.cmd == "unlockall") { for (auto& d : unlocks::Roster()) Prog().unlocked.insert(d.id); autoIdx++; continue; }
        if (c.cmd == "look") { Prog().librarian = c.arg; ApplyLibrarian(); autoIdx++; continue; }
        if (c.cmd == "quit") { autoQuit = true; autoIdx++; return; }
        autoIdx++;
    }
}
