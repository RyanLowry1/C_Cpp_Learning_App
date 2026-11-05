// code_editor.cpp - editing, selection, undo, clipboard and rendering
#include "code_editor.h"
#include "syntax.h"
#include "ui.h"
#include <algorithm>

using namespace ui;

static std::string Sanitise(const std::string& in) {
    std::string o;
    for (char c : in) {
        if (c == '\r') continue;
        if (c == '\t') { o += "    "; continue; }
        if ((unsigned char)c < 32 && c != '\n') continue;
        o += c;
    }
    return o;
}

void CodeEditor::SetText(const std::string& s) {
    lines = util::SplitLines(Sanitise(s));
    if (lines.empty()) lines.push_back("");
    cx = cy = 0; hasAnchor = false;
    scrollX = scrollY = 0;
    undo.clear(); redo.clear();
}

std::string CodeEditor::GetText() const {
    std::string s;
    for (size_t i = 0; i < lines.size(); i++) { if (i) s += '\n'; s += lines[i]; }
    return s;
}

void CodeEditor::GotoLine(int line) {
    cy = std::max(0, std::min((int)lines.size() - 1, line - 1));
    cx = (int)lines[(size_t)cy].size();
    hasAnchor = false;
    focused = true;
    wantVisible = true;
}

void CodeEditor::PushUndo(int kind) {
    if (kind == 1 && lastEditKind == 1 && !undo.empty()) return;   // merge consecutive typing
    undo.push_back(Snap{ lines, cx, cy });
    if (undo.size() > 200) undo.erase(undo.begin());
    redo.clear();
    lastEditKind = kind;
}

bool CodeEditor::HasSel() const { return hasAnchor && (ax != cx || ay != cy); }

void CodeEditor::SelRange(int& x0, int& y0, int& x1, int& y1) const {
    if (ay < cy || (ay == cy && ax < cx)) { x0 = ax; y0 = ay; x1 = cx; y1 = cy; }
    else { x0 = cx; y0 = cy; x1 = ax; y1 = ay; }
}

std::string CodeEditor::SelText() const {
    if (!HasSel()) return std::string();
    int x0, y0, x1, y1;
    SelRange(x0, y0, x1, y1);
    if (y0 == y1) return lines[(size_t)y0].substr((size_t)x0, (size_t)(x1 - x0));
    std::string s = lines[(size_t)y0].substr((size_t)x0) + "\n";
    for (int y = y0 + 1; y < y1; y++) s += lines[(size_t)y] + "\n";
    s += lines[(size_t)y1].substr(0, (size_t)x1);
    return s;
}

void CodeEditor::DeleteSel() {
    if (!HasSel()) { hasAnchor = false; return; }
    int x0, y0, x1, y1;
    SelRange(x0, y0, x1, y1);
    std::string head = lines[(size_t)y0].substr(0, (size_t)x0);
    std::string tail = lines[(size_t)y1].substr((size_t)x1);
    lines.erase(lines.begin() + y0, lines.begin() + y1 + 1);
    lines.insert(lines.begin() + y0, head + tail);
    cx = x0; cy = y0;
    hasAnchor = false;
    changed = true;
}

void CodeEditor::InsertText(const std::string& raw) {
    std::string s = Sanitise(raw);
    if (singleLine) { std::replace(s.begin(), s.end(), '\n', ' '); }
    if (HasSel()) DeleteSel();
    hasAnchor = false;
    std::vector<std::string> parts = util::SplitLines(s);
    std::string& line = lines[(size_t)cy];
    std::string tail = line.substr((size_t)cx);
    line = line.substr(0, (size_t)cx) + parts[0];
    if (parts.size() == 1) {
        cx = (int)line.size();
        line += tail;
    } else {
        for (size_t i = 1; i < parts.size(); i++) lines.insert(lines.begin() + cy + (int)i, parts[i]);
        cy += (int)parts.size() - 1;
        cx = (int)lines[(size_t)cy].size();
        lines[(size_t)cy] += tail;
    }
    changed = true;
    wantVisible = true;
}

int CodeEditor::IndentOf(int line) const {
    const std::string& l = lines[(size_t)line];
    int n = 0;
    while (n < (int)l.size() && l[(size_t)n] == ' ') n++;
    return n;
}

void CodeEditor::InsertChar(char c) {
    PushUndo(isalnum((unsigned char)c) ? 1 : 0);
    if (HasSel()) DeleteSel();
    std::string& line = lines[(size_t)cy];
    // typing '}' on an otherwise blank line dedents it
    if (c == '}' && !plain) {
        bool blank = true;
        for (int i = 0; i < cx; i++) if (line[(size_t)i] != ' ') blank = false;
        if (blank && cx >= 4) { line.erase(0, 4); cx -= 4; }
    }
    line.insert(line.begin() + cx, c);
    cx++;
    changed = true;
    wantVisible = true;
}

void CodeEditor::Newline() {
    if (singleLine) { submitted = true; return; }
    PushUndo();
    if (HasSel()) DeleteSel();
    std::string& line = lines[(size_t)cy];
    int indent = plain ? 0 : IndentOf(cy);
    indent = std::min(indent, cx);
    bool openBrace = !plain && cx > 0 && line[(size_t)cx - 1] == '{';
    bool closeNext = !plain && cx < (int)line.size() && line[(size_t)cx] == '}';
    std::string tail = line.substr((size_t)cx);
    line = line.substr(0, (size_t)cx);
    // drop trailing spaces on the line we leave
    while (!line.empty() && line.back() == ' ' && (int)line.size() > indent) line.pop_back();
    size_t tl = 0;
    while (tl < tail.size() && tail[tl] == ' ') tl++;
    tail = tail.substr(tl);
    std::string pad((size_t)(indent + (openBrace ? 4 : 0)), ' ');
    if (openBrace && closeNext) {
        lines.insert(lines.begin() + cy + 1, pad);
        lines.insert(lines.begin() + cy + 2, std::string((size_t)indent, ' ') + tail);
        cy++;
        cx = (int)pad.size();
    } else {
        lines.insert(lines.begin() + cy + 1, pad + tail);
        cy++;
        cx = (int)pad.size();
    }
    changed = true;
    wantVisible = true;
}

void CodeEditor::Backspace() {
    PushUndo();
    if (HasSel()) { DeleteSel(); return; }
    hasAnchor = false;
    std::string& line = lines[(size_t)cy];
    if (cx > 0) {
        bool allSpace = true;
        for (int i = 0; i < cx; i++) if (line[(size_t)i] != ' ') allSpace = false;
        int n = 1;
        if (allSpace && !plain) { n = cx % 4 == 0 ? 4 : cx % 4; n = std::min(n, cx); }
        line.erase((size_t)(cx - n), (size_t)n);
        cx -= n;
    } else if (cy > 0) {
        cx = (int)lines[(size_t)cy - 1].size();
        lines[(size_t)cy - 1] += line;
        lines.erase(lines.begin() + cy);
        cy--;
    }
    changed = true;
    wantVisible = true;
}

void CodeEditor::DeleteForward() {
    PushUndo();
    if (HasSel()) { DeleteSel(); return; }
    std::string& line = lines[(size_t)cy];
    if (cx < (int)line.size()) line.erase((size_t)cx, 1);
    else if (cy + 1 < (int)lines.size()) { line += lines[(size_t)cy + 1]; lines.erase(lines.begin() + cy + 1); }
    changed = true;
}

void CodeEditor::MoveTo(int x, int y, bool select) {
    if (select && !hasAnchor) { hasAnchor = true; ax = cx; ay = cy; }
    if (!select) hasAnchor = false;
    cy = std::max(0, std::min((int)lines.size() - 1, y));
    cx = std::max(0, std::min((int)lines[(size_t)cy].size(), x));
    wantVisible = true;
    blink = 0;
}

void CodeEditor::Clamp() {
    if (lines.empty()) lines.push_back("");
    cy = std::max(0, std::min((int)lines.size() - 1, cy));
    cx = std::max(0, std::min((int)lines[(size_t)cy].size(), cx));
}

void CodeEditor::ToggleComment() {
    PushUndo();
    int y0 = cy, y1 = cy;
    if (HasSel()) { int a, b; SelRange(a, y0, b, y1); }
    bool allCommented = true;
    for (int y = y0; y <= y1; y++) {
        std::string t = util::Trim(lines[(size_t)y]);
        if (!t.empty() && !util::StartsWith(t, "//")) allCommented = false;
    }
    for (int y = y0; y <= y1; y++) {
        std::string& l = lines[(size_t)y];
        int ind = IndentOf(y);
        if (allCommented) {
            size_t p = l.find("//");
            if (p != std::string::npos) { l.erase(p, (p + 2 < l.size() && l[p + 2] == ' ') ? 3 : 2); if (y == cy) cx = std::max(ind, cx - 3); }
        } else if (!util::Trim(l).empty()) {
            l.insert((size_t)ind, "// ");
            if (y == cy) cx += 3;
        }
    }
    Clamp();
    changed = true;
}

void CodeEditor::IndentSel(bool out) {
    PushUndo();
    int y0 = cy, y1 = cy, x0 = 0, x1 = 0;
    if (HasSel()) SelRange(x0, y0, x1, y1);
    for (int y = y0; y <= y1; y++) {
        std::string& l = lines[(size_t)y];
        if (out) { int n = std::min(4, IndentOf(y)); l.erase(0, (size_t)n); if (y == cy) cx = std::max(0, cx - n); if (hasAnchor && y == ay) ax = std::max(0, ax - n); }
        else { l.insert(0, "    "); if (y == cy) cx += 4; if (hasAnchor && y == ay) ax += 4; }
    }
    Clamp();
    changed = true;
}

static bool IsWordChar(char c) { return isalnum((unsigned char)c) || c == '_'; }

void CodeEditor::HandleKeys() {
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    auto K = [](int k) { return IsKeyPressed(k) || IsKeyPressedRepeat(k); };

    if (!readOnly) {
        int ch;
        while ((ch = GetCharPressed()) > 0) {
            if (ctrl) continue;
            if (ch >= 32 && ch < 127) InsertChar((char)ch);
        }
    }
    if (ctrl && IsKeyPressed(KEY_A)) { hasAnchor = true; ay = 0; ax = 0; cy = (int)lines.size() - 1; cx = (int)lines.back().size(); }
    if (ctrl && IsKeyPressed(KEY_C) && HasSel()) SetClipboardText(SelText().c_str());
    if (ctrl && IsKeyPressed(KEY_X) && HasSel() && !readOnly) { SetClipboardText(SelText().c_str()); PushUndo(); DeleteSel(); }
    if (ctrl && IsKeyPressed(KEY_V) && !readOnly) {
        const char* clip = GetClipboardText();
        if (clip && *clip) { PushUndo(); InsertText(clip); }
    }
    if (ctrl && IsKeyPressed(KEY_Z) && !shift && !undo.empty() && !readOnly) {
        redo.push_back(Snap{ lines, cx, cy });
        Snap s = undo.back(); undo.pop_back();
        lines = s.lines; cx = s.cx; cy = s.cy; hasAnchor = false; Clamp(); changed = true; lastEditKind = 0;
    }
    if (((ctrl && IsKeyPressed(KEY_Y)) || (ctrl && shift && IsKeyPressed(KEY_Z))) && !redo.empty() && !readOnly) {
        undo.push_back(Snap{ lines, cx, cy });
        Snap s = redo.back(); redo.pop_back();
        lines = s.lines; cx = s.cx; cy = s.cy; hasAnchor = false; Clamp(); changed = true; lastEditKind = 0;
    }
    if (ctrl && IsKeyPressed(KEY_SLASH) && !readOnly && !plain) ToggleComment();
    if (ctrl && IsKeyPressed(KEY_D) && !readOnly && !singleLine) {
        PushUndo();
        lines.insert(lines.begin() + cy + 1, lines[(size_t)cy]);
        cy++; changed = true;
    }
    if (!readOnly) {
        if (K(KEY_ENTER) || K(KEY_KP_ENTER)) { if (!(ctrl && !singleLine)) Newline(); else if (singleLine) submitted = true; }
        if (K(KEY_BACKSPACE)) {
            if (ctrl && !HasSel()) {   // delete previous word
                int x = cx; std::string& l = lines[(size_t)cy];
                while (x > 0 && l[(size_t)x - 1] == ' ') x--;
                while (x > 0 && IsWordChar(l[(size_t)x - 1])) x--;
                if (x == cx && x > 0) x--;
                PushUndo(); hasAnchor = true; ax = x; ay = cy; DeleteSel();
            } else Backspace();
        }
        if (K(KEY_DELETE)) DeleteForward();
        if (K(KEY_TAB)) {
            if (HasSel() || shift) IndentSel(shift);
            else { PushUndo(); int n = 4 - cx % 4; InsertText(std::string((size_t)n, ' ')); }
        }
    }
    lastEditKind = (IsKeyPressed(KEY_SPACE) || K(KEY_ENTER)) ? 0 : lastEditKind;

    // navigation
    if (K(KEY_LEFT)) {
        if (HasSel() && !shift) { int x0, y0, x1, y1; SelRange(x0, y0, x1, y1); MoveTo(x0, y0, false); }
        else if (ctrl) {
            int x = cx, y = cy;
            if (x == 0 && y > 0) { y--; x = (int)lines[(size_t)y].size(); }
            else {
                const std::string& l = lines[(size_t)y];
                while (x > 0 && l[(size_t)x - 1] == ' ') x--;
                if (x > 0 && IsWordChar(l[(size_t)x - 1])) { while (x > 0 && IsWordChar(l[(size_t)x - 1])) x--; }
                else if (x > 0) x--;
            }
            MoveTo(x, y, shift);
        } else if (cx > 0) MoveTo(cx - 1, cy, shift);
        else if (cy > 0) MoveTo((int)lines[(size_t)cy - 1].size(), cy - 1, shift);
    }
    if (K(KEY_RIGHT)) {
        if (HasSel() && !shift) { int x0, y0, x1, y1; SelRange(x0, y0, x1, y1); MoveTo(x1, y1, false); }
        else if (ctrl) {
            int x = cx, y = cy;
            const std::string& l = lines[(size_t)y];
            if (x >= (int)l.size() && y + 1 < (int)lines.size()) { y++; x = 0; }
            else {
                if (x < (int)l.size() && IsWordChar(l[(size_t)x])) { while (x < (int)l.size() && IsWordChar(l[(size_t)x])) x++; }
                else if (x < (int)l.size()) x++;
                while (x < (int)l.size() && l[(size_t)x] == ' ') x++;
            }
            MoveTo(x, y, shift);
        } else if (cx < (int)lines[(size_t)cy].size()) MoveTo(cx + 1, cy, shift);
        else if (cy + 1 < (int)lines.size()) MoveTo(0, cy + 1, shift);
    }
    if (K(KEY_UP) && !singleLine) MoveTo(cx, cy - 1, shift);
    if (K(KEY_DOWN) && !singleLine) MoveTo(cx, cy + 1, shift);
    if (K(KEY_HOME)) {
        int ind = IndentOf(cy);
        if (ctrl) MoveTo(0, 0, shift); else MoveTo(cx == ind ? 0 : ind, cy, shift);
    }
    if (K(KEY_END)) {
        if (ctrl) MoveTo((int)lines.back().size(), (int)lines.size() - 1, shift);
        else MoveTo((int)lines[(size_t)cy].size(), cy, shift);
    }
    int page = std::max(1, (int)(textArea.height / lh) - 2);
    if (K(KEY_PAGE_UP)) MoveTo(cx, cy - page, shift);
    if (K(KEY_PAGE_DOWN)) MoveTo(cx, cy + page, shift);
}

Vector2 CodeEditor::PosToCell(Vector2 m) const {
    int y = (int)floorf((m.y - textArea.y + scrollY) / lh);
    y = std::max(0, std::min((int)lines.size() - 1, y));
    int x = (int)floorf((m.x - textArea.x + scrollX) / cw + 0.5f);
    x = std::max(0, std::min((int)lines[(size_t)y].size(), x));
    return Vector2{ (float)x, (float)y };
}

void CodeEditor::HandleMouse(bool interactive) {
    if (!interactive) { dragging = false; return; }
    Vector2 m = Mouse();
    bool inside = Hover(textArea);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (inside) {
            focused = true;
            ConsumePress();
            Vector2 c = PosToCell(m);
            double now = GetTime();
            clicks = (now - lastClick < 0.35) ? clicks + 1 : 1;
            lastClick = now;
            bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            if (clicks == 2) {
                const std::string& l = lines[(size_t)c.y];
                int a = (int)c.x, b = (int)c.x;
                while (a > 0 && IsWordChar(l[(size_t)a - 1])) a--;
                while (b < (int)l.size() && IsWordChar(l[(size_t)b])) b++;
                hasAnchor = true; ax = a; ay = (int)c.y; cx = b; cy = (int)c.y;
            } else if (clicks >= 3) {
                hasAnchor = true; ax = 0; ay = (int)c.y; cx = (int)lines[(size_t)c.y].size(); cy = (int)c.y;
            } else {
                MoveTo((int)c.x, (int)c.y, shift);
                if (!shift) { hasAnchor = true; ax = cx; ay = cy; }
                dragging = true;
            }
            blink = 0;
        } else if (!Hover(Rectangle{ textArea.x - 60, textArea.y - 10, textArea.width + 70, textArea.height + 20 })) {
            focused = false;
        }
    }
    if (dragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 c = PosToCell(m);
            cx = (int)c.x; cy = (int)c.y;
            if (m.y < textArea.y) scrollY -= lh * 0.5f;
            if (m.y > textArea.y + textArea.height) scrollY += lh * 0.5f;
        } else dragging = false;
    }
    if (inside) {
        WantHand();
        float w = Wheel();
        if (w != 0) { scrollY -= w * lh * 3.f; ConsumeWheel(); }
    }
}

void CodeEditor::Draw(Rectangle r, bool interactive, float fontSize) {
    cw = MonoCharWidth(fontSize);
    lh = fontSize * 1.38f;
    float gutter = plain ? 10.f : cw * 3.f + 22.f;
    textArea = Rectangle{ r.x + gutter, r.y + 8, r.width - gutter - 14, r.height - 16 };
    if (singleLine) textArea = Rectangle{ r.x + 12, r.y + (r.height - lh) * 0.5f, r.width - 24, lh };

    HandleMouse(interactive);
    if (focused && interactive) { CaptureKeyboard(); HandleKeys(); }
    Clamp();
    blink += GetFrameTime();

    // keep the cursor visible
    if (wantVisible) {
        float cyPix = cy * lh;
        if (cyPix < scrollY) scrollY = cyPix;
        if (cyPix + lh > scrollY + textArea.height) scrollY = cyPix + lh - textArea.height;
        float cxPix = cx * cw;
        if (cxPix < scrollX) scrollX = std::max(0.f, cxPix - cw * 4);
        if (cxPix > scrollX + textArea.width - cw * 2) scrollX = cxPix - textArea.width + cw * 6;
        wantVisible = false;
    }
    float maxScroll = std::max(0.f, lines.size() * lh - textArea.height + lh * 0.5f);
    scrollY = std::max(0.f, std::min(scrollY, maxScroll));
    scrollX = std::max(0.f, scrollX);

    // frame
    Color bg = plain ? Color{ 252, 249, 240, 255 } : Color{ 251, 247, 236, 255 };
    DrawRectangleRounded(r, plain ? 0.18f : 0.02f, 6, bg);
    DrawRectangleRoundedLinesEx(r, plain ? 0.18f : 0.02f, 6, focused ? 2.5f : 1.5f, focused ? pal::Blue : Color{ 190, 172, 140, 255 });
    if (!plain) {
        DrawRectangle((int)r.x + 1, (int)r.y + 1, (int)gutter - 6, (int)r.height - 2, Color{ 240, 232, 214, 255 });
        DrawLine((int)(r.x + gutter - 6), (int)r.y + 1, (int)(r.x + gutter - 6), (int)(r.y + r.height - 1), Color{ 214, 198, 170, 255 });
    }

    int first = (int)(scrollY / lh);
    int last = std::min((int)lines.size() - 1, (int)((scrollY + textArea.height) / lh) + 1);
    // block comment state up to the first visible line
    bool inBlock = false;
    std::vector<unsigned char> kinds;
    for (int i = 0; i < first; i++) syntax::Classify(lines[(size_t)i], inBlock, kinds);

    PushClip(Rectangle{ r.x + 1, r.y + 1, r.width - 2, r.height - 2 });
    for (int i = first; i <= last; i++) {
        float y = textArea.y + i * lh - scrollY;
        // current line
        if (focused && i == cy && !HasSel() && !singleLine)
            DrawRectangle((int)textArea.x - 4, (int)y, (int)textArea.width + 8, (int)lh, Color{ 236, 226, 196, 120 });
        // markers
        for (const Marker& mk : markers) {
            if (mk.line - 1 != i) continue;
            Color mc = mk.error ? pal::Red : pal::Amber;
            DrawRectangle((int)textArea.x - 4, (int)y, (int)textArea.width + 8, (int)lh, util::WithAlpha(mc, 0.10f));
            if (!plain) DrawCircleV(Vector2{ r.x + 9, y + lh * 0.5f }, 4.5f, mc);
            float w = std::max(3.f, (float)lines[(size_t)i].size()) * cw;
            for (float sx = 0; sx < w; sx += 6) {
                float x0 = textArea.x - scrollX + sx;
                DrawLineEx(Vector2{ x0, y + lh - 3 }, Vector2{ x0 + 3, y + lh - 1 }, 1.2f, mc);
                DrawLineEx(Vector2{ x0 + 3, y + lh - 1 }, Vector2{ x0 + 6, y + lh - 3 }, 1.2f, mc);
            }
        }
        if (!plain) {
            Color ln = (i == cy && focused) ? pal::Ink : Color{ 170, 152, 124, 255 };
            TextRight(FontId::Mono, std::to_string(i + 1), r.x + gutter - 12, y + (lh - fontSize * 1.2f) * 0.5f + 1, fontSize * 0.82f, ln);
        }
    }
    PopClip();
    PushClip(textArea.width > 0 ? Rectangle{ textArea.x - 3, r.y + 1, textArea.width + 10, r.height - 2 } : r);
    // selection
    if (HasSel()) {
        int x0, y0, x1, y1;
        SelRange(x0, y0, x1, y1);
        for (int i = std::max(first, y0); i <= std::min(last, y1); i++) {
            float y = textArea.y + i * lh - scrollY;
            int a = i == y0 ? x0 : 0;
            int b = i == y1 ? x1 : (int)lines[(size_t)i].size() + 1;
            DrawRectangle((int)(textArea.x + a * cw - scrollX), (int)y, (int)((b - a) * cw), (int)lh, Color{ 120, 160, 220, 110 });
        }
    }
    // text
    if (lines.size() == 1 && lines[0].empty() && !placeholder.empty() && !focused)
        Text(FontId::SerifItalic, placeholder, textArea.x, textArea.y + (lh - fontSize * 1.2f) * 0.5f, fontSize, pal::InkFaint);
    for (int i = first; i <= last; i++) {
        float y = textArea.y + i * lh - scrollY + (lh - fontSize * 1.2f) * 0.5f + 1;
        const std::string& l = lines[(size_t)i];
        if (plain) { Text(FontId::Mono, l, textArea.x - scrollX, y, fontSize, pal::Ink); continue; }
        syntax::Classify(l, inBlock, kinds);
        size_t s = 0;
        while (s < l.size()) {
            size_t e = s;
            while (e < l.size() && kinds[e] == kinds[s]) e++;
            Text(kinds[s] == syntax::Keyword ? FontId::MonoBold : FontId::Mono, l.substr(s, e - s), textArea.x + s * cw - scrollX, y, fontSize, syntax::ColorFor(kinds[s]));
            s = e;
        }
    }
    // cursor
    if (focused && interactive && fmodf(blink, 1.0f) < 0.6f) {
        float x = textArea.x + cx * cw - scrollX;
        float y = textArea.y + cy * lh - scrollY;
        DrawRectangle((int)x, (int)y + 2, 2, (int)lh - 4, pal::Ink);
    }
    PopClip();
    // scrollbar
    float contentH = lines.size() * lh;
    if (contentH > textArea.height + 1) {
        float trackH = r.height - 12;
        float thumbH = std::max(24.f, trackH * textArea.height / contentH);
        float t = maxScroll > 0 ? scrollY / maxScroll : 0;
        DrawRectangleRounded(Rectangle{ r.x + r.width - 9, r.y + 6 + (trackH - thumbH) * t, 5, thumbH }, 0.8f, 4, Color{ 150, 130, 100, 150 });
    }
    // hover tooltip for markers
    if (interactive && !plain) {
        for (const Marker& mk : markers) {
            float y = textArea.y + (mk.line - 1) * lh - scrollY;
            if (y < r.y || y > r.y + r.height - lh) continue;
            if (Hover(Rectangle{ r.x, y, gutter, lh })) {
                float w = std::min(520.f, MeasureWidth(FontId::Serif, mk.msg, 15) + 20);
                Rectangle tip{ r.x + gutter, y - 34, w, 30 };
                Panel(tip, Color{ 60, 40, 30, 240 }, pal::Gold, 0.2f, 1.f);
                Text(FontId::Serif, mk.msg, tip.x + 10, tip.y + 6, 15, pal::GoldSoft);
            }
        }
    }
}
