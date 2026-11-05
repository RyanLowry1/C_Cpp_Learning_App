// content.cpp - loads all learning content from assets/content
#include "content.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
using namespace util;

static Content g_content;
Content& Data() { return g_content; }

// ---------------------------------------------------------------------------
// Generic record reader. A file is a list of records:
//   @kind id
//   key: single line value
//   key:            <- a key with nothing after the colon starts a block
//   ...verbatim lines...
//   @end
// Lines starting with '#' (outside blocks) are comments.
// ---------------------------------------------------------------------------
struct Rec {
    std::string kind, id, file;
    int line = 0;
    std::vector<std::pair<std::string, std::string>> kv;
    std::string Get(const std::string& k, const std::string& def = "") const {
        for (auto& p : kv) if (p.first == k) return p.second;
        return def;
    }
    std::vector<std::string> All(const std::string& k) const {
        std::vector<std::string> v;
        for (auto& p : kv) if (p.first == k) v.push_back(p.second);
        return v;
    }
};

static bool ParseKey(const std::string& line, std::string& key, std::string& val) {
    size_t c = line.find(':');
    if (c == std::string::npos || c == 0 || c > 24) return false;
    for (size_t i = 0; i < c; i++) {
        char ch = line[i];
        if (!(isalnum((unsigned char)ch) || ch == '_' || ch == '*' || ch == '-')) return false;
    }
    if (c + 1 < line.size() && line[c + 1] != ' ') return false;
    key = line.substr(0, c);
    val = c + 1 < line.size() ? Trim(line.substr(c + 1)) : std::string();
    return true;
}

static std::vector<Rec> ReadRecords(const std::string& path, std::vector<std::string>& errors) {
    std::vector<Rec> recs;
    bool ok = false;
    std::string text = ReadTextFile(path, &ok);
    if (!ok) { errors.push_back("Could not read " + path); return recs; }
    std::vector<std::string> lines = SplitLines(text);
    Rec* cur = nullptr;
    for (size_t i = 0; i < lines.size(); i++) {
        std::string raw = lines[i];
        std::string t = Trim(raw);
        if (t.empty() || t[0] == '#') continue;
        if (t[0] == '@' && t != "@end") {
            size_t sp = t.find(' ');
            Rec r;
            r.kind = t.substr(1, sp == std::string::npos ? std::string::npos : sp - 1);
            r.id = sp == std::string::npos ? "" : Trim(t.substr(sp + 1));
            r.file = path; r.line = (int)i + 1;
            recs.push_back(r);
            cur = &recs.back();
            continue;
        }
        if (!cur) continue;
        std::string key, val;
        if (ParseKey(t, key, val)) {
            if (val.empty()) {
                // block value until @end
                std::string block;
                size_t j = i + 1;
                for (; j < lines.size(); j++) {
                    if (Trim(lines[j]) == "@end") break;
                    block += lines[j];
                    block += '\n';
                }
                if (j >= lines.size()) errors.push_back(path + ":" + std::to_string(i + 1) + " block '" + key + "' has no @end");
                // trim trailing blank lines
                while (!block.empty() && (block.back() == '\n' || block.back() == ' ')) block.pop_back();
                cur->kv.push_back({ key, block });
                i = j;
            } else {
                cur->kv.push_back({ key, val });
            }
        } else {
            errors.push_back(path + ":" + std::to_string(i + 1) + " unrecognised line: " + t.substr(0, 40));
        }
    }
    return recs;
}

static Color ParseColor(const std::string& s, Color def) {
    auto p = Split(s, ',');
    if (p.size() < 3) return def;
    return Color{ (unsigned char)atoi(p[0].c_str()), (unsigned char)atoi(p[1].c_str()), (unsigned char)atoi(p[2].c_str()), 255 };
}

// ---------------------------------------------------------------------------
void Content::Load() {
    topics = {
        { "basics",     "C Basics",               "Types, printf & scanf, operators and casting",       "%d",   { 122, 32, 36, 255 },  1, 0, 0 },
        { "control",    "Decisions & Loops",      "if / else, switch, while, do-while and for",          "if",   { 34, 54, 104, 255 },  1, 0, 1 },
        { "arrays",     "Arrays & Strings",       "Indexing, 2D arrays, sizeof and the null terminator",  "[ ]",  { 30, 86, 52, 255 },   1, 1, 0 },
        { "functions",  "Functions & Parameters", "Pass by value versus pass by reference",               "f(x)", { 168, 118, 32, 255 }, 1, 1, 1 },
        { "pointers",   "Pointers",               "Addresses, * and &, arithmetic, double pointers",      "*p",   { 92, 40, 96, 255 },   2, 2, 0 },
        { "memory",     "Memory Management",      "Stack & heap, new/delete, malloc/free, leaks",         "new",  { 28, 96, 100, 255 },  2, 2, 1 },
        { "linkedlist", "Linked Lists",           "Nodes, insertion, deletion, doubly linked lists",      "->",   { 176, 78, 30, 255 },  2, 3, 0 },
        { "classes",    "Classes & Objects",      "Constructors, this, static, copy constructors",        "{ }",  { 54, 72, 110, 255 },  2, 3, 1 },
        { "operators",  "Overloading",            "Function & operator overloading, ++ and <<",           "+",    { 120, 30, 58, 255 },  3, 4, 0 },
        { "templates",  "Templates",              "Function templates and class templates",               "<T>",  { 94, 104, 36, 255 },  3, 4, 1 },
        { "vectors",    "STL Vector & Iterators", "vector, iterators, erase-remove, sorting",             "v[i]", { 48, 44, 120, 255 },  3, 5, 0 },
        { "listsmaps",  "STL List & Map",         "list, splice, map, pair, stack and queue",             "map",  { 110, 72, 42, 255 },  3, 5, 1 },
    };
    mcqs.clear(); written.clear(); chapters.clear(); problems.clear(); sets.clear(); loadErrors.clear(); itemTopic.clear();

    std::string root = AssetPath("content");
    LoadMcq(root + "/mcq.txt");
    LoadWritten(root + "/written.txt");
    std::error_code ec;
    if (fs::exists(root + "/reading", ec)) {
        std::vector<std::string> files;
        for (auto& e : fs::directory_iterator(root + "/reading", ec)) if (e.path().extension() == ".txt") files.push_back(e.path().string());
        std::sort(files.begin(), files.end());
        for (auto& f : files) LoadChapter(f);
    }
    if (fs::exists(root + "/coding", ec)) {
        std::vector<std::string> files;
        for (auto& e : fs::directory_iterator(root + "/coding", ec)) if (e.path().extension() == ".txt") files.push_back(e.path().string());
        std::sort(files.begin(), files.end());
        for (auto& f : files) LoadProblems(f);
    }
    for (auto& q : mcqs) itemTopic[q.id] = q.topic;
    for (auto& w : written) itemTopic[w.id] = w.topic;
    for (auto& p : problems) itemTopic[p.id] = p.topic;
    for (auto& e : loadErrors) TraceLog(LOG_WARNING, "CONTENT: %s", e.c_str());
    TraceLog(LOG_INFO, "CONTENT: %d MCQs, %d written, %d chapters, %d problems in %d sets",
             (int)mcqs.size(), (int)written.size(), (int)chapters.size(), (int)problems.size(), (int)sets.size());
}

void Content::LoadMcq(const std::string& path) {
    for (const Rec& r : ReadRecords(path, loadErrors)) {
        if (r.kind != "q") continue;
        MCQ q;
        q.id = r.id;
        q.topic = r.Get("topic");
        q.question = Unescape(r.Get("q"));
        q.code = r.Get("code");
        q.why = Unescape(r.Get("why"));
        q.diff = atoi(r.Get("diff", "1").c_str());
        q.verifyOutput = r.Get("verify") == "output";
        for (auto& p : r.kv) {
            if (p.first == "a") q.options.push_back(Unescape(p.second));
            else if (p.first == "*a") { q.correct = (int)q.options.size(); q.options.push_back(Unescape(p.second)); }
        }
        if (q.options.size() < 2) { loadErrors.push_back(path + ": question " + q.id + " has fewer than 2 options"); continue; }
        if (TopicIndex(q.topic) < 0) { loadErrors.push_back(path + ": question " + q.id + " has unknown topic " + q.topic); continue; }
        mcqs.push_back(q);
    }
}

void Content::LoadWritten(const std::string& path) {
    for (const Rec& r : ReadRecords(path, loadErrors)) {
        if (r.kind != "w") continue;
        Written w;
        w.id = r.id;
        w.topic = r.Get("topic");
        w.kind = r.Get("kind", "concept");
        w.question = Unescape(r.Get("q"));
        w.code = r.Get("code");
        w.why = Unescape(r.Get("why"));
        w.runnable = r.Get("run") == "yes";
        for (auto& a : r.All("ans")) w.answers.push_back(Unescape(a));
        w.multiline = r.Get("lines") == "multi";
        if (w.answers.empty()) { loadErrors.push_back(path + ": written " + w.id + " has no answers"); continue; }
        if (TopicIndex(w.topic) < 0) { loadErrors.push_back(path + ": written " + w.id + " has unknown topic " + w.topic); continue; }
        written.push_back(w);
    }
}

// Reading markup: see assets/content/README.txt
void Content::LoadChapter(const std::string& path) {
    bool ok = false;
    std::string text = ReadTextFile(path, &ok);
    if (!ok) { loadErrors.push_back("Could not read " + path); return; }
    Chapter ch;
    std::vector<std::string> lines = SplitLines(text);
    std::string para;
    auto flushPara = [&]() {
        std::string p = Trim(para);
        if (!p.empty()) ch.blocks.push_back(ReadBlock{ ReadBlock::Para, p, {} });
        para.clear();
    };
    for (size_t i = 0; i < lines.size(); i++) {
        std::string raw = lines[i];
        std::string t = Trim(raw);
        if (StartsWith(t, "```")) {
            flushPara();
            bool isOut = t.find("output") != std::string::npos;
            std::string code;
            size_t j = i + 1;
            for (; j < lines.size(); j++) {
                if (StartsWith(Trim(lines[j]), "```")) break;
                code += lines[j]; code += '\n';
            }
            while (!code.empty() && code.back() == '\n') code.pop_back();
            ch.blocks.push_back(ReadBlock{ isOut ? ReadBlock::Output : ReadBlock::Code, code, {} });
            i = j;
            continue;
        }
        if (t == "@mem") {
            flushPara();
            ReadBlock b{ ReadBlock::Mem, "", {} };
            size_t j = i + 1;
            for (; j < lines.size(); j++) {
                std::string l = Trim(lines[j]);
                if (l == "@end") break;
                if (l.empty()) continue;
                auto parts = Split(l, '|');
                MemCell c;
                if (parts.size() > 0) c.name = Trim(parts[0]);
                if (parts.size() > 1) c.type = Trim(parts[1]);
                if (parts.size() > 2) c.value = Trim(parts[2]);
                if (parts.size() > 3) c.addr = Trim(parts[3]);
                if (parts.size() > 4) c.pointsTo = Trim(parts[4]);
                b.mem.push_back(c);
            }
            if (j < lines.size() && j + 1 < lines.size() && StartsWith(Trim(lines[j + 1]), "caption:")) {
                b.text = Trim(Trim(lines[j + 1]).substr(8));
                j++;
            }
            ch.blocks.push_back(b);
            i = j;
            continue;
        }
        if (t.empty()) { flushPara(); continue; }
        if (StartsWith(t, "@title ")) { ch.title = Trim(t.substr(7)); continue; }
        if (StartsWith(t, "@topic ")) { ch.topic = Trim(t.substr(7)); continue; }
        if (StartsWith(t, "@subtitle ")) { ch.subtitle = Trim(t.substr(10)); continue; }
        if (t == "===") { flushPara(); ch.blocks.push_back(ReadBlock{ ReadBlock::Break, "", {} }); continue; }
        if (StartsWith(t, "## ")) { flushPara(); ch.blocks.push_back(ReadBlock{ ReadBlock::Sub, t.substr(3), {} }); continue; }
        if (StartsWith(t, "# ")) { flushPara(); ch.blocks.push_back(ReadBlock{ ReadBlock::Heading, t.substr(2), {} }); continue; }
        if (StartsWith(t, "- ")) { flushPara(); ch.blocks.push_back(ReadBlock{ ReadBlock::Bullet, t.substr(2), {} }); continue; }
        if (StartsWith(t, "!> ")) { flushPara(); ch.blocks.push_back(ReadBlock{ ReadBlock::Warn, t.substr(3), {} }); continue; }
        if (StartsWith(t, "*> ")) { flushPara(); ch.blocks.push_back(ReadBlock{ ReadBlock::Exam, t.substr(3), {} }); continue; }
        if (StartsWith(t, "> ")) { flushPara(); ch.blocks.push_back(ReadBlock{ ReadBlock::Tip, t.substr(2), {} }); continue; }
        if (!para.empty()) para += ' ';
        para += t;
    }
    flushPara();
    if (ch.topic.empty()) {
        std::string stem = fs::path(path).stem().string();
        size_t us = stem.find('_');
        ch.topic = us == std::string::npos ? stem : stem.substr(us + 1);
    }
    if (TopicIndex(ch.topic) < 0) { loadErrors.push_back(path + ": unknown topic " + ch.topic); return; }
    chapters.push_back(ch);
}

void Content::LoadProblems(const std::string& path) {
    int setIdx = -1;
    for (const Rec& r : ReadRecords(path, loadErrors)) {
        if (r.kind == "set") {
            ProblemSet s;
            s.id = r.id;
            s.title = r.Get("title", r.id);
            s.subtitle = r.Get("subtitle");
            s.desc = Unescape(r.Get("desc"));
            s.prelude = r.Get("prelude", "cpp");
            s.examNo = atoi(r.Get("exam", "0").c_str());
            s.color = ParseColor(r.Get("color"), s.color);
            sets.push_back(s);
            setIdx = (int)sets.size() - 1;
            continue;
        }
        if (r.kind != "p") continue;
        if (setIdx < 0) { loadErrors.push_back(path + ": problem before any @set"); continue; }
        Problem p;
        p.id = r.id;
        p.set = sets[setIdx].id;
        p.title = r.Get("title", r.id);
        p.topic = r.Get("topic");
        p.diff = r.Get("diff", "Easy");
        p.desc = r.Get("desc");
        p.example = r.Get("example");
        p.starter = r.Get("starter");
        p.support = r.Get("support");
        p.helpers = r.Get("helpers");
        p.solution = r.Get("solution");
        p.hint = Unescape(r.Get("hint"));
        p.prelude = r.Get("prelude", sets[setIdx].prelude);
        for (auto& f : r.All("forbid")) {
            size_t semi = f.find(';');
            ForbidRule fr;
            std::string toks = semi == std::string::npos ? f : f.substr(0, semi);
            fr.message = semi == std::string::npos ? "This exercise forbids: " + toks : Trim(f.substr(semi + 1));
            for (auto& t : Split(toks, '|')) { std::string tt = Trim(t); if (!tt.empty()) fr.tokens.push_back(tt); }
            p.forbid.push_back(fr);
        }
        // tests
        std::string tests = r.Get("tests");
        TestCase* cur = nullptr;
        for (const std::string& line : SplitLines(tests)) {
            std::string t = Trim(line);
            if (t.empty()) continue;
            if (StartsWith(t, "+ ") || StartsWith(t, "- ")) {
                TestCase tc;
                tc.sample = t[0] == '+';
                tc.label = Trim(t.substr(2));
                p.tests.push_back(tc);
                cur = &p.tests.back();
                continue;
            }
            if (!cur) { loadErrors.push_back(path + ": test code before any test label in " + p.id); continue; }
            cur->code += line;
            cur->code += '\n';
        }
        if (p.tests.empty()) loadErrors.push_back(path + ": problem " + p.id + " has no tests");
        problems.push_back(p);
        sets[setIdx].problems.push_back((int)problems.size() - 1);
    }
}

int Content::TopicIndex(const std::string& id) const {
    for (int i = 0; i < (int)topics.size(); i++) if (topics[i].id == id) return i;
    return -1;
}
const Topic* Content::FindTopic(const std::string& id) const {
    int i = TopicIndex(id);
    return i < 0 ? nullptr : &topics[i];
}
const Chapter* Content::ChapterFor(const std::string& topic) const {
    for (auto& c : chapters) if (c.topic == topic) return &c;
    return nullptr;
}
int Content::ProblemIndex(const std::string& id) const {
    for (int i = 0; i < (int)problems.size(); i++) if (problems[i].id == id) return i;
    return -1;
}
std::string Content::TopicOfItem(const std::string& itemId) const {
    auto it = itemTopic.find(itemId);
    return it == itemTopic.end() ? std::string() : it->second;
}
std::vector<int> Content::McqsFor(const std::string& topic) const {
    std::vector<int> v;
    for (int i = 0; i < (int)mcqs.size(); i++) if (mcqs[i].topic == topic) v.push_back(i);
    return v;
}
std::vector<int> Content::WrittenFor(const std::string& topic) const {
    std::vector<int> v;
    for (int i = 0; i < (int)written.size(); i++) if (written[i].topic == topic) v.push_back(i);
    return v;
}
