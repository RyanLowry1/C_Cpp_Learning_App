// progress.cpp - simple line-based save format in save/progress.txt
#include "progress.h"
#include "content.h"
#include <sstream>
#include <algorithm>

using namespace util;

static Progress g_prog;
Progress& Prog() { return g_prog; }

static std::string SafeName(const std::string& id) {
    std::string s;
    for (char c : id) s += (isalnum((unsigned char)c) || c == '_' || c == '-') ? c : '_';
    return s;
}

static std::string EscapeField(const std::string& s) {
    std::string o;
    for (char c : s) { if (c == '\n') o += "\\n"; else if (c == '|') o += '/'; else o += c; }
    return o;
}

void Progress::Load() {
    bool ok = false;
    std::string text = ReadTextFile(SavePath("progress.txt"), &ok);
    if (!ok) return;
    Session* cur = nullptr;
    for (const std::string& raw : SplitLines(text)) {
        std::string line = Trim(raw);
        if (line.empty()) continue;
        if (line == "[session]") { sessions.push_back(Session{}); cur = &sessions.back(); continue; }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "volume") settings.volume = (float)atof(v.c_str());
        else if (k == "voice") settings.voice = v == "1";
        else if (k == "quick") settings.quickFetch = v == "1";
        else if (k == "fullscreen") settings.fullscreen = v == "1";
        else if (k == "mouselook") settings.mouseLook = v == "1";
        else if (k == "returned") booksReturned = atoi(v.c_str());
        else if (k == "item") {
            auto p = Split(v, ',');
            if (p.size() >= 5) {
                ItemStat s; s.attempts = atoi(p[1].c_str()); s.correct = atoi(p[2].c_str());
                s.last = atoll(p[3].c_str()); s.lastCorrect = p[4] == "1";
                items[p[0]] = s;
            }
        } else if (k == "code") {
            auto p = Split(v, ',');
            if (p.size() >= 6) {
                CodeStat s; s.attempts = atoi(p[1].c_str()); s.bestPassed = atoi(p[2].c_str());
                s.total = atoi(p[3].c_str()); s.solved = p[4] == "1"; s.last = atoll(p[5].c_str());
                code[p[0]] = s;
            }
        } else if (k == "chapter") {
            auto p = Split(v, ',');
            if (p.size() >= 2) chaptersRead[p[0]] = atoi(p[1].c_str());
        } else if (cur) {
            if (k == "time") cur->time = atoll(v.c_str());
            else if (k == "mode") cur->mode = v;
            else if (k == "topic") cur->topic = v;
            else if (k == "title") cur->title = Unescape(v);
            else if (k == "score") cur->score = atoi(v.c_str());
            else if (k == "total") cur->total = atoi(v.c_str());
            else if (k == "seconds") cur->seconds = (float)atof(v.c_str());
            else if (k == "items") {
                for (auto& it : Split(v, ';')) {
                    size_t c = it.rfind(':');
                    if (c == std::string::npos) continue;
                    cur->items.push_back({ it.substr(0, c), it.substr(c + 1) == "1" });
                }
            }
        }
    }
}

void Progress::Save() {
    std::ostringstream o;
    o << "# Malloc's Library progress file\n";
    o << "volume=" << settings.volume << "\n";
    o << "voice=" << (settings.voice ? 1 : 0) << "\n";
    o << "quick=" << (settings.quickFetch ? 1 : 0) << "\n";
    o << "fullscreen=" << (settings.fullscreen ? 1 : 0) << "\n";
    o << "mouselook=" << (settings.mouseLook ? 1 : 0) << "\n";
    o << "returned=" << booksReturned << "\n";
    for (auto& kv : items)
        o << "item=" << kv.first << "," << kv.second.attempts << "," << kv.second.correct << "," << kv.second.last << "," << (kv.second.lastCorrect ? 1 : 0) << "\n";
    for (auto& kv : code)
        o << "code=" << kv.first << "," << kv.second.attempts << "," << kv.second.bestPassed << "," << kv.second.total << ","
          << (kv.second.solved ? 1 : 0) << "," << kv.second.last << "\n";
    for (auto& kv : chaptersRead) o << "chapter=" << kv.first << "," << kv.second << "\n";
    for (auto& s : sessions) {
        o << "[session]\n";
        o << "time=" << s.time << "\nmode=" << s.mode << "\ntopic=" << s.topic << "\ntitle=" << EscapeField(s.title)
          << "\nscore=" << s.score << "\ntotal=" << s.total << "\nseconds=" << s.seconds << "\nitems=";
        for (size_t i = 0; i < s.items.size(); i++) {
            if (i) o << ';';
            o << s.items[i].first << ':' << (s.items[i].second ? 1 : 0);
        }
        o << "\n";
    }
    WriteTextFile(SavePath("progress.txt"), o.str());
}

void Progress::Reset() {
    Settings keep = settings;
    *this = Progress();
    settings = keep;
    Save();
}

void Progress::RecordSession(const Session& s) {
    sessions.push_back(s);
    Save();
}

void Progress::RecordAnswer(const std::string& id, bool correct) {
    ItemStat& s = items[id];
    s.attempts++;
    if (correct) s.correct++;
    s.last = NowEpoch();
    s.lastCorrect = correct;
}

void Progress::RecordCode(const std::string& id, int passed, int total) {
    CodeStat& s = code[id];
    s.attempts++;
    s.total = total;
    s.bestPassed = std::max(s.bestPassed, passed);
    if (passed == total && total > 0) s.solved = true;
    s.last = NowEpoch();
    Save();
}

ItemStat Progress::Item(const std::string& id) const {
    auto it = items.find(id);
    return it == items.end() ? ItemStat{} : it->second;
}

CodeStat Progress::Code(const std::string& id) const {
    auto it = code.find(id);
    return it == code.end() ? CodeStat{} : it->second;
}

std::string Progress::LoadDraft(const std::string& problemId) const {
    bool ok = false;
    std::string s = ReadTextFile(SavePath("code/" + SafeName(problemId) + ".cpp"), &ok);
    return ok ? s : std::string();
}

void Progress::SaveDraft(const std::string& problemId, const std::string& text) const {
    WriteTextFile(SavePath("code/" + SafeName(problemId) + ".cpp"), text);
}

std::vector<TopicMastery> Progress::Mastery() const {
    const Content& c = Data();
    std::vector<TopicMastery> out;
    for (const Topic& t : c.topics) {
        TopicMastery m; m.topic = t.id;
        out.push_back(m);
    }
    auto find = [&](const std::string& topic) -> TopicMastery* {
        for (auto& m : out) if (m.topic == topic) return &m;
        return nullptr;
    };
    for (auto& kv : items) {
        TopicMastery* m = find(c.TopicOfItem(kv.first));
        if (!m) continue;
        // weight recent performance: last result counts double
        m->attempts += kv.second.attempts;
        m->correct += kv.second.correct;
        if (kv.second.lastCorrect) m->correct += 1;
        m->attempts += 1;
    }
    for (const Problem& p : c.problems) {
        TopicMastery* m = find(p.topic);
        if (!m) continue;
        auto it = code.find(p.id);
        if (it == code.end() || it->second.attempts == 0) continue;
        m->codeTotal++;
        if (it->second.solved) m->codeSolved++;
    }
    for (auto& m : out) {
        float qa = m.attempts > 0 ? (float)m.correct / m.attempts : -1.f;
        float ca = m.codeTotal > 0 ? (float)m.codeSolved / m.codeTotal : -1.f;
        if (qa >= 0 && ca >= 0) m.score = qa * 0.65f + ca * 0.35f;
        else if (qa >= 0) m.score = qa;
        else if (ca >= 0) m.score = ca;
        else m.score = -1.f;
    }
    return out;
}

int Progress::TotalAnswered() const {
    int n = 0;
    for (auto& kv : items) n += kv.second.attempts;
    return n;
}

float Progress::OverallAccuracy() const {
    int a = 0, c = 0;
    for (auto& kv : items) { a += kv.second.attempts; c += kv.second.correct; }
    return a ? (float)c / a : 0.f;
}
