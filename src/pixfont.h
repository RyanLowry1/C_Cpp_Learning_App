// pixfont.h - the built-in 5x7 bitmap font used by the 8-bit edition
#pragma once
#include "common.h"

namespace pixfont {

void Init();
void Shutdown();
int Index(int codepoint);                         // glyph for a Unicode code point (non-ASCII falls back to a look-alike)
int Advance(int glyph, bool mono);                // advance in font pixels, gap included
// Draw one glyph. (x, y) = top-left in drawing units, pix = size of one font pixel in drawing units, k = integer magnification.
void Draw(int glyph, float x, float y, float pix, int k, Color c, bool mono, bool bold, bool italic);

} // namespace pixfont
