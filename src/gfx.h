// gfx.h - lit 3D rendering helpers built on raylib meshes + a custom lighting shader
#pragma once
#include "common.h"

namespace gfx {

struct Light {
    Vector3 pos;
    Vector3 color;   // already multiplied by intensity
    float range;
};

void Init();
void Shutdown();

// Call once per frame inside BeginMode3D, before drawing lit geometry.
void BeginFrame(const Camera3D& cam, const Light* lights, int count, Vector3 ambient, Vector3 fogColor, float fogDensity);

// ---------- matrix helpers (raylib order: MatrixMultiply(local, parent)) ----------
Matrix TRS(Vector3 t, Vector3 euler, Vector3 s);      // rotation order: roll(z) -> pitch(x) -> yaw(y)
Matrix Basis(Vector3 X, Vector3 Y, Vector3 Z, Vector3 T);
Matrix BoxM(Vector3 center, Vector3 size, float yaw = 0.f);
inline Matrix Mul(const Matrix& local, const Matrix& parent) { return MatrixMultiply(local, parent); }
inline Vector3 Origin(const Matrix& m) { return Vector3{ m.m12, m.m13, m.m14 }; }
inline Vector3 AxisX(const Matrix& m) { return Vector3{ m.m0, m.m1, m.m2 }; }
inline Vector3 AxisY(const Matrix& m) { return Vector3{ m.m4, m.m5, m.m6 }; }
inline Vector3 AxisZ(const Matrix& m) { return Vector3{ m.m8, m.m9, m.m10 }; }
Matrix LerpRigid(const Matrix& a, const Matrix& b, float t);   // interpolate translation + rotation

// ---------- lit drawing ----------
void DrawMeshLit(const Mesh& mesh, const Matrix& m, Color c, float emissive = 0.f, const Texture2D* tex = nullptr);
void Cube(const Matrix& m, Color c, float emissive = 0.f);       // unit cube centred on origin
void Sphere(const Matrix& m, Color c, float emissive = 0.f);     // radius 1
void Cylinder(const Matrix& m, Color c, float emissive = 0.f);   // radius 1, y in [0,1]
void Cone(const Matrix& m, Color c, float emissive = 0.f);       // radius 1, y in [0,1]
void Torus(const Matrix& m, Color c, float emissive = 0.f);      // ring radius 1 around Z axis, thin tube
void Hemi(const Matrix& m, Color c, float emissive = 0.f);       // dome, radius 1, y >= 0
void Quad(const Matrix& m, const Texture2D& tex, Color c, float emissive = 0.f); // unit quad in XZ, facing +Y
void Box(Vector3 center, Vector3 size, Color c, float yaw = 0.f, float emissive = 0.f);
void Limb(const Matrix& joint, float length, float radius, Color c);   // cylinder hanging down -Y from a joint

// transparent extras (draw after opaque geometry)
void BeginTransparent();
void EndTransparent();
void BlobShadow(Vector3 center, float sx, float sz, float alpha, float yaw = 0.f);
void Glow(Vector3 center, float size, Color c, const Camera3D& cam);   // additive camera-facing sprite

Texture2D WhiteTexture();
Texture2D GlowTexture();
Shader LitShader();
const Mesh& QuadMesh();
Mesh MakeQuadMesh(float u0, float v0, float u1, float v1);   // XZ unit quad; (u0,v0) at -x,-z corner

// ---------- smooth swept surfaces ----------
// A lathe is one smooth mesh built from rings stacked along the local Y axis. Each ring is an ellipse
// (radii rx,rz) centred at (cx, y, cz) with its own vertex colour, so bodies, limbs and necks come out as
// continuous organic surfaces instead of a pile of overlapping balls. Each slot owns a dynamic GPU mesh that is
// refilled when drawn (use a different slot for each lathe drawn in the same frame, 0..23). Two rings at
// (nearly) the same y with different colours make a hard colour edge. Ends are open: hide them inside other parts.
struct LatheRing { float y, cx, cz, rx, rz; Color c; };
// a0..a1 limits the sweep to an arc (radians; x = cos a, z = sin a, so +Z, the front, is PI/2): used for hair that skips the face
void Lathe(int slot, const Matrix& frame, const std::vector<LatheRing>& rings, int segs = 28, float a0 = 0.f, float a1 = 2.f * PI);

// A batch of smooth tubes (strands, lips, rims, fingers) drawn in a single call. Every tube has its own radius at
// each point, so tapered ends make it look like one continuous organic stroke rather than a chain of balls.
class TubeSet {
public:
    void Add(const std::vector<Vector3>& pts, const std::vector<float>& radii, Color c, bool closed = false);
    void Taper(const std::vector<Vector3>& pts, float r0, float rMid, float r1, Color c);   // thin - thick - thin
    void Flush(int slot, const Matrix& frame, float emissive = 0.f);                        // draws and clears (slots 0..7)
private:
    std::vector<float> pos, nrm;
    std::vector<unsigned char> col;
    std::vector<unsigned short> idx;
};

// ---------- static geometry batching ----------
struct Builder {
    std::vector<float> pos, nrm, uv;
    std::vector<unsigned char> col;
    void Vert(Vector3 p, Vector3 n, Vector2 t, Color c);
    void Tri(Vector3 a, Vector3 b, Vector3 c, Vector3 na, Vector3 nb, Vector3 nc, Color col);
    void Quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color col);   // CCW seen from the front
    void Box(const Matrix& m, Color col);
    void Box(Vector3 center, Vector3 size, Color col, float yaw = 0.f);
    void BoxTop(Vector3 center, Vector3 size, Color sides, Color top);
    void Cylinder(const Matrix& m, Color col, int slices = 14, bool caps = true);
    int VertexCount() const { return (int)(pos.size() / 3); }
    Mesh Build();   // uploads to GPU and clears the builder
};

} // namespace gfx
