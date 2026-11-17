// sdf.cpp - evaluating the sculpted field and polygonising it with naive surface nets
#include "sdf.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <thread>

namespace sdf {
namespace {

constexpr int kGroups = 8;
constexpr float kFar = 1000.f;
constexpr int kBlock = 8;   // samples per block edge; each block only evaluates the solids that can reach it

// Distance from a point to one solid. Never larger than the true distance outside the solid, so it is safe to
// use for culling. Exact for balls; a first-order estimate for stretched ellipsoids and tapered capsules.
inline float PrimDist(const Prim& q, float px, float py, float pz) {
    float vx = px - q.a.x, vy = py - q.a.y, vz = pz - q.a.z;
    if (q.kind == Prim::Ellipsoid) {
        float bs = sqrtf(vx * vx + vy * vy + vz * vz) - q.br;
        float lx = (q.rot[0] * vx + q.rot[1] * vy + q.rot[2] * vz) * q.inv.x;
        float ly = (q.rot[3] * vx + q.rot[4] * vy + q.rot[5] * vz) * q.inv.y;
        float lz = (q.rot[6] * vx + q.rot[7] * vy + q.rot[8] * vz) * q.inv.z;
        float k0 = sqrtf(lx * lx + ly * ly + lz * lz);
        float mx = lx * q.inv.x, my = ly * q.inv.y, mz = lz * q.inv.z;
        float k1 = sqrtf(mx * mx + my * my + mz * mz);
        float d = k1 > 1e-9f ? k0 * (k0 - 1.f) / k1 : -q.rmin;
        return d > bs ? d : bs;
    }
    // Tapered capsule: the exact distance to the hull of its two end balls (Inigo Quilez's "round cone").
    // Exact matters: lerping the radius along the axis leaves each end ball standing proud of a tapering
    // neighbour, which shows as a ring at every joint of a lock.
    const float l2 = q.inv.y, il2 = q.inv.x, a2 = q.inv.z, rr = q.ra - q.rb;
    if (a2 <= 0.f) {   // one end ball swallows the other (or the ends coincide)
        float da = sqrtf(vx * vx + vy * vy + vz * vz) - q.ra;
        float wx = vx - q.b.x, wy = vy - q.b.y, wz = vz - q.b.z;
        float db = sqrtf(wx * wx + wy * wy + wz * wz) - q.rb;
        return da < db ? da : db;
    }
    float y = vx * q.b.x + vy * q.b.y + vz * q.b.z, z = y - l2;
    float wx = vx * l2 - q.b.x * y, wy = vy * l2 - q.b.y * y, wz = vz * l2 - q.b.z * y;
    float x2 = wx * wx + wy * wy + wz * wz, y2 = y * y * l2, z2 = z * z * l2;
    float k = (rr > 0.f ? 1.f : (rr < 0.f ? -1.f : 0.f)) * rr * rr * x2;
    if ((z > 0.f ? a2 : -a2) * z2 > k) return sqrtf(x2 + z2) * il2 - q.rb;
    if ((y > 0.f ? a2 : -a2) * y2 < k) return sqrtf(x2 + y2) * il2 - q.ra;
    return (sqrtf(x2 * a2 * il2) + y * rr) * il2 - q.ra;
}

struct Acc {
    float d[kGroups];
    float c[kGroups][3];
};

// Runs the listed solids in order. Within a group they melt together (polynomial smooth min / max).
template <bool COLOR>
inline void Eval(const std::vector<Prim>& prims, const int* list, int n, float px, float py, float pz, Acc& A) {
    for (int g = 0; g < kGroups; g++) {
        A.d[g] = kFar;
        if (COLOR) A.c[g][0] = A.c[g][1] = A.c[g][2] = 0.f;
    }
    auto merge = [&](const Prim& q, float d) {
        float& D = A.d[q.group];
        float h;
        if (q.mode == Prim::Add) {
            h = 0.5f + 0.5f * (D - d) / q.k;
            h = h < 0.f ? 0.f : (h > 1.f ? 1.f : h);
            D = D * (1.f - h) + d * h - q.k * h * (1.f - h);
        } else {
            h = 0.5f - 0.5f * (D + d) / q.k;
            h = h < 0.f ? 0.f : (h > 1.f ? 1.f : h);
            D = D * (1.f - h) - d * h + q.k * h * (1.f - h);
        }
        if (COLOR) for (int c = 0; c < 3; c++) A.c[q.group][c] += (q.col[c] - A.c[q.group][c]) * h;
    };
    const Prim* lock = nullptr;   // the lock being gathered: its cones are joined with a plain min first
    float lockD = kFar;
    for (int i = 0; i < n; i++) {
        const Prim& q = prims[(size_t)list[i]];
        if (lock && q.chain != lock->chain) { merge(*lock, lockD); lock = nullptr; }
        if (q.mode == Prim::Paint) {
            if (!COLOR) continue;
            float w = 0.5f - 0.5f * PrimDist(q, px, py, pz) / q.k;
            w = w < 0.f ? 0.f : (w > 1.f ? 1.f : w);
            w = w * w * (3.f - 2.f * w) * q.alpha;
            for (int c = 0; c < 3; c++) A.c[q.group][c] += (q.col[c] - A.c[q.group][c]) * w;
            continue;
        }
        float d = PrimDist(q, px, py, pz);
        if (q.chain) {
            lockD = lock ? std::min(lockD, d) : d;
            lock = &q;
        } else merge(q, d);
    }
    if (lock) merge(*lock, lockD);
}

inline int NearestGroup(const Acc& A) {
    int best = 0;
    for (int g = 1; g < kGroups; g++) if (A.d[g] < A.d[best]) best = g;
    return best;
}

void SetCol(Prim& q, Color c) { q.col[0] = c.r / 255.f; q.col[1] = c.g / 255.f; q.col[2] = c.b / 255.f; }

void RunThreads(const std::function<void()>& work) {
    unsigned n = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> pool;
    for (unsigned i = 1; i < n; i++) pool.emplace_back(work);
    work();
    for (auto& t : pool) t.join();
}

} // namespace

// ---------------------------------------------------------------------------
// building a model
// ---------------------------------------------------------------------------
void Model::Ell(Vector3 c, Vector3 r, Color col, int group, float k, Vector3 e) {
    Prim q;
    q.kind = Prim::Ellipsoid;
    q.group = (unsigned char)(group & 7);
    q.k = std::max(k, 1e-4f);
    q.a = c; q.b = r;
    q.inv = Vector3{ 1.f / r.x, 1.f / r.y, 1.f / r.z };
    q.rmin = std::min(r.x, std::min(r.y, r.z));
    if (e.x != 0.f || e.y != 0.f || e.z != 0.f) {
        Matrix m = MatrixMultiply(MatrixMultiply(MatrixRotateZ(e.z), MatrixRotateX(e.x)), MatrixRotateY(e.y));
        q.rot[0] = m.m0; q.rot[1] = m.m1; q.rot[2] = m.m2;
        q.rot[3] = m.m4; q.rot[4] = m.m5; q.rot[5] = m.m6;
        q.rot[6] = m.m8; q.rot[7] = m.m9; q.rot[8] = m.m10;
    }
    q.bc = c;
    q.br = std::max(r.x, std::max(r.y, r.z));
    q.bh = (e.x != 0.f || e.y != 0.f || e.z != 0.f) ? Vector3{ q.br, q.br, q.br } : r;
    SetCol(q, col);
    prims.push_back(q);
}

void Model::Cone(Vector3 a, Vector3 b, float ra, float rb, Color col, int group, float k) {
    Prim q;
    q.kind = Prim::Cone;
    q.group = (unsigned char)(group & 7);
    q.k = std::max(k, 1e-4f);
    q.a = a;
    q.b = Vector3Subtract(b, a);
    float l2 = Vector3DotProduct(q.b, q.b);
    q.inv.x = l2 > 1e-12f ? 1.f / l2 : 0.f;
    q.inv.y = l2;
    q.inv.z = l2 > 1e-12f ? l2 - (ra - rb) * (ra - rb) : 0.f;   // <= 0 when one end ball contains the other
    q.ra = ra; q.rb = rb;
    q.bc = Vector3Lerp(a, b, 0.5f);
    q.br = 0.5f * sqrtf(l2) + std::max(ra, rb);
    q.bh = Vector3{ 0.5f * fabsf(q.b.x) + std::max(ra, rb), 0.5f * fabsf(q.b.y) + std::max(ra, rb), 0.5f * fabsf(q.b.z) + std::max(ra, rb) };
    SetCol(q, col);
    prims.push_back(q);
}

void Model::Lock(const std::vector<Vector3>& pts, const std::vector<float>& radii, Color col, int group, float k) {
    int n = (int)pts.size();
    if (n < 2 || (int)radii.size() < n) return;
    const int sub = 3;   // Catmull-Rom subdivisions per span, so a few control points give a smooth curve
    int id = nextChain++;
    Vector3 prev = pts[0];
    float prevR = radii[0];
    for (int i = 0; i < n - 1; i++) {
        Vector3 p0 = pts[(size_t)std::max(0, i - 1)], p1 = pts[(size_t)i], p2 = pts[(size_t)i + 1], p3 = pts[(size_t)std::min(n - 1, i + 2)];
        for (int s = 1; s <= sub; s++) {
            float t = (float)s / sub, t2 = t * t, t3 = t2 * t;
            auto cr = [&](float a, float b, float c, float d) {
                return 0.5f * (2.f * b + (c - a) * t + (2.f * a - 5.f * b + 4.f * c - d) * t2 + (3.f * b - a - 3.f * c + d) * t3);
            };
            Vector3 p{ cr(p0.x, p1.x, p2.x, p3.x), cr(p0.y, p1.y, p2.y, p3.y), cr(p0.z, p1.z, p2.z, p3.z) };
            float r = radii[(size_t)i] + (radii[(size_t)i + 1] - radii[(size_t)i]) * t;
            Cone(prev, p, prevR, r, col, group, k);
            prims.back().chain = id;
            prev = p; prevR = r;
        }
    }
}

void Model::Lock(const std::vector<Vector3>& pts, float r0, float rMid, float r1, Color col, int group, float k) {
    std::vector<float> r(pts.size());
    for (size_t i = 0; i < pts.size(); i++) {
        float u = pts.size() > 1 ? (float)i / (pts.size() - 1) : 0.f;
        r[i] = u < 0.5f ? r0 + (rMid - r0) * (u * 2.f) : rMid + (r1 - rMid) * ((u - 0.5f) * 2.f);
    }
    Lock(pts, r, col, group, k);
}

void Model::Carve(Vector3 c, Vector3 r, Color col, int group, float k, Vector3 euler) {
    Ell(c, r, col, group, k, euler);
    prims.back().mode = Prim::Carve;
}

void Model::Paint(Vector3 c, Vector3 r, Color col, float soft, float alpha, int group) {
    Ell(c, r, col, group, soft);
    prims.back().mode = Prim::Paint;
    prims.back().alpha = alpha;
    prims.back().br += soft;
    prims.back().bh = Vector3AddValue(prims.back().bh, soft);
}

float Model::Dist(Vector3 p, int group) const {
    const bool all = group < 0;
    group = all ? 8 : (group & 7);
    std::vector<int>& L = groupList[group];
    // rebuilt whenever solids were added since the last query (the list starts with the solid count it was made for)
    if (L.empty() || L[0] != (int)prims.size()) {
        L.assign(1, (int)prims.size());
        for (int i = 0; i < (int)prims.size(); i++)
            if ((all || prims[(size_t)i].group == group) && prims[(size_t)i].mode != Prim::Paint) L.push_back(i);
    }
    Acc A;
    Eval<false>(prims, L.data() + 1, (int)L.size() - 1, p.x, p.y, p.z, A);
    return all ? A.d[NearestGroup(A)] : A.d[group];
}

// ---------------------------------------------------------------------------
// polygonising
// ---------------------------------------------------------------------------
std::vector<Mesh> Model::Build(float h) const {
    double t0 = GetTime();
    std::vector<Mesh> out;
    Vector3 lo{ 1e9f, 1e9f, 1e9f }, hi{ -1e9f, -1e9f, -1e9f };
    for (const Prim& q : prims) {
        if (q.mode != Prim::Add) continue;
        lo = Vector3Min(lo, Vector3Subtract(q.bc, q.bh));
        hi = Vector3Max(hi, Vector3Add(q.bc, q.bh));
    }
    if (lo.x > hi.x) return out;
    const float pad = 6.f * h;   // margin around the model; also how far the ambient-occlusion probes reach
    lo = Vector3SubtractValue(lo, pad + h);
    hi = Vector3AddValue(hi, pad + h);
    const int nx = (int)ceilf((hi.x - lo.x) / h) + 1, ny = (int)ceilf((hi.y - lo.y) / h) + 1, nz = (int)ceilf((hi.z - lo.z) / h) + 1;
    std::vector<float> f((size_t)nx * ny * nz, kFar);
    auto F = [&](int i, int j, int k) -> float& { return f[((size_t)k * ny + j) * nx + i]; };

    // ---- 1. sample the field, block by block, on every core ----
    const int bx = (nx + kBlock - 1) / kBlock, by = (ny + kBlock - 1) / kBlock, bz = (nz + kBlock - 1) / kBlock;
    const int nblocks = bx * by * bz;
    std::vector<std::vector<int>> cand((size_t)nblocks);
    const float rb = 0.5f * 1.7321f * (kBlock + 1) * h;
    {
        std::atomic<int> next{ 0 };
        RunThreads([&]() {
            Acc A;
            for (;;) {
                int b = next.fetch_add(1);
                if (b >= nblocks) break;
                int bi = b % bx, bj = (b / bx) % by, bk = b / (bx * by);
                Vector3 c{ lo.x + (bi + 0.5f) * kBlock * h, lo.y + (bj + 0.5f) * kBlock * h, lo.z + (bk + 0.5f) * kBlock * h };
                std::vector<int>& L = cand[(size_t)b];
                bool solid = false;
                for (int i = 0; i < (int)prims.size(); i++) {
                    const Prim& q = prims[(size_t)i];
                    float reach = rb + 2.f * q.k + pad;   // measured from the solid's bounding box
                    float dx = std::max(0.f, fabsf(c.x - q.bc.x) - q.bh.x), dy = std::max(0.f, fabsf(c.y - q.bc.y) - q.bh.y), dz = std::max(0.f, fabsf(c.z - q.bc.z) - q.bh.z);
                    if (dx * dx + dy * dy + dz * dz < reach * reach) { L.push_back(i); solid = solid || q.mode == Prim::Add; }
                }
                if (!solid) { L.clear(); continue; }
                int i1 = std::min(nx, (bi + 1) * kBlock), j1 = std::min(ny, (bj + 1) * kBlock), k1 = std::min(nz, (bk + 1) * kBlock);
                for (int k = bk * kBlock; k < k1; k++)
                    for (int j = bj * kBlock; j < j1; j++)
                        for (int i = bi * kBlock; i < i1; i++) {
                            Eval<false>(prims, L.data(), (int)L.size(), lo.x + i * h, lo.y + j * h, lo.z + k * h, A);
                            F(i, j, k) = A.d[NearestGroup(A)];
                        }
            }
        });
    }

    // ---- 2. surface nets: one vertex in every cell the surface passes through ----
    const int cx = nx - 1, cy = ny - 1, cz = nz - 1;
    std::vector<int> cellV((size_t)cx * cy * cz, -1);
    auto CV = [&](int i, int j, int k) -> int& { return cellV[((size_t)k * cy + j) * cx + i]; };
    std::vector<Vector3> P;   // vertex positions in grid units
    static const int kEdge[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
    for (int k = 0; k < cz; k++)
        for (int j = 0; j < cy; j++)
            for (int i = 0; i < cx; i++) {
                float v[8];
                int mask = 0;
                for (int c = 0; c < 8; c++) {
                    v[c] = F(i + (c & 1), j + ((c >> 1) & 1), k + ((c >> 2) & 1));
                    if (v[c] < 0.f) mask |= 1 << c;
                }
                if (mask == 0 || mask == 255) continue;
                Vector3 s{ 0, 0, 0 };
                int cnt = 0;
                for (const auto& e : kEdge) {
                    int a = e[0], b = e[1];
                    if ((v[a] < 0.f) == (v[b] < 0.f)) continue;
                    float t = v[a] / (v[a] - v[b]);
                    s.x += (a & 1) + ((b & 1) - (a & 1)) * t;
                    s.y += ((a >> 1) & 1) + (((b >> 1) & 1) - ((a >> 1) & 1)) * t;
                    s.z += ((a >> 2) & 1) + (((b >> 2) & 1) - ((a >> 2) & 1)) * t;
                    cnt++;
                }
                CV(i, j, k) = (int)P.size();
                P.push_back(Vector3{ i + s.x / cnt, j + s.y / cnt, k + s.z / cnt });
            }

    // ---- 3. a quad across every grid edge that crosses the surface, wound counter-clockwise seen from outside ----
    std::vector<unsigned> tris;
    for (int k = 1; k < nz - 1; k++)
        for (int j = 1; j < ny - 1; j++)
            for (int i = 1; i < nx - 1; i++) {
                float f0 = F(i, j, k);
                for (int axis = 0; axis < 3; axis++) {
                    float f1 = F(i + (axis == 0), j + (axis == 1), k + (axis == 2));
                    if ((f0 < 0.f) == (f1 < 0.f)) continue;
                    int q[4];
                    if (axis == 0) { q[0] = CV(i, j - 1, k - 1); q[1] = CV(i, j, k - 1); q[2] = CV(i, j, k); q[3] = CV(i, j - 1, k); }
                    else if (axis == 1) { q[0] = CV(i - 1, j, k - 1); q[1] = CV(i, j, k - 1); q[2] = CV(i, j, k); q[3] = CV(i - 1, j, k); }
                    else { q[0] = CV(i - 1, j - 1, k); q[1] = CV(i, j - 1, k); q[2] = CV(i, j, k); q[3] = CV(i - 1, j, k); }
                    if (q[0] < 0 || q[1] < 0 || q[2] < 0 || q[3] < 0) continue;
                    Vector3 n = Vector3CrossProduct(Vector3Subtract(P[(size_t)q[2]], P[(size_t)q[0]]), Vector3Subtract(P[(size_t)q[3]], P[(size_t)q[1]]));
                    float along = axis == 0 ? n.x : (axis == 1 ? n.y : n.z);
                    if ((along > 0.f) != (f0 < 0.f)) std::swap(q[1], q[3]);   // outside is where the field is positive
                    bool d02 = Vector3DistanceSqr(P[(size_t)q[0]], P[(size_t)q[2]]) <= Vector3DistanceSqr(P[(size_t)q[1]], P[(size_t)q[3]]);
                    const int order[2][6] = { { 1, 2, 3, 1, 3, 0 }, { 0, 1, 2, 0, 2, 3 } };
                    for (int c = 0; c < 6; c++) tris.push_back((unsigned)q[order[d02 ? 1 : 0][c]]);
                }
            }
    const int nv = (int)P.size();
    if (nv == 0 || tris.empty()) return out;

    // ---- 4. per vertex: normal from the field's gradient, colour from the solids, baked ambient occlusion ----
    auto Tri = [&](float x, float y, float z) {
        x = Clamp(x, 0.f, nx - 1.001f); y = Clamp(y, 0.f, ny - 1.001f); z = Clamp(z, 0.f, nz - 1.001f);
        int i = (int)x, j = (int)y, k = (int)z;
        float u = x - i, v = y - j, w = z - k;
        float a = F(i, j, k) * (1 - u) + F(i + 1, j, k) * u, b = F(i, j + 1, k) * (1 - u) + F(i + 1, j + 1, k) * u;
        float c = F(i, j, k + 1) * (1 - u) + F(i + 1, j, k + 1) * u, d = F(i, j + 1, k + 1) * (1 - u) + F(i + 1, j + 1, k + 1) * u;
        return (a * (1 - v) + b * v) * (1 - w) + (c * (1 - v) + d * v) * w;
    };
    std::vector<Vector3> N((size_t)nv);
    std::vector<Color> C((size_t)nv);
    {
        std::atomic<int> next{ 0 };
        const float probeT[4] = { 1.2f, 2.4f, 4.f, 6.f }, probeW[4] = { 0.35f, 0.3f, 0.2f, 0.15f };
        RunThreads([&]() {
            Acc A;
            for (;;) {
                int v0 = next.fetch_add(256);
                if (v0 >= nv) break;
                for (int v = v0; v < std::min(nv, v0 + 256); v++) {
                    Vector3 g = P[(size_t)v];
                    Vector3 n{ Tri(g.x + 1, g.y, g.z) - Tri(g.x - 1, g.y, g.z), Tri(g.x, g.y + 1, g.z) - Tri(g.x, g.y - 1, g.z), Tri(g.x, g.y, g.z + 1) - Tri(g.x, g.y, g.z - 1) };
                    n = Vector3Normalize(n);
                    N[(size_t)v] = n;
                    int b = ((int)g.x / kBlock) + bx * (((int)g.y / kBlock) + by * ((int)g.z / kBlock));
                    const std::vector<int>& L = cand[(size_t)b];
                    Eval<true>(prims, L.data(), (int)L.size(), lo.x + g.x * h, lo.y + g.y * h, lo.z + g.z * h, A);
                    // where two materials meet (hair on skin), fade between them over about a cell instead of
                    // flipping per vertex, which would leave a saw-toothed edge
                    int g1 = NearestGroup(A), g2 = g1 == 0 ? 1 : 0;
                    for (int q = 0; q < kGroups; q++) if (q != g1 && A.d[q] < A.d[g2]) g2 = q;
                    float mixw = Clamp(0.5f + 0.5f * (A.d[g2] - A.d[g1]) / (0.8f * h), 0.f, 1.f);
                    float col[3];
                    for (int c = 0; c < 3; c++) col[c] = A.c[g2][c] + (A.c[g1][c] - A.c[g2][c]) * mixw;
                    float occ = 0.f;
                    for (int s = 0; s < 4; s++) {
                        float t = probeT[s];
                        float d = Tri(g.x + n.x * t, g.y + n.y * t, g.z + n.z * t) / h;
                        occ += probeW[s] * Clamp((t - d) / t, 0.f, 1.f);
                    }
                    float shade = 1.f - ao * occ;
                    C[(size_t)v] = Color{ (unsigned char)(Clamp(col[0] * shade, 0.f, 1.f) * 255.f), (unsigned char)(Clamp(col[1] * shade, 0.f, 1.f) * 255.f),
                                          (unsigned char)(Clamp(col[2] * shade, 0.f, 1.f) * 255.f), 255 };
                }
            }
        });
    }

    // ---- 5. split into meshes small enough for 16-bit indices ----
    std::vector<int> remap((size_t)nv, -1), used;
    std::vector<unsigned short> idx;
    auto flush = [&]() {
        if (idx.empty()) return;
        Mesh m{};
        m.vertexCount = (int)used.size();
        m.triangleCount = (int)idx.size() / 3;
        m.vertices = (float*)MemAlloc((unsigned)(used.size() * 3 * sizeof(float)));
        m.normals = (float*)MemAlloc((unsigned)(used.size() * 3 * sizeof(float)));
        m.texcoords = (float*)MemAlloc((unsigned)(used.size() * 2 * sizeof(float)));
        m.colors = (unsigned char*)MemAlloc((unsigned)(used.size() * 4));
        m.indices = (unsigned short*)MemAlloc((unsigned)(idx.size() * sizeof(unsigned short)));
        memset(m.texcoords, 0, used.size() * 2 * sizeof(float));
        for (size_t u = 0; u < used.size(); u++) {
            int g = used[u];
            m.vertices[u * 3 + 0] = lo.x + P[(size_t)g].x * h; m.vertices[u * 3 + 1] = lo.y + P[(size_t)g].y * h; m.vertices[u * 3 + 2] = lo.z + P[(size_t)g].z * h;
            m.normals[u * 3 + 0] = N[(size_t)g].x; m.normals[u * 3 + 1] = N[(size_t)g].y; m.normals[u * 3 + 2] = N[(size_t)g].z;
            memcpy(m.colors + u * 4, &C[(size_t)g], 4);
            remap[(size_t)g] = -1;
        }
        memcpy(m.indices, idx.data(), idx.size() * sizeof(unsigned short));
        UploadMesh(&m, false);
        out.push_back(m);
        used.clear();
        idx.clear();
    };
    for (size_t t = 0; t < tris.size(); t += 3) {
        if (used.size() + 3 > 65000) flush();
        for (int c = 0; c < 3; c++) {
            unsigned g = tris[t + c];
            if (remap[g] < 0) { remap[g] = (int)used.size(); used.push_back((int)g); }
            idx.push_back((unsigned short)remap[g]);
        }
    }
    flush();
    TraceLog(LOG_INFO, "SDF: %d solids -> %d vertices, %d triangles, %d meshes, grid %dx%dx%d, %.0f ms", (int)prims.size(), nv,
             (int)tris.size() / 3, (int)out.size(), nx, ny, nz, (GetTime() - t0) * 1000.0);
    return out;
}

} // namespace sdf
