// view.h - base class for everything that can be shown on the pages of an open book
#pragma once
#include "../common.h"
#include "../ui.h"
#include "../content.h"
#include "../progress.h"
#include "../syntax.h"
#include <memory>

// Page space: an open spread is 1560 x 1000 virtual units (two 780-wide pages).
namespace page {
constexpr float W = 1560.f, H = 1000.f, PW = 780.f;
constexpr float Outer = 72.f, Inner = 58.f, Top = 70.f, Bottom = 80.f;
inline Rectangle Left() { return Rectangle{ Outer, Top, PW - Outer - Inner, H - Top - Bottom }; }
inline Rectangle Right() { return Rectangle{ PW + Inner, Top, PW - Outer - Inner, H - Top - Bottom }; }

void DrawPaper();
void RunningHeader(const std::string& left, const std::string& right);
void PageNumbers(int leftNo, int rightNo);
bool ReturnRibbon(bool interactive);                 // returns true when clicked
float Heading(const std::string& s, float x, float y, float w, float size = 36.f, Color c = ui::pal::Ink);
float Callout(const std::string& label, const std::string& text, float x, float y, float w, Color accent, Color fill,
              Icon icon, bool draw = true, float size = 20.f);
void Chip(const std::string& text, float x, float y, Color c, float size = 15.f, float* outW = nullptr);
Color DiffColor(const std::string& diff);
} // namespace page

class BookView {
public:
    virtual ~BookView() = default;
    virtual void Tick(float dt) { time += dt; }
    // Draw the whole spread in page space. When `interactive` is false the view is
    // being rendered onto the 3D book (during transitions): no input must be used.
    virtual void Draw(bool interactive) = 0;
    virtual bool HandleEscape() { return false; }     // true if the view consumed Esc
    virtual void OnClose() {}                         // save progress etc.
    virtual std::string Remark() { return std::string(); }
    virtual std::string SessionKey() const { return std::string(); }
    virtual void DebugCommand(const std::string&) {}  // used by the scripted autopilot

    bool wantClose = false;
    std::string requestAction, requestArg;           // ask the app to fetch something else
    float time = 0.f;
};

std::unique_ptr<BookView> MakeReadingView(const std::string& topic);
std::unique_ptr<BookView> MakeMcqView(const std::vector<int>& questions, const std::string& title, const std::string& sessionTopic, const std::string& mode);
std::unique_ptr<BookView> MakeWrittenView(const std::vector<int>& questions, const std::string& title, const std::string& sessionTopic);
std::unique_ptr<BookView> MakeCodingView(int setIndex, bool mock, const std::string& openProblem = "");
std::unique_ptr<BookView> MakeStatsView(bool problemAreas);

// question selection helpers shared by the app and the stats view
std::vector<int> PickMcqs(const std::vector<std::string>& topics, int count, bool preferWeak);
std::vector<int> PickWritten(const std::vector<std::string>& topics, int count, bool preferWeak);
