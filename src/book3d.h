// book3d.h - the physical book the librarian fetches, places and opens on the desk
#pragma once
#include "common.h"

namespace book {

// Dimensions in metres. Local frame: spine along Z at x = 0, pages to +X (right half),
// the left half (front cover + left pages) hinges around the spine.
constexpr float W = 0.312f;        // width of one page
constexpr float D = 0.400f;        // page height (along Z)
constexpr float Cover = 0.0045f;   // board thickness
constexpr float Half = 0.018f;     // page block thickness per half
constexpr float PageY = Cover + Half;  // y of the page surface when open (pivot height)
constexpr int PageTexW = 1560;     // page-space virtual resolution of the open spread
constexpr int PageTexH = 1000;

struct Look {
    std::string title;
    std::string subtitle;
    std::string emblem;
    std::string volume;
    Color cover{ 90, 30, 30, 255 };
};

class Book3D {
public:
    Matrix world = MatrixIdentity();   // book origin = spine, bottom of the closed book
    float open = 0.f;                  // 0 closed .. 1 fully open
    bool visible = false;
    bool pagesTextured = false;
    float pageEmissive = 0.45f;
    Texture2D pageTex{};               // render texture showing the page UI (flipped)

    void Init();
    void Shutdown();
    void SetLook(const Look& look);    // regenerates the cover texture
    const Look& GetLook() const { return look; }
    void Draw() const;

    // world-space corners of the open spread (for projecting the 2D page UI)
    Vector3 SpreadTopLeft() const;
    Vector3 SpreadBottomRight() const;
    Vector3 SpreadCenter() const;

private:
    Look look;
    Texture2D coverTex{};
    bool hasCover = false;
    Mesh leftPage{}, rightPage{}, coverQuad{};
};

// Simple closed book used for the shelf copies (no cover texture)
void DrawClosedBook(const Matrix& m, Color cover, bool goldBands = true);

// Transform for a closed book standing upright in a shelf with its spine facing +Z.
// `pos` = centre of the spine's bottom edge... see implementation for the exact anchor.
Matrix StandingInShelf(Vector3 spineBottomFront);

} // namespace book
