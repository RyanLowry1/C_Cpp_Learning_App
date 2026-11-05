// harness.cpp - generates the test program that is compiled with the learner's code
#include "harness.h"
#include <sstream>

using namespace util;

static const char* kPreludeC = R"HX(// ---- Malloc's Library test harness (C mode) ----
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <string>
#include <vector>
#include <iostream>
#include <exception>
)HX";

static const char* kPreludeCpp = R"HX(// ---- Malloc's Library test harness (C++ mode) ----
#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>
#include <climits>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <set>
#include <stack>
#include <queue>
#include <algorithm>
#include <iterator>
#include <utility>
#include <exception>
using namespace std;
)HX";

static const char* kCore = R"HX(
// ---------------------------------------------------------------------------
#include <io.h>
#include <fcntl.h>
#ifdef _MSC_VER
#include <excpt.h>
#include <malloc.h>
#endif
namespace __h {
static bool failed = false;
static std::string got, expected, expr;
static int capturing = 0;
static int savedFd = -1;
static FILE* capFile = nullptr;
inline std::string str(bool v) { return v ? "true" : "false"; }
inline std::string str(char v) { if (v == 0) return "'\\0'"; std::string s = "'"; s += v; s += "'"; return s; }
inline std::string str(signed char v) { return std::to_string((int)v); }
inline std::string str(unsigned char v) { return std::to_string((int)v); }
inline std::string str(short v) { return std::to_string((int)v); }
inline std::string str(unsigned short v) { return std::to_string((int)v); }
inline std::string str(int v) { return std::to_string(v); }
inline std::string str(long v) { return std::to_string(v); }
inline std::string str(long long v) { return std::to_string(v); }
inline std::string str(unsigned v) { return std::to_string(v); }
inline std::string str(unsigned long v) { return std::to_string(v); }
inline std::string str(unsigned long long v) { return std::to_string(v); }
inline std::string str(float v) { char b[64]; snprintf(b, sizeof b, "%g", (double)v); return b; }
inline std::string str(double v) { char b[64]; snprintf(b, sizeof b, "%g", v); return b; }
inline std::string str(const char* v) { return v ? "\"" + std::string(v) + "\"" : std::string("NULL"); }
inline std::string str(char* v) { return str((const char*)v); }
inline std::string str(const std::string& v) { return "\"" + v + "\""; }
inline std::string str(std::nullptr_t) { return "nullptr"; }
template <class T> std::string str(T* p) { if (!p) return "nullptr"; char b[32]; snprintf(b, sizeof b, "%p", (void*)p); return b; }
template <class T> std::string str(const T&) { return "(value)"; }
inline void fail(const std::string& g, const std::string& e, const char* x) {
    if (failed) return;
    failed = true; got = g; expected = e; expr = x;
}
inline void cap_begin() {
    fflush(stdout); std::cout.flush();
    capFile = fopen("__capture.txt", "w+b");
    if (!capFile) return;
    savedFd = _dup(_fileno(stdout));
    _dup2(_fileno(capFile), _fileno(stdout));
    capturing = 1;
}
inline std::string cap_end() {
    if (!capturing) return std::string();
    fflush(stdout); std::cout.flush();
    _dup2(savedFd, _fileno(stdout)); _close(savedFd); capturing = 0;
    fseek(capFile, 0, SEEK_END); long n = ftell(capFile); fseek(capFile, 0, SEEK_SET);
    std::string s; if (n > 0) { s.resize((size_t)n); size_t r = fread(&s[0], 1, (size_t)n, capFile); s.resize(r); }
    fclose(capFile); capFile = nullptr;
    std::string o; for (char c : s) if (c != '\r') o += c;
    return o;
}
inline void cap_abort() { if (capturing) { cap_end(); } }
inline std::string norm(const std::string& in, bool tokens) {
    std::vector<std::string> lines; std::string cur;
    for (char c : in) { if (c == '\r') continue; if (c == '\n') { lines.push_back(cur); cur.clear(); } else cur += c; }
    lines.push_back(cur);
    for (auto& l : lines) {
        if (tokens) {
            std::string o; bool sp = false;
            for (char c : l) { if (c == ' ' || c == '\t') { sp = true; continue; } if (sp && !o.empty()) o += ' '; sp = false; o += c; }
            l = o;
        } else {
            while (!l.empty() && (l.back() == ' ' || l.back() == '\t')) l.pop_back();
        }
    }
    while (!lines.empty() && lines.back().empty()) lines.pop_back();
    size_t first = 0; while (first < lines.size() && lines[first].empty()) first++;
    std::string out;
    for (size_t i = first; i < lines.size(); i++) { if (i > first) out += '\n'; out += lines[i]; }
    return out;
}
inline std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) { if (c == '\\') o += "\\\\"; else if (c == '\n') o += "\\n"; else if (c == '\t') o += "\\t"; else if (c == '\r') {} else o += c; }
    if (o.size() > 600) o = o.substr(0, 600) + "...";
    return o;
}
} // namespace __h

#define EXPECT_EQ(actual, expected) do { auto __a = (actual); auto __e = (expected); if (!(__a == __e)) __h::fail(__h::str(__a), __h::str(__e), #actual); } while (0)
#define EXPECT_TRUE(cond) do { if (!(cond)) __h::fail("false", "true", #cond); } while (0)
#define EXPECT_FALSE(cond) do { if ((cond)) __h::fail("true", "false", #cond); } while (0)
#define EXPECT_STR(actual, expected) do { const char* __a = (actual); const char* __e = (expected); if (!__a || strcmp(__a, __e) != 0) __h::fail(__h::str(__a), __h::str(__e), #actual); } while (0)
#define EXPECT_NEAR(actual, expected, eps) do { double __a = (double)(actual); double __e = (double)(expected); if (!(__a - __e <= (eps) && __e - __a <= (eps))) __h::fail(__h::str(__a), __h::str(__e), #actual); } while (0)
#define EXPECT_SIGN(actual, s) do { auto __a = (actual); int __s = (__a > 0) - (__a < 0); if (__s != (s)) __h::fail(__h::str(__a), (s) < 0 ? "a negative value" : ((s) > 0 ? "a positive value" : "0"), #actual); } while (0)
#define EXPECT_ARR(arr, n, ...) do { \
    std::vector<long long> __e = __VA_ARGS__; std::string __g = "{", __x = "{"; bool __ok = ((int)__e.size() == (int)(n)); \
    for (int __i = 0; __i < (int)(n); __i++) { long long __v = (long long)(arr)[__i]; if (__i) __g += ", "; __g += std::to_string(__v); \
        if (__i >= (int)__e.size() || __v != __e[(size_t)__i]) __ok = false; } \
    for (size_t __i = 0; __i < __e.size(); __i++) { if (__i) __x += ", "; __x += std::to_string(__e[__i]); } \
    __g += "}"; __x += "}"; if (!__ok) __h::fail(__g, __x, #arr); } while (0)
#define EXPECT_OUT(stmt, expected) do { __h::cap_begin(); stmt; std::string __o = __h::cap_end(); \
    std::string __n = __h::norm(__o, false), __x = __h::norm(expected, false); if (__n != __x) __h::fail(__n, __x, #stmt); } while (0)
#define EXPECT_OUT_WS(stmt, expected) do { __h::cap_begin(); stmt; std::string __o = __h::cap_end(); \
    std::string __n = __h::norm(__o, true), __x = __h::norm(expected, true); if (__n != __x) __h::fail(__n, __x, #stmt); } while (0)
)HX";

static const char* kMain = R"HX(
static int __run_cpp(int i) {
    try { __tests[i](); }
    catch (const std::exception& e) { __h::fail(std::string("exception: ") + e.what(), "no exception", "(test body)"); }
    catch (...) { __h::fail("an unknown exception", "no exception", "(test body)"); }
    return 0;
}
#ifdef _MSC_VER
static int __run_seh(int i) {
    __try { return __run_cpp(i); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return (int)GetExceptionCode(); }
}
#else
static int __run_seh(int i) { return __run_cpp(i); }
#endif
int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    int start = argc > 1 ? atoi(argv[1]) : 0;
    for (int i = start; i < __ntests; i++) {
        printf("\n@@BEGIN %d\n", i);
        __h::failed = false; __h::got.clear(); __h::expected.clear(); __h::expr.clear();
        int code = __run_seh(i);
        __h::cap_abort();
        if (code != 0) {
#ifdef _MSC_VER
            if ((unsigned)code == 0xC00000FDu) _resetstkoflw();
#endif
            printf("\n@@CRASH %d %u\n", i, (unsigned)code);
            continue;
        }
        if (__h::failed) printf("\n@@FAIL %d\t%s\t%s\t%s\n", i, __h::esc(__h::got).c_str(), __h::esc(__h::expected).c_str(), __h::esc(__h::expr).c_str());
        else printf("\n@@PASS %d\n", i);
    }
    printf("\n@@DONE\n");
    return 0;
}
)HX";

BuiltProgram BuildProgram(const Problem& p, const std::string& userCode, bool samplesOnly) {
    BuiltProgram bp;
    std::ostringstream o;
    o << (p.prelude == "c" ? kPreludeC : kPreludeCpp);
    if (!p.support.empty()) {
        o << "#line 1 \"given_code.cpp\"\n" << p.support << "\n";
    }
    o << "#line 1 \"your_code.cpp\"\n" << userCode << "\n";
    o << "#line 1 \"harness.cpp\"\n" << kCore;
    if (!p.helpers.empty()) o << "#line 1 \"test_helpers.cpp\"\n" << p.helpers << "\n#line 200 \"harness.cpp\"\n";
    int n = 0;
    for (int i = 0; i < (int)p.tests.size(); i++) {
        if (samplesOnly && !p.tests[i].sample) continue;
        o << "static void __t" << n << "() {\n" << p.tests[i].code << "\n}\n";
        bp.testIndex.push_back(i);
        n++;
    }
    o << "typedef void (*__test_fn)();\nstatic __test_fn __tests[] = { ";
    for (int i = 0; i < n; i++) o << (i ? ", " : "") << "__t" << i;
    if (n == 0) o << "nullptr";
    o << " };\nstatic const int __ntests = " << n << ";\n";
    o << kMain;
    bp.source = o.str();
    return bp;
}

std::string StripCommentsAndStrings(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
            while (i < s.size() && s[i] != '\n') i++;
            if (i < s.size()) o += '\n';
            continue;
        }
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '*') {
            i += 2;
            while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/')) { if (s[i] == '\n') o += '\n'; i++; }
            i++;
            o += ' ';
            continue;
        }
        if (c == '"' || c == '\'') {
            char q = c;
            o += q;
            i++;
            while (i < s.size() && s[i] != q && s[i] != '\n') { if (s[i] == '\\') i++; i++; }
            o += q;
            continue;
        }
        o += c;
    }
    return o;
}

static bool IsIdent(char c) { return isalnum((unsigned char)c) || c == '_'; }

std::vector<std::string> CheckRules(const Problem& p, const std::string& userCode) {
    std::vector<std::string> msgs;
    std::string code = StripCommentsAndStrings(userCode);
    for (const ForbidRule& r : p.forbid) {
        bool hit = false;
        for (const std::string& tok : r.tokens) {
            bool word = IsIdent(tok[0]);
            size_t pos = 0;
            while ((pos = code.find(tok, pos)) != std::string::npos) {
                bool okL = !word || pos == 0 || !IsIdent(code[pos - 1]);
                bool okR = !word || pos + tok.size() >= code.size() || !IsIdent(code[pos + tok.size()]);
                if (okL && okR) { hit = true; break; }
                pos += tok.size();
            }
            if (hit) break;
        }
        if (hit) msgs.push_back(r.message);
    }
    // a learner-written main() would clash with the test runner's main()
    std::string c2 = code;
    size_t m = 0;
    while ((m = c2.find("main", m)) != std::string::npos) {
        bool l = m == 0 || !IsIdent(c2[m - 1]);
        size_t r = m + 4;
        while (r < c2.size() && (c2[r] == ' ' || c2[r] == '\t')) r++;
        if (l && r < c2.size() && c2[r] == '(' && (m + 4 >= c2.size() || !IsIdent(c2[m + 4]))) {
            msgs.push_back("Remove your main() function - the tests supply their own main().");
            break;
        }
        m += 4;
    }
    return msgs;
}

std::string BuildOutputProgram(const std::string& code) {
    std::string stripped = StripCommentsAndStrings(code);
    if (stripped.find("main(") != std::string::npos || stripped.find("main (") != std::string::npos) {
        return std::string("#define _CRT_SECURE_NO_WARNINGS\n") + code + "\n";
    }
    std::string o = kPreludeCpp;
    o += "int main() {\n" + code + "\nreturn 0;\n}\n";
    return o;
}

std::string NormaliseAnswer(const std::string& s, bool removeAllSpaces) {
    std::string t = ToLower(Trim(s));
    // tolerate smart quotes pasted from documents
    t = ReplaceAll(t, "\xE2\x80\x98", "'"); t = ReplaceAll(t, "\xE2\x80\x99", "'");
    t = ReplaceAll(t, "\xE2\x80\x9C", "\""); t = ReplaceAll(t, "\xE2\x80\x9D", "\"");
    if (removeAllSpaces) {
        std::string o;
        for (char c : t) if (c != ' ' && c != '\t' && c != '\n') o += c;
        while (!o.empty() && o.back() == ';') o.pop_back();
        return o;
    }
    t = CollapseSpaces(t);
    while (!t.empty() && (t.back() == '.' )) t.pop_back();
    return t;
}
