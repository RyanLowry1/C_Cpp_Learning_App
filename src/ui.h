// ui.h - immediate-mode UI toolkit: fonts, text layout, widgets, icons, clipping.
// Every UI pass draws in "virtual" coordinates that map to the screen (or a render
// texture) through   screen = offset + virtual * scale.
#pragma once
#include "common.h"

enum class FontId { Serif = 0, SerifBold, SerifItalic, Mono, MonoBold, Hand, Count };

enum class Icon {
    Book, Quiz, Quill, Code, Target, Ledger, Gear, Help, Close, ArrowLeft, ArrowRight,
    Check, Cross, Play, Bulb, Clock, Lock, Star, Bell, Refresh, Eye, Flag, Dot
};

namespace ui {

// Palette shared by the whole app (warm library tones)
namespace pal {
    const Color Paper      = { 243, 234, 212, 255 };
    const Color PaperDark  = { 226, 211, 180, 255 };
    const Color PaperEdge  = { 205, 186, 150, 255 };
    const Color Ink        = { 46, 34, 26, 255 };
    const Color InkSoft    = { 98, 80, 64, 255 };
    const Color InkFaint   = { 150, 130, 108, 255 };
    const Color Wood       = { 74, 46, 28, 255 };
    const Color WoodDark   = { 44, 27, 17, 255 };
    const Color WoodLight  = { 120, 80, 50, 255 };
    const Color Gold       = { 212, 172, 92, 255 };
    const Color GoldSoft   = { 236, 212, 150, 255 };
    const Color Green      = { 46, 98, 70, 255 };
    const Color GreenSoft  = { 196, 222, 192, 255 };
    const Color Red        = { 150, 44, 40, 255 };
    const Color RedSoft    = { 240, 200, 190, 255 };
    const Color Blue       = { 44, 74, 120, 255 };
    const Color BlueSoft   = { 200, 214, 236, 255 };
    const Color Amber      = { 176, 120, 30, 255 };
    const Color AmberSoft  = { 246, 226, 180, 255 };
}

enum ButtonStyle { BtnPaper = 0, BtnPrimary, BtnDanger, BtnLink, BtnWood, BtnGhost };

void LoadFonts();
void UnloadFonts();

// ---- per-frame ----
void NewFrame();          // reset per-frame input state (call once at the start of each frame)
void EndFrameCursor();    // apply hand cursor if a clickable element was hovered

// ---- passes ----
void Begin(Vector2 offset, float scale, bool interactive);
void End();
float Scale();
Vector2 Offset();
Vector2 ToScreen(Vector2 v);

// ---- input (virtual coordinates of the current pass) ----
Vector2 Mouse();
bool Interactive();
bool Hover(Rectangle r);
bool Pressed(Rectangle r);   // left mouse pressed inside r this frame (consumes the press)
bool AnyPress();             // unconsumed left press anywhere
bool MouseDown();
float Wheel();               // unconsumed wheel movement
void ConsumePress();
void ConsumeWheel();
void WantHand();             // request pointing-hand cursor
void CaptureKeyboard();      // a text field has focus this frame: global hotkeys should be ignored
bool KeyboardCaptured();

// ---- text ----
float MeasureWidth(FontId f, const std::string& s, float size, float spacing = 0.f);
std::string Ellipsize(FontId f, const std::string& s, float size, float maxW);   // trims with "..." to fit
Vector2 Measure(FontId f, const std::string& s, float size, float spacing = 0.f);
void Text(FontId f, const std::string& s, float x, float y, float size, Color c, float spacing = 0.f);
void TextCentered(FontId f, const std::string& s, Rectangle r, float size, Color c);
void TextRight(FontId f, const std::string& s, float rightX, float y, float size, Color c);
float Wrap(FontId f, const std::string& s, float x, float y, float width, float size, Color c,
           float lineH = 1.32f, bool draw = true);
// Rich text: **bold**, __italic__, `code`. Returns used height.
float Rich(const std::string& s, float x, float y, float width, float size, Color c,
           float lineH = 1.38f, bool draw = true);
float MonoCharWidth(float size);

// ---- clipping ----
void PushClip(Rectangle r);
void PopClip();

// ---- widgets / drawing ----
void Panel(Rectangle r, Color fill, Color border, float roundness = 0.12f, float borderW = 2.f);
bool Button(Rectangle r, const std::string& label, ButtonStyle style = BtnPaper, bool enabled = true, float fontSize = 0.f);
bool IconButton(Rectangle r, Icon icon, ButtonStyle style = BtnPaper, bool enabled = true);
void DrawIcon(Icon icon, Vector2 c, float size, Color col);
void ProgressBar(Rectangle r, float t, Color fill, Color back);
void Ornament(float cx, float y, float width, Color c);   // little decorative divider
bool Checkbox(Rectangle r, const std::string& label, bool* value);
bool Slider(Rectangle r, float* value);

} // namespace ui
