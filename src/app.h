// app.h - the director: phases, choreography, camera moves, HUD and book sessions (2D edition)
#pragma once
#include "common.h"
#include "stage.h"
#include "puppet.h"
#include "book2d.h"
#include "dialogue.h"
#include "views/view.h"
#include <memory>

class App {
public:
    int Run(int argc, char** argv);

private:
    enum class Phase { Intro, Hub, Fetching, Presenting, Reading, Closing };

    struct Request {
        std::string kind;     // read, mcq, written, coding, mock, stats, areas, drill
        std::string key;      // topic id, mix id ("exam1", "exam2", "cpp", "all") or set id
        int count = 10;
        std::string problem;  // optional problem to open in a workbook
    };

    struct Step {
        float dur = 1.f;
        std::function<void()> enter;
        std::function<void(float t, float dt)> tick;
        std::function<void()> exit;
    };

    // world
    Stage stage;
    Puppet lib;
    book::Book2D bk;
    Dialogue dlg;
    RenderTexture2D pageRT{};
    std::unique_ptr<BookView> view;
    Texture2D vignette{};

    // the book as a prop: in the librarian's hands, free on the desk, or lying open as a spread
    bool bookAsSpread = false;
    Vector2 bookPos{ 800.f, 792.f };
    float bookW = 210.f, bookSquash = 0.62f, bookRot = 0.f, bookOpen = 0.f;
    // a book being pulled off the shelf
    bool pullShow = false;
    Rectangle pullRect{};
    Color pullColor{};
    std::string pullMark;
    float pullAlpha = 1.f;

    // state
    Phase phase = Phase::Intro;
    float time = 0.f, phaseT = 0.f, fade = 1.f;
    Request current, queued;
    bool hasQueued = false;
    int shelfIndex = -1;           // shelf book being fetched (-1 = the ledger)
    std::vector<Step> steps;
    int stepIdx = 0;
    float stepT = 0.f;
    bool stepStarted = false;
    float timeScale = 1.f;
    std::string pendingRemark;

    // camera: bookBlend 0 = looking across the desk, 1 = looking straight down at the open book
    float bookBlend = 0.f;
    Vector2 parallax{ 0, 0 };
    Rectangle pageRect{};          // where the open spread is on screen

    // HUD
    float hudW = 1600, hudH = 900, hudScale = 1;
    float tabSlide[6] = { 0 };
    float hudVisible = 0.f;
    int hoverTab = -1;
    bool settingsOpen = false, helpOpen = false, confirmReset = false;
    float overlayAnim = 0.f;
    int funIndex = 0;

    // autopilot (for scripted screenshots / demos)
    struct AutoCmd { std::string cmd, arg; };
    std::vector<AutoCmd> autoCmds;
    size_t autoIdx = 0;
    float autoWait = 0.f;
    bool autoQuit = false;

    struct Cam { float z = 1.f; Vector2 anchor{ 0, 0 }, off{ 0, 0 }; float spreadScale = 1.f; };

    // setup
    void Init();
    void Shutdown();
    void ApplySettings();

    // frame
    void Update(float dt);
    void Render();
    Cam ComputeCam(float e) const;
    void UpdatePan(float dt);
    void UpdateLibrarianIdle(float dt);
    void RenderPagesToTexture();
    void DrawScene(const Cam& cam);
    void DrawDeskBackdrop(float alpha);
    void DrawHud();
    void DrawTabs(bool interactive);
    void DrawOverlays();
    void DrawSettings();
    void DrawHelp();
    void HandleWorldClicks();
    Vector2 MouseScene() const;

    // flow
    void SetPhase(Phase p);
    void OnTab(int i);
    void StartRequest(const Request& r);
    void BuildShelfFetch();
    void BuildDrawerFetch();
    void BeginPresenting();
    void BeginClosing();
    void FinishClosing();
    void RunSteps(float dt);
    Step StandUp();
    Step SitDown();
    Step WalkTo(Vector2 target, float speed = 280.f);
    Step Turn(bool toBack);
    Step PlaceBook(bool seated);
    std::unique_ptr<BookView> MakeView(const Request& r);
    book::Look LookFor(const Request& r) const;
    std::string ShelfKeyFor(const Request& r) const;
    std::string FetchLine(const Request& r) const;
    std::vector<Choice> TopicChoices(bool withMixes) const;
    void AskCount(const std::string& kind, const std::string& key);

    // autopilot
    void LoadScript(const std::string& path);
    void RunAutopilot(float dt);
    void SaveShot(const std::string& name);
};
