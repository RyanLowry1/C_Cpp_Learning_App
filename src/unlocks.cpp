// unlocks.cpp - librarian roster and achievement rules
#include "unlocks.h"
#include "progress.h"

namespace unlocks {

const std::vector<LibrarianDef>& Roster() {
    static const std::vector<LibrarianDef> r = {
        { "malloc", "Mr. Malloc", "Head Librarian. Tweed, spectacles and a soft spot for pointers.", "Always on duty", Color{ 150, 104, 66, 255 }, "M" },
        { "pointer", "Professor Pointer", "A wizened wizard of addresses. Knows where everything lives.", "Finish a Reading Room chapter", Color{ 96, 62, 150, 255 }, "P" },
        { "heap", "Dr. Heap", "Lab coat, wild hair, zero memory leaks. Allegedly.", "Score 100% on a multiple choice quiz", Color{ 40, 128, 132, 255 }, "H" },
        { "ada", "Ada the Compiler", "Fast, cheerful and merciless about missing semicolons.", "Solve a coding problem", Color{ 200, 96, 60, 255 }, "A" },
        { "byte", "B.Y.T.E.", "Binary Yielding Tutor Engine. Runs on tea and optimism. Beeps when impressed.", "Answer 100 questions in total", Color{ 36, 150, 168, 255 }, "B" },
    };
    return r;
}

const LibrarianDef* Find(const std::string& id) {
    for (const LibrarianDef& d : Roster()) if (d.id == id) return &d;
    return nullptr;
}

bool IsUnlocked(const std::string& id) {
    // demoUnlock is the developer option in Settings: everyone is available while it is on, and nothing is added to `unlocked`
    return id == "malloc" || Prog().settings.demoUnlock || Prog().unlocked.count(id) > 0;
}

std::vector<std::string> Check() {
    Progress& p = Prog();
    std::vector<std::string> fresh;
    auto grant = [&](const char* id) {
        if (p.unlocked.insert(id).second) fresh.push_back(id);
    };
    bool chapter = false, perfectQuiz = false, solved = false;
    for (const Session& s : p.sessions) {
        if (s.mode == "reading" && s.total > 0 && s.score >= s.total - 1) chapter = true;           // read to the end
        if ((s.mode == "mcq" || s.mode == "drill") && s.total >= 5 && s.score == s.total) perfectQuiz = true;
    }
    for (auto& kv : p.code) if (kv.second.solved) solved = true;
    if (chapter) grant("pointer");
    if (perfectQuiz) grant("heap");
    if (solved) grant("ada");
    if (p.TotalAnswered() >= 100) grant("byte");
    if (!fresh.empty()) p.Save();
    return fresh;
}

const char* TierName(float f) {
    if (f < 0.12f) return "Bookworm";
    if (f < 0.30f) return "Casual Lifter";
    if (f < 0.50f) return "Gym Regular";
    if (f < 0.70f) return "Fitness Fanatic";
    if (f < 0.88f) return "Absolute Unit";
    return "Gym Legend";
}

} // namespace unlocks
