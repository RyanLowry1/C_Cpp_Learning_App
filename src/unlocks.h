// unlocks.h - the roster of librarians and the achievements that unlock them
#pragma once
#include "common.h"

struct LibrarianDef {
    std::string id;
    std::string name;       // "Professor Pointer"
    std::string title;      // one-line description
    std::string how;        // how to unlock, shown while locked
    Color accent;
    std::string initial;
};

namespace unlocks {
const std::vector<LibrarianDef>& Roster();
const LibrarianDef* Find(const std::string& id);
bool IsUnlocked(const std::string& id);   // earned, or the "unlock every librarian" developer option is on
// Evaluates every achievement against the saved history. Unlocks are permanent: nothing is ever re-locked.
// Returns the ids unlocked by this call (already saved).
std::vector<std::string> Check();
// Human-readable fitness tier ("Bookworm" ... "Gym Legend")
const char* TierName(float fitness);
}
