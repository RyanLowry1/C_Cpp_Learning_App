// dialogue.h - the librarian's speech box with typewriter text and choice cards
#pragma once
#include "common.h"
#include "ui.h"

struct Choice {
    std::string label;
    std::string sub;          // small second line
    Color accent{ 120, 80, 50, 255 };
    float mastery = -1.f;     // 0..1 shows a progress bar, -1 hides it
    std::string badge;        // e.g. "EXAM 1"
    Icon icon = Icon::Book;
};

class Dialogue {
public:
    void Say(const std::string& text);                            // replace current speech
    void Queue(const std::string& text);                          // add after the current line
    void Ask(const std::string& text, const std::vector<Choice>& choices, std::function<void(int)> onPick,
             int columns = 3, bool allowCancel = true);
    void Clear();
    void Hide() { visible = false; }

    void Update(float dt);
    // Draw inside a HUD ui pass. W/H are the virtual HUD dimensions.
    void Draw(float W, float H, bool interactive);

    bool Visible() const { return visible; }
    bool Typing() const { return visible && shown < (int)current.size(); }
    bool HasChoices() const { return visible && !choices.empty(); }
    bool ChoicesShowing() const { return HasChoices() && !Typing(); }
    float Mouth() const { return mouth; }
    void Pick(int i) {   // programmatic choice (autopilot)
        if (!HasChoices()) return;
        auto cb = onPick; choices.clear(); onPick = nullptr; shown = (int)current.size();
        if (cb) cb(i);
    }
    void Finish() { shown = (int)current.size(); }
    void SetSpeaker(const std::string& s) { speaker = s; }

private:
    std::string speaker = "Mr. Malloc";
    std::string current;
    std::vector<std::string> queue;
    std::vector<Choice> choices;
    std::function<void(int)> onPick;
    int columns = 3;
    bool allowCancel = true;
    bool visible = false;
    int shown = 0;
    float charTimer = 0.f;
    float mouth = 0.f;
    float mouthTarget = 0.f;
    float appear = 0.f;
    float choiceAppear = 0.f;
    float idle = 0.f;
    int blipCounter = 0;
    void Advance();
};
