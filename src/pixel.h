// pixel.h - the 8-bit display pipeline: a small fixed-height canvas, a limited palette with ordered dithering,
// and a "sharp bilinear" upscale so the big chunky pixels stay crisp at any window size.
#pragma once
#include "common.h"

namespace pixel {

constexpr int CanvasH = 540;       // the canvas is always this tall; its width follows the window's aspect ratio

int CanvasW();
void Init();
void Shutdown();
void BeginCanvas();                // (re)sizes the canvas to the window and starts drawing into it
void EndCanvas();                  // palette + dither pass, then scales the canvas up to the window
void UpdateMouseMapping();         // makes GetMousePosition() return canvas coordinates

} // namespace pixel
