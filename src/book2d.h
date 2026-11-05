// book2d.h - the book the librarian fetches: a painted cover, and an open spread seen from above
#pragma once
#include "common.h"

namespace book {

constexpr int PageW = 780;       // one page in page space (the views draw a 1560 x 1000 spread)
constexpr int PageH = 1000;
constexpr float CoverAspect = (float)PageH / (float)PageW;

struct Look {
    std::string title;
    std::string subtitle;
    std::string emblem;
    std::string volume;
    Color cover{ 90, 30, 30, 255 };
};

class Book2D {
public:
    bool visible = false;

    void Init();
    void Shutdown();
    void SetLook(const Look& look);                 // repaints the cover
    const Look& GetLook() const { return look; }

    // Closed book, cover towards the viewer. c = centre, w = width, squash = vertical foreshortening
    // (1 = held upright, ~0.6 = lying on the desk).
    void DrawClosed(Vector2 c, float w, float squash, float rotDeg = 0.f, bool shadow = true) const;

    // The book lying open on the desk seen from above. c = centre of the fully open spread,
    // s = screen pixels per page unit, open = 0 closed .. 1 open (the cover swings across the spine).
    void DrawSpread(Vector2 c, float s, float squash, float open, Texture2D pageTex) const;

    // Only the leather board, page block and shadow of the fully open book (the live page UI is drawn on top).
    void DrawBoard(Vector2 c, float s) const;

private:
    Look look;
    RenderTexture2D cover{};
    bool ready = false;
};

// A closed book standing in the shelf, seen from the spine
void DrawSpine(Rectangle r, Color c, const std::string& mark, bool highlight);

} // namespace book
