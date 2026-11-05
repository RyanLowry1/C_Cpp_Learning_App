// art.h - small 2D drawing toolkit: soft glows and shadows, shaded ellipses, polygons, capsules, 2-bone IK
#pragma once
#include "common.h"

namespace art {

void Init();
void Shutdown();

// Soft radial blobs (a generated gradient texture). Glow is meant for additive blending or light maps,
// Shadow is a soft black ellipse drawn with ordinary alpha blending.
void Glow(Vector2 c, float rx, float ry, Color col);
void Shadow(Vector2 c, float rx, float ry, float alpha);

// Shapes (all in the current transform's coordinates)
void Ell(Vector2 c, float rx, float ry, Color col, float rotDeg = 0.f);
void EllShaded(Vector2 c, float rx, float ry, Color col);                     // stacked ellipses give a rounded, lit look
void Poly(const Vector2* p, int n, Color col);                                // convex polygon, winding-safe
void Cap(Vector2 a, Vector2 b, float thick, Color col);                       // line with round caps
void RRect(Rectangle r, float radius, Color col);                             // rounded rectangle, radius in pixels
void RRectLines(Rectangle r, float radius, float thick, Color col);
void Quad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color col);             // convex quad in any order

// Elbow position for a two-bone arm. `side` is -1 for a left-hand-side arm and +1 for the right: the elbow
// prefers to point outwards and downwards.
Vector2 Elbow(Vector2 shoulder, Vector2 hand, float l1, float l2, float side);
Vector2 ClampReach(Vector2 shoulder, Vector2 hand, float l1, float l2);

} // namespace art
