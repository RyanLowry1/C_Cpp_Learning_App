// syntax.cpp - tiny C/C++ lexer for colouring code on parchment
#include "syntax.h"
#include "ui.h"
#include <unordered_set>
#include <algorithm>
#include <cstring>

namespace syntax {

static const std::unordered_set<std::string>& Keywords() {
    static const std::unordered_set<std::string> k = {
        "auto", "break", "case", "catch", "class", "const", "constexpr", "continue", "default", "delete", "do", "else",
        "enum", "explicit", "extern", "false", "for", "friend", "goto", "if", "inline", "mutable", "namespace", "new",
        "nullptr", "operator", "private", "protected", "public", "register", "return", "sizeof", "static", "struct",
        "switch", "template", "this", "throw", "true", "try", "typedef", "typename", "union", "using", "virtual",
        "volatile", "while", "NULL", "override", "noexcept", "static_cast", "const_cast", "dynamic_cast", "reinterpret_cast" };
    return k;
}
static const std::unordered_set<std::string>& Types() {
    static const std::unordered_set<std::string> t = {
        "int", "char", "float", "double", "void", "bool", "long", "short", "unsigned", "signed", "size_t", "string",
        "vector", "list", "map", "pair", "stack", "queue", "deque", "set", "unordered_map", "iterator", "Node", "FILE",
        "ostream", "istream", "std", "wchar_t", "uint8_t", "int64_t", "T", "Point", "Vector", "Box", "Clock",
        "LinkedList", "DoublyLinkedList", "DoublyLinkedNode", "Adder", "Particle", "MyClass", "Landmark", "SomeClass",
        "MITStudent", "TwentyThousandPoints", "Maths" };
    return t;
}

void Classify(const std::string& s, bool& inBlock, std::vector<unsigned char>& k) {
    k.assign(s.size(), Plain);
    size_t i = 0;
    // preprocessor line
    size_t fs = s.find_first_not_of(" \t");
    bool pre = fs != std::string::npos && s[fs] == '#' && !inBlock;
    while (i < s.size()) {
        if (inBlock) {
            size_t e = s.find("*/", i);
            size_t end = e == std::string::npos ? s.size() : e + 2;
            for (size_t j = i; j < end; j++) k[j] = Comment;
            i = end;
            if (e != std::string::npos) inBlock = false;
            continue;
        }
        char c = s[i];
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') { for (size_t j = i; j < s.size(); j++) k[j] = Comment; break; }
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '*') { inBlock = true; k[i] = k[i + 1] = Comment; i += 2; continue; }
        if (c == '"' || c == '\'' || (pre && c == '<' && s.find("include") != std::string::npos)) {
            char q = c == '<' ? '>' : c;
            size_t j = i + 1;
            while (j < s.size() && s[j] != q) { if (s[j] == '\\') j++; j++; }
            size_t end = std::min(s.size(), j + 1);
            for (size_t m = i; m < end; m++) k[m] = String;
            i = end;
            continue;
        }
        if (isdigit((unsigned char)c) || (c == '.' && i + 1 < s.size() && isdigit((unsigned char)s[i + 1]))) {
            size_t j = i;
            while (j < s.size() && (isalnum((unsigned char)s[j]) || s[j] == '.')) j++;
            for (size_t m = i; m < j; m++) k[m] = Number;
            i = j;
            continue;
        }
        if (isalpha((unsigned char)c) || c == '_') {
            size_t j = i;
            while (j < s.size() && (isalnum((unsigned char)s[j]) || s[j] == '_')) j++;
            std::string w = s.substr(i, j - i);
            unsigned char kind = Plain;
            if (pre && (w == "include" || w == "define" || w == "ifdef" || w == "ifndef" || w == "endif" || w == "pragma")) kind = Preproc;
            else if (Keywords().count(w)) kind = Keyword;
            else if (Types().count(w)) kind = Type;
            else {
                size_t n = j;
                while (n < s.size() && s[n] == ' ') n++;
                if (n < s.size() && s[n] == '(') kind = Function;
            }
            for (size_t m = i; m < j; m++) k[m] = kind;
            i = j;
            continue;
        }
        if (c == '#' && pre) { k[i] = Preproc; i++; continue; }
        if (strchr("{}()[];,<>=+-*/&|!%^~?:.", c)) k[i] = Punct;
        i++;
    }
}

Color ColorFor(unsigned char k) {
    switch (k) {
        case Keyword: return Color{ 150, 28, 60, 255 };
        case Type: return Color{ 30, 66, 140, 255 };
        case Number: return Color{ 120, 52, 150, 255 };
        case String: return Color{ 36, 116, 52, 255 };
        case Comment: return Color{ 136, 124, 104, 255 };
        case Preproc: return Color{ 160, 96, 30, 255 };
        case Function: return Color{ 20, 96, 108, 255 };
        case Punct: return Color{ 90, 70, 60, 255 };
        default: return Color{ 44, 34, 28, 255 };
    }
}

static void DrawHighlightedLine(const std::string& line, const std::vector<unsigned char>& kinds, float x, float y, float size, float cw) {
    size_t i = 0;
    while (i < line.size()) {
        size_t j = i;
        while (j < line.size() && kinds[j] == kinds[i]) j++;
        std::string run = line.substr(i, j - i);
        bool bold = kinds[i] == Keyword;
        ui::Text(bold ? FontId::MonoBold : FontId::Mono, run, x + cw * (float)i, y, size, ColorFor(kinds[i]));
        i = j;
    }
}

// The bitmap font cannot be shrunk to fit, so lines that are too long for the page are wrapped instead
// (continuation rows are indented two spaces further than the line they continue).
static std::vector<std::pair<std::string, int>> WrapRows(const std::vector<std::string>& lines, size_t maxChars) {
    std::vector<std::pair<std::string, int>> rows;
    for (size_t i = 0; i < lines.size(); i++) {
        std::string l = lines[i];
        if (l.size() <= maxChars) { rows.push_back({ l, (int)i + 1 }); continue; }
        size_t indent = 0;
        while (indent < l.size() && l[indent] == ' ') indent++;
        indent = std::min(indent, maxChars / 2);
        bool first = true;
        size_t pos = 0;
        while (pos < l.size()) {
            size_t room = first ? maxChars : maxChars - indent - 2;
            std::string chunk = l.substr(pos, room);
            if (pos + room < l.size()) {
                size_t sp = chunk.find_last_of(" ,;)");
                if (sp != std::string::npos && sp > room / 2) chunk = chunk.substr(0, sp + 1);
            }
            pos += chunk.size();
            rows.push_back({ first ? chunk : std::string(indent + 2, ' ') + chunk, first ? (int)i + 1 : 0 });
            first = false;
        }
    }
    return rows;
}

float DrawCode(const std::string& code, float x, float y, float width, float size, bool draw, bool lineNumbers, float minSize) {
    (void)minSize;
    std::vector<std::string> lines = util::SplitLines(code);
    for (auto& l : lines) l = util::ReplaceAll(l, "\t", "    ");
    float pad = 14.f;
    float gutter = lineNumbers ? 34.f : 0.f;
    float avail = width - pad * 2 - gutter;
    float fs = size;
    float cw = ui::MonoCharWidth(fs);
    size_t maxChars = (size_t)std::max(8.f, floorf(avail / cw + 0.001f));
    std::vector<std::pair<std::string, int>> rows = WrapRows(lines, maxChars);
    float lh = fs * 1.34f;
    float h = rows.size() * lh + pad * 1.4f;
    if (!draw) return h;
    Rectangle box{ x, y, width, h };
    DrawRectangleRounded(box, 0.04f, 6, Color{ 250, 245, 232, 255 });
    DrawRectangleRoundedLinesEx(box, 0.04f, 6, 1.5f, Color{ 196, 178, 146, 255 });
    DrawRectangle((int)x + 3, (int)y + 6, 3, (int)h - 12, Color{ 190, 150, 90, 160 });
    if (lineNumbers) DrawRectangle((int)(x + pad + gutter - 8), (int)y + 4, 1, (int)h - 8, Color{ 210, 196, 170, 255 });
    bool inBlock = false;
    std::vector<unsigned char> kinds;
    ui::PushClip(Rectangle{ x + 2, y + 2, width - 4, h - 4 });
    for (size_t i = 0; i < rows.size(); i++) {
        float ly = y + pad * 0.7f + i * lh;
        if (lineNumbers && rows[i].second > 0) ui::TextRight(FontId::Mono, std::to_string(rows[i].second), x + pad + gutter - 14, ly, fs * 0.85f, Color{ 170, 150, 120, 255 });
        Classify(rows[i].first, inBlock, kinds);
        DrawHighlightedLine(rows[i].first, kinds, x + pad + gutter, ly, fs, cw);
    }
    ui::PopClip();
    return h;
}

float DrawOutput(const std::string& text, float x, float y, float width, float size, bool draw) {
    std::vector<std::string> lines = util::SplitLines(text);
    float fs = size;
    float cw = ui::MonoCharWidth(fs);
    size_t maxChars = (size_t)std::max(8.f, floorf((width - 28) / cw + 0.001f));
    std::vector<std::pair<std::string, int>> rows = WrapRows(lines, maxChars);
    float lh = fs * 1.3f;
    float h = rows.size() * lh + 34.f;
    if (!draw) return h;
    Rectangle box{ x, y, width, h };
    DrawRectangleRounded(box, 0.05f, 6, Color{ 44, 40, 36, 255 });
    ui::Text(FontId::SerifItalic, "output", x + width - 60, y + 4, 13, Color{ 160, 150, 130, 255 });
    for (size_t i = 0; i < rows.size(); i++)
        ui::Text(FontId::Mono, rows[i].first, x + 14, y + 20 + i * lh, fs, Color{ 226, 222, 200, 255 });
    return h;
}

} // namespace syntax
