// code_editor.h - a small but capable code editor widget drawn on the book page
#pragma once
#include "common.h"

class CodeEditor {
public:
    struct Marker { int line; std::string msg; bool error; };

    bool focused = false;
    bool plain = false;          // no syntax colours / line numbers (answer boxes)
    bool singleLine = false;     // Enter submits instead of inserting a newline
    bool readOnly = false;
    bool changed = false;        // set on every edit; the owner clears it
    bool submitted = false;      // set when Enter is pressed in singleLine mode
    std::string placeholder;
    std::vector<Marker> markers; // compiler errors / warnings (1-based lines)

    void SetText(const std::string& s);
    std::string GetText() const;
    void Draw(Rectangle r, bool interactive, float fontSize = 17.f);
    void GotoLine(int line);
    int LineCount() const { return (int)lines.size(); }

private:
    std::vector<std::string> lines{ std::string() };
    int cx = 0, cy = 0;          // cursor column / line
    int ax = 0, ay = 0;          // selection anchor
    bool hasAnchor = false;
    float scrollY = 0, scrollX = 0;
    float blink = 0;
    bool dragging = false;
    double lastClick = 0;
    int clicks = 0;
    bool wantVisible = false;
    int lastEditKind = 0;        // 1 = typing letters (merged undo)
    struct Snap { std::vector<std::string> lines; int cx, cy; };
    std::vector<Snap> undo, redo;
    float cw = 9, lh = 20;
    Rectangle textArea{};

    void PushUndo(int kind = 0);
    bool HasSel() const;
    void SelRange(int& x0, int& y0, int& x1, int& y1) const;
    std::string SelText() const;
    void DeleteSel();
    void InsertText(const std::string& s);
    void InsertChar(char c);
    void Newline();
    void Backspace();
    void DeleteForward();
    void MoveTo(int x, int y, bool select);
    void HandleKeys();
    void HandleMouse(bool interactive);
    void ToggleComment();
    void IndentSel(bool out);
    int IndentOf(int line) const;
    void Clamp();
    Vector2 PosToCell(Vector2 m) const;
};
