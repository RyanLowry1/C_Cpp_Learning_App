// pixel.cpp - canvas, palette quantiser and upscaler
#include "pixel.h"
#include <algorithm>

namespace pixel {

float minLine = 1.f;

static int g_w = 0;
static RenderTexture2D g_canvas{}, g_dithered{};
static Shader g_dither{}, g_sharp{};
static int g_locDTex = -1, g_locDPal = -1, g_locDAmt = -1, g_locSTex = -1, g_locSScale = -1;

// 40 colours: the warm library palette plus every colour the interface uses, so flat UI stays perfectly flat
static const unsigned char kPalette[40][3] = {
    { 10, 7, 8 }, { 26, 18, 16 }, { 44, 29, 22 }, { 74, 46, 30 }, { 112, 72, 44 }, { 150, 104, 62 }, { 190, 146, 92 }, { 226, 190, 134 },
    { 243, 234, 212 }, { 205, 186, 150 }, { 150, 130, 108 }, { 98, 80, 64 }, { 46, 34, 26 },
    { 88, 24, 30 }, { 150, 44, 40 }, { 196, 76, 60 }, { 240, 200, 190 },
    { 176, 120, 30 }, { 212, 172, 92 }, { 246, 226, 180 }, { 255, 246, 214 },
    { 24, 52, 40 }, { 46, 98, 70 }, { 84, 150, 100 }, { 196, 222, 192 },
    { 28, 34, 70 }, { 44, 74, 120 }, { 92, 40, 96 }, { 120, 150, 200 }, { 200, 214, 236 },
    { 236, 190, 150 }, { 204, 152, 116 },
    { 28, 96, 100 }, { 176, 78, 30 }, { 130, 30, 34 }, { 255, 255, 255 }, { 160, 160, 170 }, { 90, 90, 100 }, { 60, 56, 70 }, { 120, 100, 80 },
};

static const char* kDitherFs = R"GLSL(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec2 texSize;
uniform vec3 pal[40];
uniform float amount;
out vec4 finalColor;
const int bayer[16] = int[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
void main() {
    ivec2 ip = ivec2(floor(fragTexCoord * texSize));
    vec3 c = texelFetch(texture0, ip, 0).rgb;
    float t = (float(bayer[(ip.y & 3) * 4 + (ip.x & 3)]) + 0.5) / 16.0 - 0.5;
    c += t * amount;
    float best = 1e9;
    vec3 bc = pal[0];
    for (int i = 0; i < 40; i++) {
        vec3 d = c - pal[i];
        float dist = dot(d * d, vec3(0.30, 0.50, 0.20));
        if (dist < best) { best = dist; bc = pal[i]; }
    }
    finalColor = vec4(bc, 1.0);
}
)GLSL";

static const char* kSharpFs = R"GLSL(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec2 texSize;
uniform vec2 scale;
out vec4 finalColor;
void main() {
    vec2 p = fragTexCoord * texSize;
    vec2 i = floor(p);
    vec2 f = p - i;
    f = clamp((f - 0.5) * scale + 0.5, 0.0, 1.0);
    finalColor = vec4(texture(texture0, (i + f) / texSize).rgb, 1.0);
}
)GLSL";

int CanvasW() { return g_w > 0 ? g_w : CanvasH * 16 / 9; }

static int WantedW() {
    float aspect = (float)GetScreenWidth() / std::max(1, GetScreenHeight());
    aspect = std::max(4.f / 3.f, std::min(21.f / 9.f, aspect));
    int w = (int)(CanvasH * aspect + 0.5f);
    return w & ~1;
}

void Init() {
    g_dither = LoadShaderFromMemory(nullptr, kDitherFs);
    g_sharp = LoadShaderFromMemory(nullptr, kSharpFs);
    g_locDPal = GetShaderLocation(g_dither, "pal");
    g_locDAmt = GetShaderLocation(g_dither, "amount");
    g_locDTex = GetShaderLocation(g_dither, "texSize");
    g_locSTex = GetShaderLocation(g_sharp, "texSize");
    g_locSScale = GetShaderLocation(g_sharp, "scale");
    float pal[40 * 3];
    for (int i = 0; i < 40; i++) for (int k = 0; k < 3; k++) pal[i * 3 + k] = kPalette[i][k] / 255.f;
    SetShaderValueV(g_dither, g_locDPal, pal, SHADER_UNIFORM_VEC3, 40);
    float amt = 0.085f;
    SetShaderValue(g_dither, g_locDAmt, &amt, SHADER_UNIFORM_FLOAT);
}

void Shutdown() {
    if (g_w > 0) { UnloadRenderTexture(g_canvas); UnloadRenderTexture(g_dithered); }
    UnloadShader(g_dither);
    UnloadShader(g_sharp);
    g_w = 0;
}

void UpdateMouseMapping() {
    SetMouseOffset(0, 0);
    SetMouseScale((float)CanvasW() / std::max(1, GetScreenWidth()), (float)CanvasH / std::max(1, GetScreenHeight()));
}

void BeginCanvas() {
    int w = WantedW();
    if (w != g_w) {
        if (g_w > 0) { UnloadRenderTexture(g_canvas); UnloadRenderTexture(g_dithered); }
        g_w = w;
        g_canvas = LoadRenderTexture(w, CanvasH);
        g_dithered = LoadRenderTexture(w, CanvasH);
        SetTextureFilter(g_canvas.texture, TEXTURE_FILTER_POINT);
        SetTextureFilter(g_dithered.texture, TEXTURE_FILTER_BILINEAR);
    }
    BeginTextureMode(g_canvas);
}

void EndCanvas() {
    EndTextureMode();
    float tex[2] = { (float)g_w, (float)CanvasH };
    SetShaderValue(g_dither, g_locDTex, tex, SHADER_UNIFORM_VEC2);
    BeginTextureMode(g_dithered);
    BeginShaderMode(g_dither);
    DrawTexturePro(g_canvas.texture, Rectangle{ 0, 0, (float)g_w, -(float)CanvasH }, Rectangle{ 0, 0, (float)g_w, (float)CanvasH }, Vector2{ 0, 0 }, 0, WHITE);
    EndShaderMode();
    EndTextureMode();

    float ww = (float)GetScreenWidth(), wh = (float)GetScreenHeight();
    float scale[2] = { ww / g_w, wh / CanvasH };
    SetShaderValue(g_sharp, g_locSTex, tex, SHADER_UNIFORM_VEC2);
    SetShaderValue(g_sharp, g_locSScale, scale, SHADER_UNIFORM_VEC2);
    BeginShaderMode(g_sharp);
    DrawTexturePro(g_dithered.texture, Rectangle{ 0, 0, (float)g_w, -(float)CanvasH }, Rectangle{ 0, 0, ww, wh }, Vector2{ 0, 0 }, 0, WHITE);
    EndShaderMode();
}

} // namespace pixel
