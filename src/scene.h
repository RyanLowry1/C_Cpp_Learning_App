// scene.h - the library room: shelves, desk, props, lights, ambience
#pragma once
#include "common.h"
#include "gfx.h"

namespace world {
constexpr float DeskTopY = 0.788f;         // leather surface of the desk
constexpr float DeskBackZ = 0.0f;          // librarian's side
constexpr float DeskFrontZ = 1.10f;        // visitor's side
constexpr float ShelfFrontZ = -3.22f;
constexpr float ShelfBackZ = -3.60f;
constexpr float BayWidth = 1.5f;
constexpr int Bays = 6;
constexpr float BoardTop[8] = { 0.10f, 0.54f, 0.98f, 1.42f, 1.86f, 2.30f, 2.74f, 3.18f };
inline float BayCenter(int b) { return (b - 2.5f) * BayWidth; }
const Vector3 SeatPos = { 0.f, 0.f, -0.30f };       // librarian's pelvis xz while seated
const Vector3 BookOnDesk = { 0.f, DeskTopY, 0.70f };// spine position of the presented book
const Vector3 CatPos = { 0.84f, DeskTopY, 0.66f };
const Vector3 LampPos = { -0.98f, DeskTopY, 0.14f };
const Vector3 BellPos = { -0.42f, DeskTopY, 0.92f };
const Vector3 DrawerPos = { 0.92f, 0.62f, 0.05f };
}

// A fetchable book on the shelves (topic books + exam papers / workbooks)
struct ShelfBook {
    std::string key;        // topic id or special key ("paper_exam1", "workbook_exam1", ...)
    std::string title;
    Color color;
    Matrix inShelf;         // closed-book transform while standing in the shelf
    Vector3 standPos;       // where the librarian stands to reach it
    float reachY;           // height of the book centre
    bool present = true;    // false while it is out of the shelf
};

class Scene {
public:
    void Build();
    void Shutdown();
    void Update(float dt, float time);
    void Draw(const Camera3D& cam, float time) const;
    void DrawTransparent(const Camera3D& cam, float time) const;

    std::vector<gfx::Light> Lights(float time) const;
    Vector3 Ambient() const { return Vector3{ 0.46f, 0.40f, 0.35f }; }

    int FindShelfBook(const std::string& key) const;
    ShelfBook& Shelf(int i) { return shelf[(size_t)i]; }
    const std::vector<ShelfBook>& ShelfBooks() const { return shelf; }

    // returned-books pile on the desk
    int pileCount = 0;
    std::vector<Color> pileColors;
    Matrix PileSlot(int index) const;

    float chairBack = 0.f;     // 0 tucked in .. 1 pushed back
    float drawerOpen = 0.f;    // 0 closed .. 1 open
    float catPet = 0.f;        // >0 while the cat reacts to a click
    float bellRing = 0.f;

    Matrix ChairMatrix() const;
    // re-letters the brass nameplate on the desk ("A. MALLOC\nHead Librarian")
    void SetNameplate(const std::string& text);

private:
    int nameplateIdx = -1;
    std::vector<Mesh> staticMeshes;
    std::vector<ShelfBook> shelf;
    struct Label { Texture2D tex; Matrix m; };
    std::vector<Label> labels;
    Texture2D rugTex{};
    struct Mote { Vector3 p; Vector3 v; float phase; };
    std::vector<Mote> motes;

    void BuildRoom(gfx::Builder& b);
    void BuildShelves(gfx::Builder& b);
    void BuildSideShelves(gfx::Builder& b, float x, float dir);
    void BuildDesk(gfx::Builder& b);
    void BuildFurniture(gfx::Builder& b);
    void FillShelfRow(gfx::Builder& b, float x0, float x1, float y, float clearance, float frontZ, float depth, util::Rng& rng,
                      const std::vector<std::pair<float, float>>& reserved, float dirZ = 1.f, bool sideways = false, float sideX = 0.f);
    void AddLabel(const std::string& text, Vector3 center, Vector2 size, float yaw, int texW, int texH, float fontSize, bool brass);
    void DrawChair() const;
    void DrawLamp(float time) const;
    void DrawClock(const Camera3D& cam) const;
    void DrawCat(float time) const;
    void DrawBell() const;
    void DrawDrawer() const;
    void DrawPile() const;
};
