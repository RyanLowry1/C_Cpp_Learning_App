// stage.h - the library as a painted 2D set: shelves, floor, chair, desk, props, lighting
#pragma once
#include "common.h"

namespace world {
// Scene space is 1600 x 900. The visitor looks across the desk at the shelves.
constexpr float W = 1600.f, H = 900.f;
constexpr float WallFeetY = 586.f;     // feet line of someone standing at the shelves
constexpr float FloorY = 565.f;        // base of the shelves
constexpr float RimY = 664.f;          // top of the desk's back lip (everything behind it is hidden below this line)
constexpr float DeskEdgeY = 680.f;     // far edge of the leather top
constexpr float SeatFeetY = 732.f;     // feet line of the librarian when he stands behind the desk
constexpr float BayW = 250.f, Bay0X = 50.f;
constexpr int Rows = 7;
constexpr float RowStep = 66.f;
inline float RowBottom(int r) { return 557.f - RowStep * (float)r; }
const Vector2 BookOnDesk{ 800.f, 792.f };
const Vector2 CatPos{ 1345.f, 848.f };
const Vector2 LampPos{ 292.f, 736.f };
const Vector2 BellPos{ 236.f, 848.f };
const Vector2 InkPos{ 1004.f, 744.f };
const Vector2 PileBase{ 1296.f, 796.f };
inline float ScaleAtY(float feetY) { return Lerp(0.70f, 1.0f, Clamp(( feetY - WallFeetY) / (SeatFeetY - WallFeetY), 0.f, 1.f)); }
}

// A fetchable book on the shelves (topic books, exam papers, workbooks)
struct ShelfBook {
    std::string key;        // topic id or special key ("paper_exam1", "workbook_exam1", ...)
    std::string title;
    std::string mark;       // short emblem printed on the spine
    Color color;
    Rectangle rect;         // spine rectangle in scene space
    float standX;           // where the librarian stands to reach it
    bool low;               // on the lower row (he bends down)
    bool present = true;    // false while it is out of the shelf
};

class Stage {
public:
    void Build();
    void Shutdown();
    void Update(float dt, float time);

    // Parallax: layer 0 = far wall, 1 = mid (floor, chair, librarian), 2 = near (desk). par is -1..1.
    Vector2 Shift(int layer, Vector2 par) const;

    void BuildLightmap(float time, Vector2 headPos, float headLight);      // call outside any 2D pass
    void DrawBack(float time, Vector2 par) const;
    void DrawChair(Vector2 par) const;
    void DrawDesk(float time, Vector2 par) const;
    void DrawAtmosphere(float time) const;                                  // dust, then the light map multiplied on top

    int FindShelfBook(const std::string& key) const;
    ShelfBook& Shelf(int i) { return shelf[(size_t)i]; }
    const std::vector<ShelfBook>& ShelfBooks() const { return shelf; }

    // returned-books pile on the desk
    int pileCount = 0;
    std::vector<Color> pileColors;
    Vector2 PileSlot(int index) const;     // centre of the n-th book of the pile (scene space, near layer shift not applied)

    float chairBack = 0.f;     // 0 tucked in .. 1 pushed back
    float catPet = 0.f;        // >0 while the cat reacts to a click
    float bellRing = 0.f;
    Vector2 lampLight{ 292.f, 560.f };

    bool HitCat(Vector2 p) const;
    bool HitBell(Vector2 p) const;

private:
    struct Filler { float x, y, w, h; Color c; int style; float tilt; };
    std::vector<Filler> fillers;
    std::vector<ShelfBook> shelf;
    struct Mote { Vector2 p, v; float phase, size; };
    std::vector<Mote> motes;
    RenderTexture2D lightRT{};
    bool built = false;
    float t = 0.f;

    void DrawBay(int b) const;
    void DrawClock(float time) const;
    void DrawLamp(float time) const;
    void DrawCat(float time) const;
    void DrawBell(float time) const;
    void DrawInk() const;
    void DrawPile() const;
};
