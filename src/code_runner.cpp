// code_runner.cpp - Windows process management for compiling / running learner code
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "code_runner.h"
#include <thread>
#include <mutex>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>

namespace fs = std::filesystem;

namespace runner {

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
static std::wstring Widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
static std::string Narrow(const std::wstring& w) {
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
static std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

struct ProcResult {
    DWORD exitCode = 0;
    std::string out;
    bool timedOut = false;
    bool killed = false;
    bool started = false;
    double ms = 0;
};

// Run a process, capturing stdout+stderr. `watch` is polled with the output so far
// and elapsed ms; returning true kills the process.
static ProcResult Exec(const std::wstring& app, const std::wstring& cmdline, const std::wstring& cwd,
                       const std::wstring* env, DWORD timeoutMs, size_t maxOut,
                       const std::function<bool(const std::string&, double)>& watch = nullptr) {
    ProcResult pr;
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE outR = nullptr, outW = nullptr, inR = nullptr, inW = nullptr;
    if (!CreatePipe(&outR, &outW, &sa, 1 << 16)) return pr;
    SetHandleInformation(outR, HANDLE_FLAG_INHERIT, 0);
    if (!CreatePipe(&inR, &inW, &sa, 0)) { CloseHandle(outR); CloseHandle(outW); return pr; }
    SetHandleInformation(inW, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = outW;
    si.hStdError = outW;
    si.hStdInput = inR;
    PROCESS_INFORMATION pi{};
    std::wstring cl = cmdline;
    DWORD flags = CREATE_NO_WINDOW | (env ? CREATE_UNICODE_ENVIRONMENT : 0);
    BOOL ok = CreateProcessW(app.empty() ? nullptr : app.c_str(), &cl[0], nullptr, nullptr, TRUE, flags,
                             env ? (LPVOID)env->c_str() : nullptr, cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
    CloseHandle(outW);
    CloseHandle(inR);
    CloseHandle(inW);   // learner code that reads stdin sees EOF instead of hanging
    if (!ok) { CloseHandle(outR); return pr; }
    pr.started = true;

    auto t0 = std::chrono::steady_clock::now();
    char buf[4096];
    bool running = true;
    while (true) {
        DWORD avail = 0;
        bool gotData = false;
        while (PeekNamedPipe(outR, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
            DWORD rd = 0;
            if (!ReadFile(outR, buf, std::min<DWORD>(avail, sizeof buf), &rd, nullptr) || rd == 0) break;
            if (pr.out.size() < maxOut) pr.out.append(buf, rd);
            gotData = true;
        }
        if (!running) break;
        DWORD w = WaitForSingleObject(pi.hProcess, gotData ? 0 : 8);
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        if (w == WAIT_OBJECT_0) { running = false; continue; }   // drain once more
        if (ms > timeoutMs) { pr.timedOut = true; TerminateProcess(pi.hProcess, 1); WaitForSingleObject(pi.hProcess, 2000); running = false; continue; }
        if (watch && watch(pr.out, ms)) { pr.killed = true; TerminateProcess(pi.hProcess, 1); WaitForSingleObject(pi.hProcess, 2000); running = false; continue; }
        if (pr.out.size() >= maxOut) { pr.killed = true; TerminateProcess(pi.hProcess, 1); WaitForSingleObject(pi.hProcess, 2000); running = false; continue; }
    }
    // final drain
    for (;;) {
        DWORD avail = 0;
        if (!PeekNamedPipe(outR, nullptr, 0, nullptr, &avail, nullptr) || avail == 0) break;
        DWORD rd = 0;
        if (!ReadFile(outR, buf, std::min<DWORD>(avail, sizeof buf), &rd, nullptr) || rd == 0) break;
        if (pr.out.size() < maxOut) pr.out.append(buf, rd);
    }
    GetExitCodeProcess(pi.hProcess, &pr.exitCode);
    pr.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(outR);
    pr.out.erase(std::remove(pr.out.begin(), pr.out.end(), '\r'), pr.out.end());
    return pr;
}

// ---------------------------------------------------------------------------
// compiler detection
// ---------------------------------------------------------------------------
static std::mutex g_mx;
static CompilerInfo g_info;
static std::wstring g_env;         // MSVC environment block (from vcvars64.bat)
static std::wstring g_compilerPath;
static bool g_started = false;

static std::wstring SearchIn(const std::wstring& pathVar, const wchar_t* exe) {
    std::wstringstream ss(pathVar);
    std::wstring dir;
    while (std::getline(ss, dir, L';')) {
        if (dir.empty()) continue;
        fs::path p = fs::path(dir) / exe;
        std::error_code ec;
        if (fs::exists(p, ec)) return p.wstring();
    }
    return std::wstring();
}

static void DetectWorker() {
    CompilerInfo info;
    info.detecting = false;
    std::wstring tmp = fs::temp_directory_path().wstring();

    // 1) Visual Studio via vswhere
    std::vector<std::wstring> vcvarsCandidates;
    wchar_t pf86[MAX_PATH] = { 0 };
    GetEnvironmentVariableW(L"ProgramFiles(x86)", pf86, MAX_PATH);
    std::wstring vswhere = std::wstring(pf86) + L"\\Microsoft Visual Studio\\Installer\\vswhere.exe";
    std::error_code ec;
    if (fs::exists(vswhere, ec)) {
        ProcResult r = Exec(vswhere, L"\"" + vswhere + L"\" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath",
                            tmp, nullptr, 15000, 1 << 16);
        std::string p = Trim(r.out);
        if (!p.empty()) {
            std::string first = p.substr(0, p.find('\n'));
            vcvarsCandidates.push_back(Widen(Trim(first)) + L"\\VC\\Auxiliary\\Build\\vcvars64.bat");
        }
    }
    const wchar_t* roots[] = { L"C:\\Program Files\\Microsoft Visual Studio", L"C:\\Program Files (x86)\\Microsoft Visual Studio" };
    const wchar_t* vers[] = { L"18", L"2022", L"2019" };
    const wchar_t* eds[] = { L"Community", L"Professional", L"Enterprise", L"BuildTools" };
    for (auto r : roots) for (auto v : vers) for (auto e : eds)
        vcvarsCandidates.push_back(std::wstring(r) + L"\\" + v + L"\\" + e + L"\\VC\\Auxiliary\\Build\\vcvars64.bat");

    for (const std::wstring& vc : vcvarsCandidates) {
        if (!fs::exists(vc, ec)) continue;
        wchar_t comspec[MAX_PATH] = { 0 };
        if (!GetEnvironmentVariableW(L"ComSpec", comspec, MAX_PATH)) wcscpy_s(comspec, L"C:\\Windows\\System32\\cmd.exe");
        std::wstring cmd = L"\"" + std::wstring(comspec) + L"\" /s /c \"\"" + vc + L"\" >nul 2>&1 && set\"";
        ProcResult r = Exec(comspec, cmd, tmp, nullptr, 60000, 1 << 20);
        if (r.out.find("VCToolsInstallDir") == std::string::npos) continue;
        // build a unicode environment block
        std::wstring block;
        std::wstring pathVar;
        std::istringstream ls(r.out);
        std::string line;
        while (std::getline(ls, line)) {
            line = Trim(line);
            size_t eq = line.find('=');
            if (eq == std::string::npos || eq == 0) continue;
            std::string key = line.substr(0, eq);
            std::wstring wl;
            int n = MultiByteToWideChar(CP_ACP, 0, line.c_str(), (int)line.size(), nullptr, 0);
            wl.resize((size_t)n);
            MultiByteToWideChar(CP_ACP, 0, line.c_str(), (int)line.size(), &wl[0], n);
            std::string ku = key;
            for (auto& c : ku) c = (char)toupper((unsigned char)c);
            if (ku == "PATH") pathVar = wl.substr((size_t)eq + 1);
            if (ku == "VSLANG") continue;
            block += wl;
            block.push_back(L'\0');
        }
        block += L"VSLANG=1033";
        block.push_back(L'\0');
        block.push_back(L'\0');
        std::wstring clPath = SearchIn(pathVar, L"cl.exe");
        if (clPath.empty()) continue;
        std::lock_guard<std::mutex> lk(g_mx);
        g_env = block;
        g_compilerPath = clPath;
        info.kind = CompilerKind::MSVC;
        info.path = Narrow(clPath);
        std::wstring vs = vc;
        info.name = "Microsoft C/C++ (MSVC)";
        g_info = info;
        return;
    }

    // 2) GCC / Clang on PATH
    wchar_t found[MAX_PATH];
    const wchar_t* others[] = { L"g++.exe", L"clang++.exe" };
    for (int i = 0; i < 2; i++) {
        if (SearchPathW(nullptr, others[i], nullptr, MAX_PATH, found, nullptr)) {
            std::lock_guard<std::mutex> lk(g_mx);
            g_compilerPath = found;
            info.kind = i == 0 ? CompilerKind::GCC : CompilerKind::Clang;
            info.path = Narrow(found);
            info.name = i == 0 ? "GNU g++" : "Clang";
            g_info = info;
            return;
        }
    }
    std::lock_guard<std::mutex> lk(g_mx);
    info.error = "No C++ compiler found. Install Visual Studio with the 'Desktop development with C++' workload.";
    g_info = info;
}

void DetectAsync() {
    {
        std::lock_guard<std::mutex> lk(g_mx);
        if (g_started) return;
        g_started = true;
        g_info.detecting = true;
    }
    std::thread(DetectWorker).detach();
}

CompilerInfo Compiler() {
    std::lock_guard<std::mutex> lk(g_mx);
    return g_info;
}

// ---------------------------------------------------------------------------
// diagnostics parsing
// ---------------------------------------------------------------------------
static void ParseDiagnostics(const std::string& log, std::vector<Diagnostic>& out) {
    std::istringstream ss(log);
    std::string line;
    while (std::getline(ss, line)) {
        // MSVC: file(line[,col]): error C1234: message
        size_t paren = line.find('(');
        size_t close = line.find("): ");
        if (paren != std::string::npos && close != std::string::npos && paren < close) {
            std::string inside = line.substr(paren + 1, close - paren - 1);
            bool digits = !inside.empty();
            for (char c : inside) if (!(isdigit((unsigned char)c) || c == ',')) digits = false;
            if (digits) {
                Diagnostic d;
                d.file = line.substr(0, paren);
                size_t slash = d.file.find_last_of("\\/");
                if (slash != std::string::npos) d.file = d.file.substr(slash + 1);
                d.line = atoi(inside.c_str());
                size_t comma = inside.find(',');
                if (comma != std::string::npos) d.col = atoi(inside.c_str() + comma + 1);
                std::string rest = line.substr(close + 3);
                d.error = rest.find("error") == 0 || rest.find("fatal error") == 0;
                if (rest.find("warning") != 0 && !d.error) continue;
                size_t c2 = rest.find(": ");
                d.message = c2 != std::string::npos ? rest.substr(c2 + 2) : rest;
                // keep the error code for reference e.g. C2065
                std::string code = c2 != std::string::npos ? rest.substr(0, c2) : "";
                size_t sp = code.rfind(' ');
                if (sp != std::string::npos) d.message += "  [" + code.substr(sp + 1) + "]";
                out.push_back(d);
                continue;
            }
        }
        // GCC / Clang: file:line:col: error: message
        size_t e = line.find(": error: "), w = line.find(": warning: ");
        size_t k = e != std::string::npos ? e : w;
        if (k != std::string::npos) {
            std::string head = line.substr(0, k);
            size_t c1 = head.rfind(':');
            if (c1 == std::string::npos) continue;
            size_t c0 = head.rfind(':', c1 - 1);
            Diagnostic d;
            if (c0 != std::string::npos && c0 > 1) {
                d.file = head.substr(0, c0);
                d.line = atoi(head.c_str() + c0 + 1);
                d.col = atoi(head.c_str() + c1 + 1);
            } else {
                d.file = head.substr(0, c1);
                d.line = atoi(head.c_str() + c1 + 1);
            }
            size_t slash = d.file.find_last_of("\\/");
            if (slash != std::string::npos) d.file = d.file.substr(slash + 1);
            d.error = e != std::string::npos;
            d.message = line.substr(k + (d.error ? 9 : 11));
            out.push_back(d);
        }
    }
}

static std::string Unesc(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[i + 1];
            if (n == 'n') { o += '\n'; i++; continue; }
            if (n == 't') { o += '\t'; i++; continue; }
            if (n == '\\') { o += '\\'; i++; continue; }
        }
        o += s[i];
    }
    return o;
}

static std::string CrashMessage(unsigned code) {
    switch (code) {
        case 0xC0000005u: return "Crashed: access violation (segmentation fault) - a bad pointer was dereferenced (null, uninitialised or deleted memory).";
        case 0xC00000FDu: return "Crashed: stack overflow - probably infinite recursion.";
        case 0xC0000094u: return "Crashed: integer division by zero.";
        case 0xC000008Cu: return "Crashed: array bounds exceeded.";
        case 0xC0000374u: return "Crashed: heap corruption - memory was freed twice or written out of bounds.";
        case 0xC0000409u: return "Crashed: stack buffer overrun - you wrote past the end of a local array.";
        case 0x80000003u: return "Crashed: debug break / abort() was called (assertion or heap check failed).";
        case 3u: return "Crashed: abort() was called (e.g. an uncaught exception or failed assertion).";
        default: {
            char b[96];
            snprintf(b, sizeof b, "Crashed with exit code 0x%08X.", code);
            return b;
        }
    }
}

// ---------------------------------------------------------------------------
// job execution
// ---------------------------------------------------------------------------
static std::atomic<int> g_jobCounter{ 0 };

static void Execute(Job& job) {
    CompilerInfo ci = Compiler();
    for (int wait = 0; ci.detecting && wait < 600; wait++) { Sleep(100); ci = Compiler(); }
    if (ci.kind == CompilerKind::None) {
        job.fatal = ci.error.empty() ? "No compiler available." : ci.error;
        job.done = true;
        return;
    }
    std::wstring envCopy, compiler;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        envCopy = g_env;
        compiler = g_compilerPath;
    }
    // every job gets its own folder: antivirus scanners and exiting processes can keep
    // files locked for a moment, so re-using a folder straight away is unreliable
    int id = g_jobCounter++;
    fs::path root = fs::temp_directory_path() / "MallocsLibrary";
    fs::path dir = root / ("run_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(id));
    std::error_code ec;
    if (id == 0) {   // tidy folders left behind by earlier sessions
        for (auto& e : fs::directory_iterator(root, ec)) {
            std::string n = e.path().filename().string();
            if (n.rfind("run", 0) == 0) fs::remove_all(e.path(), ec);
        }
    }
    fs::create_directories(dir, ec);
    {
        std::ofstream f(dir / "prog.cpp", std::ios::binary | std::ios::trunc);
        f << job.source;
    }
    // remove this job's folder (best effort) once we are finished with it
    struct Cleanup { fs::path d; ~Cleanup() { std::error_code e; for (int i = 0; i < 3; i++) { if (fs::remove_all(d, e) || !fs::exists(d, e)) break; Sleep(50); } } } cleanup{ dir };
    std::wstring cwd = dir.wstring();
    std::wstring cmd;
    if (ci.kind == CompilerKind::MSVC)
        cmd = L"\"" + compiler + L"\" /nologo /EHsc /std:c++17 /W3 /wd4996 /wd4244 /wd4267 /wd4305 /wd4101 /wd4100 /D_CRT_SECURE_NO_WARNINGS /utf-8 /Od /Fe:prog.exe prog.cpp";
    else
        cmd = L"\"" + compiler + L"\" -std=c++17 -O0 -w -o prog.exe prog.cpp";

    ProcResult cr = Exec(compiler, cmd, cwd, envCopy.empty() ? nullptr : &envCopy, 90000, 1 << 20);
    // antivirus scanners briefly lock freshly written files; back off and retry
    for (int attempt = 0; attempt < 6 && cr.exitCode != 0 &&
         (cr.out.find("Permission denied") != std::string::npos || cr.out.find("LNK1104") != std::string::npos ||
          cr.out.find("LNK1168") != std::string::npos); attempt++) {
        Sleep(150 * (attempt + 1));
        cr = Exec(compiler, cmd, cwd, envCopy.empty() ? nullptr : &envCopy, 90000, 1 << 20);
    }
    job.compileMs = cr.ms;
    job.compilerLog = cr.out;
    ParseDiagnostics(cr.out, job.diags);
    if (!cr.started) { job.fatal = "Could not start the compiler."; job.done = true; return; }
    job.compiled = cr.exitCode == 0 && fs::exists(dir / "prog.exe", ec);
    if (!job.compiled) { job.done = true; return; }

    std::wstring exe = (dir / "prog.exe").wstring();
    auto t0 = std::chrono::steady_clock::now();

    if (job.outputOnly) {
        ProcResult r = Exec(exe, L"\"" + exe + L"\"", cwd, nullptr, (DWORD)(job.perTestTimeout * 1000), 1 << 18);
        job.programOutput = r.out;
        if (r.timedOut) job.fatal = "The program did not finish (time limit exceeded).";
        job.runMs = r.ms;
        job.done = true;
        return;
    }

    job.results.assign((size_t)job.testCount, TestResult{});
    int start = 0;
    int guard = 0;
    while (start < job.testCount && guard++ < job.testCount + 2) {
        int current = start;
        double beginMs = 0;
        size_t scanned = 0;
        int beginsSeen = 0;
        auto watch = [&](const std::string& out, double ms) {
            size_t pos;
            while ((pos = out.find("@@BEGIN ", scanned)) != std::string::npos) {
                current = atoi(out.c_str() + pos + 8);
                beginMs = ms;
                beginsSeen++;
                scanned = pos + 8;
            }
            return ms - beginMs > job.perTestTimeout * 1000.0;
        };
        ProcResult r = Exec(exe, L"\"" + exe + L"\" " + std::to_wstring(start), cwd, nullptr,
                            (DWORD)(job.perTestTimeout * 1000 * (job.testCount + 1)), 1 << 19, watch);
        bool doneMarker = false;
        int lastBegin = start;
        std::istringstream ss(r.out);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.rfind("@@", 0) != 0) {
                if (job.programOutput.size() < 20000) { job.programOutput += line; job.programOutput += '\n'; }
                continue;
            }
            if (line.rfind("@@BEGIN ", 0) == 0) { lastBegin = atoi(line.c_str() + 8); continue; }
            if (line.rfind("@@PASS ", 0) == 0) {
                int i = atoi(line.c_str() + 7);
                if (i >= 0 && i < job.testCount) job.results[(size_t)i].status = TestStatus::Pass;
                continue;
            }
            if (line.rfind("@@FAIL ", 0) == 0) {
                std::string rest = line.substr(7);
                std::vector<std::string> parts;
                std::string cur;
                for (char c : rest) { if (c == '\t') { parts.push_back(cur); cur.clear(); } else cur += c; }
                parts.push_back(cur);
                int i = atoi(parts[0].c_str());
                if (i >= 0 && i < job.testCount) {
                    TestResult& tr = job.results[(size_t)i];
                    tr.status = TestStatus::Fail;
                    if (parts.size() > 1) tr.got = Unesc(parts[1]);
                    if (parts.size() > 2) tr.expected = Unesc(parts[2]);
                    if (parts.size() > 3) tr.expr = Unesc(parts[3]);
                }
                continue;
            }
            if (line.rfind("@@CRASH ", 0) == 0) {
                int i = 0; unsigned code = 0;
                sscanf_s(line.c_str() + 8, "%d %u", &i, &code);
                if (i >= 0 && i < job.testCount) {
                    job.results[(size_t)i].status = TestStatus::Crash;
                    job.results[(size_t)i].message = CrashMessage(code);
                }
                continue;
            }
            if (line == "@@DONE") doneMarker = true;
        }
        if (doneMarker) break;
        // the process died or hung inside test `lastBegin`
        if (lastBegin >= 0 && lastBegin < job.testCount) {
            TestResult& tr = job.results[(size_t)lastBegin];
            if (r.killed || r.timedOut) {
                tr.status = TestStatus::Timeout;
                tr.message = "Time limit exceeded - your code took too long (infinite loop, or an algorithm that is far too slow).";
            } else {
                tr.status = TestStatus::Crash;
                tr.message = CrashMessage((unsigned)r.exitCode);
            }
        }
        start = lastBegin + 1;
    }
    job.runMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    job.done = true;
}

void Run(std::shared_ptr<Job> job) {
    std::thread([job]() { Execute(*job); }).detach();
}

void RunSync(Job& job) { Execute(job); }

} // namespace runner
