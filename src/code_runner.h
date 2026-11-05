// code_runner.h - compiles and runs learner code in a background thread.
// Deliberately free of raylib types: the implementation includes <windows.h>.
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <atomic>

namespace runner {

enum class CompilerKind { None, MSVC, GCC, Clang };

struct CompilerInfo {
    CompilerKind kind = CompilerKind::None;
    std::string name;      // e.g. "Microsoft C/C++ (Visual Studio 18)"
    std::string path;
    bool detecting = true;
    std::string error;
};

void DetectAsync();
CompilerInfo Compiler();

struct Diagnostic {
    std::string file;
    int line = 0;
    int col = 0;
    bool error = true;
    std::string message;
};

enum class TestStatus { Pass, Fail, Crash, Timeout, NotRun };

struct TestResult {
    TestStatus status = TestStatus::NotRun;
    std::string got, expected, expr, message;
};

struct Job {
    // ---- input ----
    std::string source;
    int testCount = 0;
    float perTestTimeout = 3.0f;
    bool outputOnly = false;     // run once and capture stdout (no test protocol)
    // ---- output ----
    std::atomic<bool> done{ false };
    bool compiled = false;
    std::string compilerLog;
    std::vector<Diagnostic> diags;
    std::vector<TestResult> results;
    std::string programOutput;   // anything the learner's code printed
    double compileMs = 0, runMs = 0;
    std::string fatal;
};

void Run(std::shared_ptr<Job> job);   // asynchronous
void RunSync(Job& job);               // blocking (used by --selftest)

} // namespace runner
