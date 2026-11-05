// main.cpp - entry point. `--selftest` verifies all content against the real compiler.
#include "app.h"
#include "content.h"
#include "harness.h"
#include "code_runner.h"
#include <cstdio>
#include <sstream>
#include <thread>
#include <chrono>

// Compiles every model solution against its tests, checks every starter compiles,
// and runs every "predict the output" question to confirm its answer key.
static int SelfTest(const std::string& filter) {
    SetTraceLogLevel(LOG_WARNING);
    Data().Load();
    std::ostringstream rep;
    for (auto& e : Data().loadErrors) rep << "CONTENT ERROR: " << e << "\n";
    runner::DetectAsync();
    while (runner::Compiler().detecting) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    runner::CompilerInfo ci = runner::Compiler();
    printf("Compiler: %s (%s)\n", ci.name.c_str(), ci.path.c_str());
    if (ci.kind == runner::CompilerKind::None) { printf("%s\n", ci.error.c_str()); return 2; }

    struct Item { std::string name; std::shared_ptr<runner::Job> job; int kind; std::string expect; const Problem* p; BuiltProgram bp; };
    std::vector<Item> items;
    const Content& C = Data();
    for (const Problem& p : C.problems) {
        if (!filter.empty() && p.id.find(filter) == std::string::npos) continue;
        if (!p.solution.empty()) {
            Item it; it.name = p.id + " [solution]"; it.kind = 0; it.p = &p;
            it.bp = BuildProgram(p, p.solution, false);
            it.job = std::make_shared<runner::Job>();
            it.job->source = it.bp.source; it.job->testCount = (int)it.bp.testIndex.size(); it.job->perTestTimeout = 6.f;
            items.push_back(it);
        }
        Item st; st.name = p.id + " [starter compiles]"; st.kind = 1; st.p = &p;
        st.bp = BuildProgram(p, p.starter, true);
        st.job = std::make_shared<runner::Job>();
        st.job->source = st.bp.source; st.job->testCount = (int)st.bp.testIndex.size(); st.job->perTestTimeout = 2.f;
        items.push_back(st);
    }
    for (const Written& w : C.written) {
        if (!w.runnable) continue;
        if (!filter.empty() && w.id.find(filter) == std::string::npos) continue;
        Item it; it.name = w.id + " [written output]"; it.kind = 2; it.p = nullptr; it.expect = w.answers[0];
        it.job = std::make_shared<runner::Job>();
        it.job->source = BuildOutputProgram(w.code); it.job->outputOnly = true; it.job->perTestTimeout = 5.f;
        items.push_back(it);
    }
    for (const MCQ& q : C.mcqs) {
        if (!q.verifyOutput) continue;
        if (!filter.empty() && q.id.find(filter) == std::string::npos) continue;
        Item it; it.name = q.id + " [mcq output]"; it.kind = 2; it.p = nullptr; it.expect = q.options[(size_t)q.correct];
        it.job = std::make_shared<runner::Job>();
        it.job->source = BuildOutputProgram(q.code); it.job->outputOnly = true; it.job->perTestTimeout = 5.f;
        items.push_back(it);
    }
    printf("Running %d checks...\n", (int)items.size());
    // run up to 5 jobs at a time (each job gets its own work folder)
    size_t next = 0, done = 0;
    std::vector<size_t> active;
    while (done < items.size()) {
        while (active.size() < 5 && next < items.size()) { runner::Run(items[next].job); active.push_back(next); next++; }
        for (size_t k = 0; k < active.size();) {
            if (items[active[k]].job->done) { done++; active.erase(active.begin() + (long)k); } else k++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    int fails = 0;
    for (Item& it : items) {
        runner::Job& j = *it.job;
        bool ok = true;
        std::ostringstream why;
        if (!j.fatal.empty() && it.kind != 2) { ok = false; why << "  fatal: " << j.fatal << "\n"; }
        if (!j.compiled) {
            ok = false;
            why << "  compile failed:\n";
            for (auto& d : j.diags) if (d.error) why << "    " << d.file << "(" << d.line << "): " << d.message << "\n";
            if (j.diags.empty()) why << j.compilerLog.substr(0, 1500) << "\n";
        } else if (it.kind == 0) {
            for (size_t i = 0; i < j.results.size(); i++) {
                const runner::TestResult& r = j.results[i];
                if (r.status == runner::TestStatus::Pass) continue;
                ok = false;
                why << "  test '" << it.p->tests[(size_t)it.bp.testIndex[i]].label << "': ";
                if (r.status == runner::TestStatus::Fail) why << "expected [" << r.expected << "] got [" << r.got << "] (" << r.expr << ")\n";
                else why << r.message << "\n";
            }
        } else if (it.kind == 2) {
            std::string got = NormaliseAnswer(j.programOutput, false), exp = NormaliseAnswer(it.expect, false);
            if (got != exp && NormaliseAnswer(j.programOutput, true) != NormaliseAnswer(it.expect, true)) {
                ok = false;
                why << "  output [" << util::Trim(j.programOutput) << "] != answer [" << it.expect << "]\n";
            }
        }
        if (!ok) { fails++; rep << "FAIL " << it.name << "\n" << why.str(); }
    }
    rep << "\n" << (items.size() - (size_t)fails) << " / " << items.size() << " checks passed\n";
    std::string out = rep.str();
    printf("%s", out.c_str());
    util::WriteTextFile(util::SavePath("selftest.txt"), out);
    return fails == 0 ? 0 : 1;
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--selftest") return SelfTest(i + 1 < argc ? argv[i + 1] : "");
    }
    App app;
    return app.Run(argc, argv);
}
