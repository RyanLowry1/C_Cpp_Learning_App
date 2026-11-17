// gfx.cpp - lighting shader, primitive meshes and a static mesh batcher
#include "gfx.h"
#include "rlgl.h"
#include <cstring>

namespace gfx {

// ---------------------------------------------------------------------------
// Shader: per-pixel diffuse + soft specular from 4 point lights, hemisphere
// ambient, exponential-squared fog, optional emissive and two-sided normals.
// ---------------------------------------------------------------------------
static const char* kVS = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
void main() {
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragNormal = normalize(mat3(matNormal) * vertexNormal);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

static const char* kFS = R"(#version 330
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 viewPos;
uniform vec3 ambient;
uniform vec3 lightPos[4];
uniform vec3 lightColor[4];
uniform float lightRange[4];
uniform vec3 fogColor;
uniform float fogDensity;
uniform float emissive;
out vec4 finalColor;
void main() {
    vec4 texel = texture(texture0, fragTexCoord);
    vec4 base = texel * colDiffuse * fragColor;
    if (base.a < 0.01) discard;
    vec3 n = normalize(fragNormal);
    if (!gl_FrontFacing) n = -n;
    vec3 v = normalize(viewPos - fragPosition);
    vec3 lit = ambient * (0.70 + 0.30 * n.y);
    for (int i = 0; i < 4; i++) {
        vec3 L = lightPos[i] - fragPosition;
        float d = length(L);
        L /= max(d, 0.0001);
        float att = clamp(1.0 - d / lightRange[i], 0.0, 1.0);
        att *= att;
        float ndl = max(dot(n, L), 0.0);
        float wrap = max((dot(n, L) + 0.35) / 1.35, 0.0) * 0.25;
        vec3 h = normalize(L + v);
        float spec = pow(max(dot(n, h), 0.0), 40.0) * 0.18;
        lit += lightColor[i] * (ndl * 0.85 + wrap + spec) * att;
    }
    lit += vec3(1.0, 0.96, 0.9) * (0.26 * max(dot(n, v), 0.0));   // soft camera-side fill: side-facing surfaces never go black
    vec3 col = base.rgb * lit;
    float dist = length(viewPos - fragPosition);
    float fog = clamp(1.0 - exp(-fogDensity * dist * dist), 0.0, 1.0);
    col = mix(col, fogColor, fog);
    col = col / (1.0 + col * 0.38);      // highlight roll-off (keeps pale skin and hair from blowing out)
    vec3 glow = mix(base.rgb, fogColor / (1.0 + fogColor * 0.38), fog);   // emissive parts stay full-bright
    col = mix(col, glow, emissive);
    finalColor = vec4(col, base.a);
}
)";

static Shader g_shader{};
static Material g_mat{};
static Texture2D g_white{};
static Texture2D g_blob{};
static Texture2D g_glow{};
static Mesh g_cube{}, g_sphere{}, g_sphereLo{}, g_sphereHi{}, g_cyl{}, g_cone{}, g_torus{}, g_hemi{}, g_quad{};
static int g_locViewPos = -1, g_locAmbient = -1, g_locLightPos = -1, g_locLightColor = -1, g_locLightRange = -1;
static int g_locFogColor = -1, g_locFogDensity = -1, g_locEmissive = -1;
static float g_lastEmissive = -1.f;

Mesh MakeQuadMesh(float u0, float v0, float u1, float v1) {
    Mesh m{};
    m.vertexCount = 4;
    m.triangleCount = 2;
    m.vertices = (float*)MemAlloc(4 * 3 * sizeof(float));
    m.normals = (float*)MemAlloc(4 * 3 * sizeof(float));
    m.texcoords = (float*)MemAlloc(4 * 2 * sizeof(float));
    m.indices = (unsigned short*)MemAlloc(6 * sizeof(unsigned short));
    // a(-x,+z) b(+x,+z) c(+x,-z) d(-x,-z) : CCW seen from +Y
    const float P[12] = { -0.5f, 0, 0.5f,   0.5f, 0, 0.5f,   0.5f, 0, -0.5f,   -0.5f, 0, -0.5f };
    const float T[8] = { u0, v1,  u1, v1,  u1, v0,  u0, v0 };
    memcpy(m.vertices, P, sizeof P);
    memcpy(m.texcoords, T, sizeof T);
    for (int i = 0; i < 4; i++) { m.normals[i * 3 + 0] = 0; m.normals[i * 3 + 1] = 1; m.normals[i * 3 + 2] = 0; }
    const unsigned short I[6] = { 0, 1, 2, 0, 2, 3 };
    memcpy(m.indices, I, sizeof I);
    UploadMesh(&m, false);
    return m;
}

void Init() {
    g_shader = LoadShaderFromMemory(kVS, kFS);
    g_shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(g_shader, "matModel");
    g_shader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(g_shader, "matNormal");
    g_shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(g_shader, "viewPos");
    g_locViewPos = GetShaderLocation(g_shader, "viewPos");
    g_locAmbient = GetShaderLocation(g_shader, "ambient");
    g_locLightPos = GetShaderLocation(g_shader, "lightPos");
    g_locLightColor = GetShaderLocation(g_shader, "lightColor");
    g_locLightRange = GetShaderLocation(g_shader, "lightRange");
    g_locFogColor = GetShaderLocation(g_shader, "fogColor");
    g_locFogDensity = GetShaderLocation(g_shader, "fogDensity");
    g_locEmissive = GetShaderLocation(g_shader, "emissive");

    g_white = Texture2D{ rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    g_mat = LoadMaterialDefault();
    g_mat.shader = g_shader;

    Image blob = GenImageGradientRadial(128, 128, 0.0f, Color{ 0, 0, 0, 255 }, Color{ 0, 0, 0, 0 });
    g_blob = LoadTextureFromImage(blob);
    UnloadImage(blob);
    SetTextureFilter(g_blob, TEXTURE_FILTER_BILINEAR);

    Image glow = GenImageGradientRadial(128, 128, 0.0f, Color{ 255, 255, 255, 255 }, Color{ 255, 255, 255, 0 });
    g_glow = LoadTextureFromImage(glow);
    UnloadImage(glow);
    SetTextureFilter(g_glow, TEXTURE_FILTER_BILINEAR);

    g_cube = GenMeshCube(1.f, 1.f, 1.f);
    g_sphere = GenMeshSphere(1.f, 14, 18);
    g_sphereLo = GenMeshSphere(1.f, 8, 12);       // tiny beads (hair, beads, rivets)
    g_sphereHi = GenMeshSphere(1.f, 28, 40);      // big shapes seen up close (skull, shoulders)
    g_cyl = GenMeshCylinder(1.f, 1.f, 18);
    g_cone = GenMeshCone(1.f, 1.f, 16);
    g_torus = GenMeshTorus(0.12f, 2.0f, 24, 8);
    g_hemi = GenMeshHemiSphere(1.f, 16, 36);   // also the librarians' eyelids, so it has to be smooth
    g_quad = MakeQuadMesh(0, 0, 1, 1);

    rlDisableBackfaceCulling();
}

void Shutdown() {
    UnloadMesh(g_cube); UnloadMesh(g_sphere); UnloadMesh(g_sphereLo); UnloadMesh(g_sphereHi); UnloadMesh(g_cyl); UnloadMesh(g_cone);
    UnloadMesh(g_torus); UnloadMesh(g_hemi); UnloadMesh(g_quad);
    UnloadTexture(g_blob);
    UnloadTexture(g_glow);
    UnloadShader(g_shader);
    MemFree(g_mat.maps);
}

void BeginFrame(const Camera3D& cam, const Light* lights, int count, Vector3 ambient, Vector3 fogColor, float fogDensity) {
    Vector3 lp[4], lc[4];
    float lr[4];
    for (int i = 0; i < 4; i++) {
        if (i < count) { lp[i] = lights[i].pos; lc[i] = lights[i].color; lr[i] = lights[i].range; }
        else { lp[i] = Vector3{ 0, -100, 0 }; lc[i] = Vector3{ 0, 0, 0 }; lr[i] = 1.f; }
    }
    SetShaderValue(g_shader, g_locViewPos, &cam.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(g_shader, g_locAmbient, &ambient, SHADER_UNIFORM_VEC3);
    SetShaderValueV(g_shader, g_locLightPos, lp, SHADER_UNIFORM_VEC3, 4);
    SetShaderValueV(g_shader, g_locLightColor, lc, SHADER_UNIFORM_VEC3, 4);
    SetShaderValueV(g_shader, g_locLightRange, lr, SHADER_UNIFORM_FLOAT, 4);
    SetShaderValue(g_shader, g_locFogColor, &fogColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(g_shader, g_locFogDensity, &fogDensity, SHADER_UNIFORM_FLOAT);
    float e = 0.f;
    SetShaderValue(g_shader, g_locEmissive, &e, SHADER_UNIFORM_FLOAT);
    g_lastEmissive = 0.f;
}

Matrix TRS(Vector3 t, Vector3 e, Vector3 s) {
    Matrix S = MatrixScale(s.x, s.y, s.z);
    Matrix R = MatrixMultiply(MatrixMultiply(MatrixRotateZ(e.z), MatrixRotateX(e.x)), MatrixRotateY(e.y));
    return MatrixMultiply(MatrixMultiply(S, R), MatrixTranslate(t.x, t.y, t.z));
}

Matrix Basis(Vector3 X, Vector3 Y, Vector3 Z, Vector3 T) {
    Matrix m = MatrixIdentity();
    m.m0 = X.x; m.m1 = X.y; m.m2 = X.z;
    m.m4 = Y.x; m.m5 = Y.y; m.m6 = Y.z;
    m.m8 = Z.x; m.m9 = Z.y; m.m10 = Z.z;
    m.m12 = T.x; m.m13 = T.y; m.m14 = T.z;
    return m;
}

Matrix BoxM(Vector3 c, Vector3 s, float yaw) {
    return MatrixMultiply(MatrixMultiply(MatrixScale(s.x, s.y, s.z), MatrixRotateY(yaw)), MatrixTranslate(c.x, c.y, c.z));
}

Matrix LerpRigid(const Matrix& a, const Matrix& b, float t) {
    Quaternion qa = QuaternionFromMatrix(a), qb = QuaternionFromMatrix(b);
    Quaternion q = QuaternionSlerp(qa, qb, t);
    Vector3 p = Vector3Lerp(Origin(a), Origin(b), t);
    Matrix r = QuaternionToMatrix(q);
    r.m12 = p.x; r.m13 = p.y; r.m14 = p.z;
    return r;
}

void DrawMeshLit(const Mesh& mesh, const Matrix& m, Color c, float emissive, const Texture2D* tex) {
    g_mat.maps[MATERIAL_MAP_DIFFUSE].color = c;
    g_mat.maps[MATERIAL_MAP_DIFFUSE].texture = tex ? *tex : g_white;
    if (emissive != g_lastEmissive) {
        SetShaderValue(g_shader, g_locEmissive, &emissive, SHADER_UNIFORM_FLOAT);
        g_lastEmissive = emissive;
    }
    DrawMesh(mesh, g_mat, m);
}

void Cube(const Matrix& m, Color c, float e) { DrawMeshLit(g_cube, m, c, e); }
void Sphere(const Matrix& m, Color c, float e) {
    // pick a mesh density from the sphere's biggest on-screen size: smooth when large, cheap when tiny
    float sx = m.m0 * m.m0 + m.m1 * m.m1 + m.m2 * m.m2, sy = m.m4 * m.m4 + m.m5 * m.m5 + m.m6 * m.m6, sz = m.m8 * m.m8 + m.m9 * m.m9 + m.m10 * m.m10;
    float s2 = std::max(sx, std::max(sy, sz));
    const Mesh& mesh = s2 > 0.03f * 0.03f ? g_sphereHi : (s2 < 0.0065f * 0.0065f ? g_sphereLo : g_sphere);
    DrawMeshLit(mesh, m, c, e);
}
void Cylinder(const Matrix& m, Color c, float e) { DrawMeshLit(g_cyl, m, c, e); }
void Cone(const Matrix& m, Color c, float e) { DrawMeshLit(g_cone, m, c, e); }
void Torus(const Matrix& m, Color c, float e) { DrawMeshLit(g_torus, m, c, e); }
void Hemi(const Matrix& m, Color c, float e) { DrawMeshLit(g_hemi, m, c, e); }
void Quad(const Matrix& m, const Texture2D& tex, Color c, float e) { DrawMeshLit(g_quad, m, c, e, &tex); }
void Box(Vector3 center, Vector3 size, Color c, float yaw, float e) { DrawMeshLit(g_cube, BoxM(center, size, yaw), c, e); }

void Limb(const Matrix& joint, float length, float radius, Color c) {
    Matrix local = MatrixMultiply(MatrixScale(radius, length, radius), MatrixTranslate(0, -length, 0));
    DrawMeshLit(g_cyl, MatrixMultiply(local, joint), c);
}

// ---------------------------------------------------------------------------
// Lathe: dynamic swept surfaces
// ---------------------------------------------------------------------------
namespace {
constexpr int kLatheSlots = 24, kLatheMaxRings = 96, kLatheMaxSegs = 36;
struct LatheSlot {
    Mesh mesh{};
    bool ready = false;
    int idxRings = -1, idxSegs = -1;
    bool idxDown = false;
};
LatheSlot g_lathe[kLatheSlots];

void InitLatheSlot(LatheSlot& s) {
    Mesh& m = s.mesh;
    int maxV = kLatheMaxRings * (kLatheMaxSegs + 1);
    int maxT = (kLatheMaxRings - 1) * kLatheMaxSegs * 2;
    m.vertexCount = maxV;
    m.triangleCount = maxT;
    m.vertices = (float*)MemAlloc((unsigned)(maxV * 3 * sizeof(float)));
    m.normals = (float*)MemAlloc((unsigned)(maxV * 3 * sizeof(float)));
    m.texcoords = (float*)MemAlloc((unsigned)(maxV * 2 * sizeof(float)));
    m.colors = (unsigned char*)MemAlloc((unsigned)(maxV * 4));
    m.indices = (unsigned short*)MemAlloc((unsigned)(maxT * 3 * sizeof(unsigned short)));
    UploadMesh(&m, true);   // dynamic
    s.ready = true;
}
}

void Lathe(int slot, const Matrix& frame, const std::vector<LatheRing>& rings, int segs, float a0, float a1) {
    int n = (int)rings.size();
    if (slot < 0 || slot >= kLatheSlots || n < 2) return;
    n = std::min(n, kLatheMaxRings);
    segs = std::max(6, std::min(segs, kLatheMaxSegs));
    LatheSlot& s = g_lathe[slot];
    if (!s.ready) InitLatheSlot(s);
    Mesh& m = s.mesh;
    const int stride = segs + 1;
    for (int i = 0; i < n; i++) {
        const LatheRing& r = rings[(size_t)i];
        const LatheRing& a = rings[(size_t)std::max(0, i - 1)];
        const LatheRing& b = rings[(size_t)std::min(n - 1, i + 1)];
        float dy = b.y - a.y;
        if (fabsf(dy) < 1e-5f) dy = (i + 1 < n) ? (rings[(size_t)i + 1].y - r.y) : (r.y - a.y);
        float dcx = (b.cx - a.cx) / dy, dcz = (b.cz - a.cz) / dy, drx = (b.rx - a.rx) / dy, drz = (b.rz - a.rz) / dy;
        for (int j = 0; j <= segs; j++) {
            float ang = a0 + (a1 - a0) * (float)j / (float)segs;
            float ca = cosf(ang), sa = sinf(ang);
            int v = i * stride + j;
            m.vertices[v * 3 + 0] = r.cx + r.rx * ca;
            m.vertices[v * 3 + 1] = r.y;
            m.vertices[v * 3 + 2] = r.cz + r.rz * sa;
            // surface normal = cross(tangent around the ring, tangent along the axis)
            Vector3 t1{ -r.rx * sa, 0.f, r.rz * ca };
            Vector3 t2{ dcx + drx * ca, 1.f, dcz + drz * sa };
            Vector3 nrm = Vector3CrossProduct(t2, t1);
            if (nrm.x * ca / std::max(0.0001f, r.rx) + nrm.z * sa / std::max(0.0001f, r.rz) < 0.f) nrm = Vector3Scale(nrm, -1.f);
            nrm = Vector3Normalize(nrm);
            m.normals[v * 3 + 0] = nrm.x; m.normals[v * 3 + 1] = nrm.y; m.normals[v * 3 + 2] = nrm.z;
            m.texcoords[v * 2 + 0] = (float)j / segs; m.texcoords[v * 2 + 1] = (float)i / (n - 1);
            m.colors[v * 4 + 0] = r.c.r; m.colors[v * 4 + 1] = r.c.g; m.colors[v * 4 + 2] = r.c.b; m.colors[v * 4 + 3] = 255;
        }
    }
    // rings listed top-to-bottom would wind every triangle backwards; the shader flips normals on back faces,
    // so such a sweep (an arm, a leg) would be lit from below. Keep the outside the front face either way.
    const bool down = rings[0].y > rings[(size_t)n - 1].y;
    if (s.idxRings != n || s.idxSegs != segs || s.idxDown != down) {
        int k = 0;
        for (int i = 0; i < n - 1; i++)
            for (int j = 0; j < segs; j++) {
                unsigned short a = (unsigned short)(i * stride + j), b = (unsigned short)(i * stride + j + 1);
                unsigned short c = (unsigned short)((i + 1) * stride + j), d = (unsigned short)((i + 1) * stride + j + 1);
                if (down) std::swap(b, c);
                m.indices[k++] = a; m.indices[k++] = c; m.indices[k++] = b;
                m.indices[k++] = b; m.indices[k++] = c; m.indices[k++] = d;
            }
        UpdateMeshBuffer(m, 6, m.indices, (int)(k * sizeof(unsigned short)), 0);
        s.idxRings = n; s.idxSegs = segs; s.idxDown = down;
    }
    int vcount = n * stride;
    UpdateMeshBuffer(m, 0, m.vertices, vcount * 3 * (int)sizeof(float), 0);
    UpdateMeshBuffer(m, 2, m.normals, vcount * 3 * (int)sizeof(float), 0);
    UpdateMeshBuffer(m, 3, m.colors, vcount * 4, 0);
    m.triangleCount = (n - 1) * segs * 2;
    DrawMeshLit(m, frame, WHITE);
}

// ---------------------------------------------------------------------------
// TubeSet: batched smooth tubes
// ---------------------------------------------------------------------------
namespace {
constexpr int kTubeSlots = 8, kTubeMaxVerts = 60000, kTubeMaxIdx = 240000, kTubeSides = 7;
struct TubeSlot { Mesh mesh{}; bool ready = false; };
TubeSlot g_tube[kTubeSlots];
}

void TubeSet::Add(const std::vector<Vector3>& ptsIn, const std::vector<float>& radiiIn, Color c, bool closed) {
    if (ptsIn.size() < 2 || radiiIn.size() < ptsIn.size()) return;
    std::vector<Vector3> pts = ptsIn;
    std::vector<float> radii(radiiIn.begin(), radiiIn.begin() + (long long)ptsIn.size());
    if (!closed) {   // round off both ends (three extra rings each) so no tube is ever a hollow pipe
        static const float capD[3] = { 0.5f, 0.85f, 1.f }, capR[3] = { 0.866f, 0.527f, 0.03f };
        const size_t n0 = pts.size();
        const Vector3 e0 = pts[0], e1 = pts[n0 - 1];
        const Vector3 t0 = Vector3Normalize(Vector3Subtract(e0, pts[1])), t1 = Vector3Normalize(Vector3Subtract(e1, pts[n0 - 2]));
        const float r0 = radii[0], r1 = radii[n0 - 1];
        for (int k = 0; k < 3; k++) {
            pts.insert(pts.begin(), Vector3Add(e0, Vector3Scale(t0, r0 * capD[k])));
            radii.insert(radii.begin(), r0 * capR[k]);
            pts.push_back(Vector3Add(e1, Vector3Scale(t1, r1 * capD[k])));
            radii.push_back(r1 * capR[k]);
        }
    }
    int n = (int)pts.size();
    int rings = n + (closed ? 1 : 0);
    if ((int)(pos.size() / 3) + rings * kTubeSides >= kTubeMaxVerts - 8) return;
    unsigned short base = (unsigned short)(pos.size() / 3);
    Vector3 nPrev{ 0, 0, 0 };
    for (int i = 0; i < rings; i++) {
        int k = i % n;
        const Vector3& a = pts[(size_t)(closed ? (k + n - 1) % n : std::max(0, k - 1))];
        const Vector3& b = pts[(size_t)(closed ? (k + 1) % n : std::min(n - 1, k + 1))];
        Vector3 t = Vector3Normalize(Vector3Subtract(b, a));
        Vector3 nn;
        if (i == 0) {
            Vector3 ref = fabsf(t.y) < 0.9f ? Vector3{ 0, 1, 0 } : Vector3{ 1, 0, 0 };
            nn = Vector3Normalize(Vector3CrossProduct(t, ref));
        } else {
            nn = Vector3Subtract(nPrev, Vector3Scale(t, Vector3DotProduct(nPrev, t)));
            float l = Vector3Length(nn);
            nn = l < 1e-5f ? nPrev : Vector3Scale(nn, 1.f / l);   // rotation-minimising frame: no twisting along the tube
        }
        nPrev = nn;
        Vector3 bb = Vector3CrossProduct(t, nn);
        float r = radii[(size_t)k];
        for (int s = 0; s < kTubeSides; s++) {
            float ang = 2.f * PI * s / kTubeSides;
            Vector3 d = Vector3Add(Vector3Scale(nn, cosf(ang)), Vector3Scale(bb, sinf(ang)));
            Vector3 p = Vector3Add(pts[(size_t)k], Vector3Scale(d, r));
            pos.push_back(p.x); pos.push_back(p.y); pos.push_back(p.z);
            nrm.push_back(d.x); nrm.push_back(d.y); nrm.push_back(d.z);
            col.push_back(c.r); col.push_back(c.g); col.push_back(c.b); col.push_back(255);
        }
    }
    for (int i = 0; i < rings - 1; i++)
        for (int s = 0; s < kTubeSides; s++) {
            int s2 = (s + 1) % kTubeSides;
            unsigned short a = (unsigned short)(base + i * kTubeSides + s), b = (unsigned short)(base + i * kTubeSides + s2);
            unsigned short cc = (unsigned short)(base + (i + 1) * kTubeSides + s), d = (unsigned short)(base + (i + 1) * kTubeSides + s2);
            idx.push_back(a); idx.push_back(b); idx.push_back(cc);   // counter-clockwise seen from outside
            idx.push_back(b); idx.push_back(d); idx.push_back(cc);
        }
}

void TubeSet::Taper(const std::vector<Vector3>& pts, float r0, float rMid, float r1, Color c) {
    int n = (int)pts.size();
    std::vector<float> r((size_t)n);
    for (int i = 0; i < n; i++) {
        float u = n > 1 ? (float)i / (n - 1) : 0.f;
        r[(size_t)i] = u < 0.5f ? r0 + (rMid - r0) * (u * 2.f) : rMid + (r1 - rMid) * ((u - 0.5f) * 2.f);
    }
    Add(pts, r, c, false);
}

void TubeSet::Flush(int slot, const Matrix& frame, float emissive) {
    if (pos.empty() || slot < 0 || slot >= kTubeSlots) { pos.clear(); nrm.clear(); col.clear(); idx.clear(); return; }
    TubeSlot& s = g_tube[slot];
    Mesh& m = s.mesh;
    if (!s.ready) {
        m.vertexCount = kTubeMaxVerts;
        m.triangleCount = kTubeMaxIdx / 3;
        m.vertices = (float*)MemAlloc((unsigned)(kTubeMaxVerts * 3 * sizeof(float)));
        m.normals = (float*)MemAlloc((unsigned)(kTubeMaxVerts * 3 * sizeof(float)));
        m.texcoords = (float*)MemAlloc((unsigned)(kTubeMaxVerts * 2 * sizeof(float)));
        m.colors = (unsigned char*)MemAlloc((unsigned)(kTubeMaxVerts * 4));
        m.indices = (unsigned short*)MemAlloc((unsigned)(kTubeMaxIdx * sizeof(unsigned short)));
        UploadMesh(&m, true);
        s.ready = true;
    }
    int vc = (int)(pos.size() / 3), ic = (int)idx.size();
    UpdateMeshBuffer(m, 0, pos.data(), vc * 3 * (int)sizeof(float), 0);
    UpdateMeshBuffer(m, 2, nrm.data(), vc * 3 * (int)sizeof(float), 0);
    UpdateMeshBuffer(m, 3, col.data(), vc * 4, 0);
    UpdateMeshBuffer(m, 6, idx.data(), ic * (int)sizeof(unsigned short), 0);
    m.triangleCount = ic / 3;
    DrawMeshLit(m, frame, WHITE, emissive);
    pos.clear(); nrm.clear(); col.clear(); idx.clear();
}

void BeginTransparent() { rlDrawRenderBatchActive(); rlDisableDepthMask(); }
void EndTransparent() { rlDrawRenderBatchActive(); rlEnableDepthMask(); }

void BlobShadow(Vector3 center, float sx, float sz, float alpha, float yaw) {
    Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(sx, 1, sz), MatrixRotateY(yaw)), MatrixTranslate(center.x, center.y, center.z));
    DrawMeshLit(g_quad, m, Color{ 255, 255, 255, (unsigned char)(util::Clamp01(alpha) * 255) }, 1.0f, &g_blob);
}

void Glow(Vector3 center, float size, Color c, const Camera3D& cam) {
    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ADDITIVE);
    DrawBillboard(cam, g_glow, center, size, c);
    EndBlendMode();
}

Texture2D WhiteTexture() { return g_white; }
Texture2D GlowTexture() { return g_glow; }
Shader LitShader() { return g_shader; }
const Mesh& QuadMesh() { return g_quad; }

// ---------------------------------------------------------------------------
// Builder
// ---------------------------------------------------------------------------
void Builder::Vert(Vector3 p, Vector3 n, Vector2 t, Color c) {
    pos.push_back(p.x); pos.push_back(p.y); pos.push_back(p.z);
    nrm.push_back(n.x); nrm.push_back(n.y); nrm.push_back(n.z);
    uv.push_back(t.x); uv.push_back(t.y);
    col.push_back(c.r); col.push_back(c.g); col.push_back(c.b); col.push_back(c.a);
}

void Builder::Tri(Vector3 a, Vector3 b, Vector3 c, Vector3 na, Vector3 nb, Vector3 nc, Color k) {
    Vert(a, na, Vector2{ 0, 0 }, k);
    Vert(b, nb, Vector2{ 1, 0 }, k);
    Vert(c, nc, Vector2{ 1, 1 }, k);
}

void Builder::Quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color k) {
    Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(d, a)));
    Vert(a, n, Vector2{ 0, 1 }, k); Vert(b, n, Vector2{ 1, 1 }, k); Vert(c, n, Vector2{ 1, 0 }, k);
    Vert(a, n, Vector2{ 0, 1 }, k); Vert(c, n, Vector2{ 1, 0 }, k); Vert(d, n, Vector2{ 0, 0 }, k);
}

void Builder::Box(const Matrix& m, Color k) {
    struct F { Vector3 n, u, v; };
    static const F faces[6] = {
        { { 1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } },
        { { -1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } },
        { { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } },
        { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } },
        { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } },
        { { 0, 0, -1 }, { -1, 0, 0 }, { 0, 1, 0 } },
    };
    for (const F& f : faces) {
        Vector3 c = Vector3Scale(f.n, 0.5f);
        auto P = [&](float a, float b) {
            Vector3 p = Vector3Add(c, Vector3Add(Vector3Scale(f.u, a * 0.5f), Vector3Scale(f.v, b * 0.5f)));
            return Vector3Transform(p, m);
        };
        Quad(P(-1, -1), P(1, -1), P(1, 1), P(-1, 1), k);
    }
}

void Builder::Box(Vector3 center, Vector3 size, Color k, float yaw) { Box(BoxM(center, size, yaw), k); }

void Builder::BoxTop(Vector3 center, Vector3 size, Color sides, Color top) {
    Box(center, size, sides);
    // overlay a slightly raised top face in a different colour
    float hy = center.y + size.y * 0.5f + 0.0008f;
    float hx = size.x * 0.5f, hz = size.z * 0.5f;
    Quad(Vector3{ center.x - hx, hy, center.z + hz }, Vector3{ center.x + hx, hy, center.z + hz },
         Vector3{ center.x + hx, hy, center.z - hz }, Vector3{ center.x - hx, hy, center.z - hz }, top);
}

void Builder::Cylinder(const Matrix& m, Color k, int slices, bool caps) {
    Matrix nm = MatrixTranspose(MatrixInvert(m));
    nm.m12 = nm.m13 = nm.m14 = 0.f;
    auto N = [&](Vector3 n) { return Vector3Normalize(Vector3Transform(n, nm)); };
    for (int i = 0; i < slices; i++) {
        float a0 = 2.f * PI * i / slices, a1 = 2.f * PI * (i + 1) / slices;
        Vector3 d0{ cosf(a0), 0, sinf(a0) }, d1{ cosf(a1), 0, sinf(a1) };
        Vector3 b0 = Vector3Transform(d0, m), b1 = Vector3Transform(d1, m);
        Vector3 t0 = Vector3Transform(Vector3{ d0.x, 1, d0.z }, m), t1 = Vector3Transform(Vector3{ d1.x, 1, d1.z }, m);
        Vector3 n0 = N(d0), n1 = N(d1);
        Tri(b1, b0, t0, n1, n0, n0, k);
        Tri(b1, t0, t1, n1, n0, n1, k);
        if (caps) {
            Vector3 top = Vector3Transform(Vector3{ 0, 1, 0 }, m), bot = Vector3Transform(Vector3{ 0, 0, 0 }, m);
            Vector3 nu = N(Vector3{ 0, 1, 0 }), nd = N(Vector3{ 0, -1, 0 });
            Tri(top, t1, t0, nu, nu, nu, k);
            Tri(bot, b0, b1, nd, nd, nd, k);
        }
    }
}

Mesh Builder::Build() {
    Mesh m{};
    m.vertexCount = VertexCount();
    m.triangleCount = m.vertexCount / 3;
    m.vertices = (float*)MemAlloc((unsigned int)(pos.size() * sizeof(float)));
    m.normals = (float*)MemAlloc((unsigned int)(nrm.size() * sizeof(float)));
    m.texcoords = (float*)MemAlloc((unsigned int)(uv.size() * sizeof(float)));
    m.colors = (unsigned char*)MemAlloc((unsigned int)col.size());
    memcpy(m.vertices, pos.data(), pos.size() * sizeof(float));
    memcpy(m.normals, nrm.data(), nrm.size() * sizeof(float));
    memcpy(m.texcoords, uv.data(), uv.size() * sizeof(float));
    memcpy(m.colors, col.data(), col.size());
    UploadMesh(&m, false);
    pos.clear(); nrm.clear(); uv.clear(); col.clear();
    return m;
}

} // namespace gfx
