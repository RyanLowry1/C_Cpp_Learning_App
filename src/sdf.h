// sdf.h - sculpting with signed distance fields.
// A Model is a list of simple solids (ellipsoids and tapered capsules). Solids in the same group melt into one
// another like clay, groups meet with a crease, "carves" scoop material away and "paints" only tint the surface.
// Build() turns the result into ordinary triangle meshes with baked colour and ambient occlusion, fine enough
// that no single polygon can be picked out. Used for the librarians' heads, hair and hands.
#pragma once
#include "common.h"

namespace sdf {

struct Prim {
    enum Kind : unsigned char { Ellipsoid, Cone };
    enum Mode : unsigned char { Add, Carve, Paint };
    Kind kind = Ellipsoid;
    Mode mode = Add;
    unsigned char group = 0;
    int chain = 0;             // cones of one lock share an id: they are joined exactly, then melt in as one piece
    Vector3 a{}, b{};          // ellipsoid: centre, radii   | cone: start, (end - start)
    Vector3 inv{};             // ellipsoid: 1 / radii       | cone: x = 1 / |b|^2, y = |b|^2, z = |b|^2 - (ra - rb)^2
    float ra = 0.f, rb = 0.f;  // cone: radius at the start / end
    float rot[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };   // ellipsoid: rows are its local axes
    float rmin = 0.f;          // ellipsoid: smallest radius
    float k = 0.01f;           // Add / Carve: blend distance   | Paint: softness of its edge
    float col[3] = { 1, 1, 1 };
    float alpha = 1.f;         // Paint: strength
    Vector3 bc{};              // bounding sphere, and the half-extents of a box round the same centre
    Vector3 bh{};              // (the box is much tighter for long or flat solids, so far fewer samples are wasted)
    float br = 0.f;
};

class Model {
public:
    std::vector<Prim> prims;
    float ao = 0.6f;           // how strongly creases are darkened

    // Solids. `group` picks the material (0..7); `k` is how far the solid melts into its neighbours.
    void Ell(Vector3 c, Vector3 r, Color col, int group = 0, float k = 0.012f, Vector3 euler = Vector3{ 0, 0, 0 });
    void Ball(Vector3 c, float r, Color col, int group = 0, float k = 0.012f) { Ell(c, Vector3{ r, r, r }, col, group, k); }
    void Cone(Vector3 a, Vector3 b, float ra, float rb, Color col, int group = 0, float k = 0.012f);
    // A lock: a smooth curve through `pts` with a radius at each point (or thin - thick - thin).
    void Lock(const std::vector<Vector3>& pts, const std::vector<float>& radii, Color col, int group = 0, float k = 0.006f);
    void Lock(const std::vector<Vector3>& pts, float r0, float rMid, float r1, Color col, int group = 0, float k = 0.006f);
    // Scoop an ellipsoid out of a group; the hollow takes `col`.
    void Carve(Vector3 c, Vector3 r, Color col, int group = 0, float k = 0.006f, Vector3 euler = Vector3{ 0, 0, 0 });
    // Tint the surface of a group inside an ellipsoid (soft edge of width `soft`). Paints go after the solids.
    void Paint(Vector3 c, Vector3 r, Color col, float soft, float alpha = 1.f, int group = 0);

    float Dist(Vector3 p, int group = -1) const;  // signed distance to one group, or to the whole model (negative inside)
    std::vector<Mesh> Build(float cell) const;    // polygonise at the given cell size (metres)

private:
    int nextChain = 1;
    mutable std::vector<int> groupList[9];        // [8] lists every solid
};

} // namespace sdf
