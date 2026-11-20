// unlocks.cpp - librarian roster and achievement rules
#include "unlocks.h"
#include "progress.h"

namespace unlocks {

const std::vector<LibrarianDef>& Roster() {
    static const std::vector<LibrarianDef> r = {
        { "malloc", "Mr. Malloc", "Head Librarian. Tweed, spectacles and a soft spot for pointers.", "Always on duty", Color{ 150, 104, 66, 255 }, "M", "A. MALLOC\nHead Librarian" },
        { "pointer", "Professor Pointer", "A wizened wizard of addresses. Knows where everything lives.", "Finish a Reading Room chapter", Color{ 96, 62, 150, 255 }, "P", "PROF. POINTER\nWizard of Addresses" },
        { "heap", "Dr. Heap", "Lab coat, wild hair, zero memory leaks. Allegedly.", "Score 100% on a multiple choice quiz", Color{ 40, 128, 132, 255 }, "H", "DR. HEAP\nDynamic Memory" },
        { "ada", "Ada the Compiler", "Fast, cheerful and merciless about missing semicolons.", "Solve a coding problem", Color{ 200, 96, 60, 255 }, "A", "ADA\nChief Compiler" },
        { "byte", "B.Y.T.E.", "Binary Yielding Tutor Engine. Runs on tea and optimism. Beeps when impressed.", "Answer 100 questions in total", Color{ 36, 150, 168, 255 }, "B", "B.Y.T.E.\nTutor Engine" },
        { "stack", "Captain Stack", "Last in, first out. Runs a tight ship and an even tighter call stack.", "Solve 5 coding problems", Color{ 36, 74, 138, 255 }, "S", "CAPT. STACK\nLast In, First Out" },
        { "nana", "Nana Free", "Kettle on, glasses on a chain. Frees everything she allocates.", "Score 100% on a written test", Color{ 186, 84, 112, 255 }, "N", "NANA FREE\nMemory Housekeeper" },
        { "kernel", "Kernel Panic", "Pink mohawk, black leather. Mostly calm. Mostly.", "Score 50% or more in a mock exam", Color{ 74, 82, 104, 255 }, "K", "K. PANIC\nDon't Panic" },
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
    bool chapter = false, perfectQuiz = false, perfectWritten = false, mockPassed = false;
    for (const Session& s : p.sessions) {
        if (s.mode == "reading" && s.total > 0 && s.score >= s.total - 1) chapter = true;           // read to the end
        if ((s.mode == "mcq" || s.mode == "drill") && s.total >= 5 && s.score == s.total) perfectQuiz = true;
        if (s.mode == "written" && s.total >= 4 && s.score == s.total) perfectWritten = true;
        if (s.mode == "mock" && s.total > 0 && s.score * 2 >= s.total) mockPassed = true;           // opening one and closing it again scores nothing
    }
    int solved = 0;
    for (auto& kv : p.code) if (kv.second.solved) solved++;
    if (chapter) grant("pointer");
    if (perfectQuiz) grant("heap");
    if (solved >= 1) grant("ada");
    if (p.TotalAnswered() >= 100) grant("byte");
    if (solved >= 5) grant("stack");
    if (perfectWritten) grant("nana");
    if (mockPassed) grant("kernel");
    if (!fresh.empty()) p.Save();
    return fresh;
}

} // namespace unlocks
