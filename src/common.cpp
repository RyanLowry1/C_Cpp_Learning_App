// common.cpp - shared helper implementations
#include "common.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <algorithm>

namespace fs = std::filesystem;

namespace util {

static std::string ExeDir() {
    const char* d = GetApplicationDirectory();
    return d ? std::string(d) : std::string("./");
}

std::string AssetPath(const std::string& rel) {
    std::string p = ExeDir() + "assets/" + rel;
    if (fs::exists(p)) return p;
    std::string q = "assets/" + rel;
    if (fs::exists(q)) return q;
    return p;
}

std::string SavePath(const std::string& rel) {
    std::string dir = ExeDir() + "save/";
    EnsureDir(dir);
    std::string full = dir + rel;
    fs::path parent = fs::path(full).parent_path();
    if (!parent.empty()) EnsureDir(parent.string());
    return full;
}

std::string ReadTextFile(const std::string& path, bool* ok) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { if (ok) *ok = false; return std::string(); }
    std::stringstream ss; ss << f.rdbuf();
    if (ok) *ok = true;
    std::string s = ss.str();
    // strip UTF-8 BOM and carriage returns
    if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) s = s.substr(3);
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    return s;
}

bool WriteTextFile(const std::string& path, const std::string& text) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << text;
    return (bool)f;
}

bool FileExistsAt(const std::string& path) { std::error_code ec; return fs::exists(path, ec); }

void EnsureDir(const std::string& path) {
    std::error_code ec;
    fs::create_directories(path, ec);
}

std::string TempWorkDir() {
    std::error_code ec;
    fs::path t = fs::temp_directory_path(ec);
    if (ec) t = fs::path(ExeDir()) / "save" / "tmp";
    fs::path d = t / "MallocsLibrary";
    fs::create_directories(d, ec);
    return d.string();
}

std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n' || s[b - 1] == '\r')) b--;
    return s.substr(a, b - a);
}

std::vector<std::string> Split(const std::string& s, char d) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) { if (c == d) { out.push_back(cur); cur.clear(); } else cur += c; }
    out.push_back(cur);
    return out;
}

std::vector<std::string> SplitLines(const std::string& s) { return Split(s, '\n'); }

std::string ToLower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
bool StartsWith(const std::string& s, const std::string& p) { return s.size() >= p.size() && s.compare(0, p.size(), p) == 0; }
bool EndsWith(const std::string& s, const std::string& p) { return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0; }

std::string ReplaceAll(std::string s, const std::string& a, const std::string& b) {
    if (a.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(a, pos)) != std::string::npos) { s.replace(pos, a.size(), b); pos += b.size(); }
    return s;
}

std::string Unescape(const std::string& s) {
    std::string o; o.reserve(s.size());
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

std::string CollapseSpaces(const std::string& s) {
    std::string o; bool sp = false;
    for (char c : s) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { sp = true; continue; }
        if (sp && !o.empty()) o += ' ';
        sp = false; o += c;
    }
    return o;
}

long long NowEpoch() { return (long long)std::time(nullptr); }

std::string FormatDate(long long epoch) {
    std::time_t t = (std::time_t)epoch;
    std::tm tmv{};
    localtime_s(&tmv, &t);
    char buf[64];
    std::strftime(buf, sizeof buf, "%d %b %Y, %H:%M", &tmv);
    return buf;
}

std::string FormatDuration(float seconds) {
    if (seconds < 0) seconds = 0;
    int s = (int)seconds;
    char buf[32];
    snprintf(buf, sizeof buf, "%02d:%02d", s / 60, s % 60);
    return buf;
}

} // namespace util
