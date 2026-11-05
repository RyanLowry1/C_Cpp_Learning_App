// harness.h - turns a coding problem + the learner's code into a self-checking test program
#pragma once
#include "content.h"

struct BuiltProgram {
    std::string source;
    std::vector<int> testIndex;   // program test number -> index into Problem::tests
};

// samplesOnly: only include tests marked as samples ("Run" button) vs all tests ("Submit")
BuiltProgram BuildProgram(const Problem& p, const std::string& userCode, bool samplesOnly);

// returns human-readable messages for broken exercise rules (e.g. "no arr[i] indexing")
std::vector<std::string> CheckRules(const Problem& p, const std::string& userCode);

// for written / MCQ verification: wrap a snippet in main() if it is not a complete program
std::string BuildOutputProgram(const std::string& code);

// removes // and /* */ comments plus string / char literals (keeps line structure)
std::string StripCommentsAndStrings(const std::string& code);

// normalise program output for comparison with an expected answer
std::string NormaliseAnswer(const std::string& s, bool removeAllSpaces);
