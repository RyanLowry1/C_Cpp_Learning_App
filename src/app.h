// app.h - the director: phases, choreography, camera, HUD and book sessions
#pragma once
#include "common.h"
#include "scene.h"
#include "librarian.h"
#include "book3d.h"
#include "dialogue.h"
#include "views/view.h"
#include <map>
#include <memory>

class App {
public:
    int Run(int argc, char** argv);

private:
    enum class Phase { Intro, Hub, Fetching, Presenting, Reading, Closing, Gym };

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
    Scene scene;
    Librarian lib;
    book::Book3D bk;
    Dialogue dlg;
    Camera3D cam{};
    RenderTexture2D pageRT{};
    std::unique_ptr<BookView> view;
    Texture2D vignette{};

    // state
    Phase phase = Phase::Intro;
    float time = 0.f, phaseT = 0.f, fade = 1.f;
    Request current, queued;
    bool hasQueued = false;
    int shelfIndex = -1;           // shelf book being fetched (-1 = ledger from the drawer)
    std::vector<Step> steps;
    int stepIdx = 0;
    float stepT = 0.f;
    bool stepStarted = false;
    float timeScale = 1.f;
    std::string pendingRemark;

    // camera
    float bookBlend = 0.f, watchWeight = 0.f, zoomWeight = 0.f;
    Vector2 parallax{ 0, 0 };
    Vector3 watchTarget{ 0, 1.2f, -0.4f };
    Rectangle pageRect{};          // projected rectangle of the open spread on screen

    // HUD
    float hudW = 1600, hudH = 900, hudScale = 1;
    float tabSlide[6] = { 0 };
    float hudVisible = 0.f;
    int hoverTab = -1;
    bool settingsOpen = false, helpOpen = false, confirmReset = false, libsOpen = false;
    bool AnyOverlay() const { return settingsOpen || helpOpen || libsOpen; }
    float overlayAnim = 0.f;
    int funIndex = 0;

    // gym + librarians
    float idleT = 0.f;             // seconds the visitor has left the hub alone
    Vector2 lastMouse{ 0, 0 };
    int pendingTab = -1;           // a card clicked while he was working out
    bool gymAbort = false;
    float fitnessTimer = 0.f;
    float fitOverride = -1.f;      // autopilot: force a fitness level for screenshots
    int closeup = 0;               // autopilot: inspection camera orbiting the librarian (0 = off)
    float closeYaw = 0.f, closePitch = 0.f;
    bool hudOff = false;           // autopilot: draw the 3D scene only
    bool sceneOff = false;         // autopilot: hide the room, so the librarian can be seen from every side
    std::string unlockNote;        // unlocks earned while away, announced in the greeting
    bool newLibBadge = false;
    struct Portrait { RenderTexture2D rt{}; int level = -1; };   // level: the 5% fitness step it was drawn at (-1 = not drawn yet)
    std::map<std::string, Portrait> portraits;                   // faces for the Librarians screen, by librarian id

    // autopilot (for scripted screenshots / demos)
    struct AutoCmd { std::string cmd, arg; };
    std::vector<AutoCmd> autoCmds;
    size_t autoIdx = 0;
    float autoWait = 0.f;
    bool autoQuit = false;

    // setup
    void Init();
    void Shutdown();
    void ApplySettings();

    // frame
    void Update(float dt);
    void Render();
    void UpdateCamera(float dt);
    void UpdateLibrarianIdle(float dt);
    void RenderPagesToTexture();
    void RenderPortrait();
    void DrawHud();
    void DrawTabs(bool interactive);
    void DrawOverlays();
    void DrawSettings();
    void DrawHelp();
    void DrawLibrarians();
    void HandleWorldClicks();
    void ApplyLibrarian();
    void SelectLibrarian(const std::string& id);
    float TargetFitness() const;
    void StartGym();
    void BuildGym();
    void AbortGym();

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
    Step WalkTo(Vector3 target, float speed = 1.15f);
    Step TurnTo(float yaw, float dur = 0.6f);
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
