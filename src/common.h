// common.h - small shared helpers (math easing, colours, RNG, file + string utils)
#pragma once
#include "raylib.h"
#include "raymath.h"
#include <string>
#include <vector>
#include <cmath>
#include <cstdint>
#include <functional>

namespace util {

// ---------- easing / damping ----------
inline float Clamp01(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
inline float Smooth(float t) { t = Clamp01(t); return t * t * (3.f - 2.f * t); }
inline float Smoother(float t) { t = Clamp01(t); return t * t * t * (t * (t * 6.f - 15.f) + 10.f); }
inline float EaseOutCubic(float t) { t = Clamp01(t); float u = 1.f - t; return 1.f - u * u * u; }
inline float EaseInCubic(float t) { t = Clamp01(t); return t * t * t; }
inline float EaseInOutCubic(float t) { t = Clamp01(t); return t < 0.5f ? 4.f * t * t * t : 1.f - powf(-2.f * t + 2.f, 3.f) * 0.5f; }
inline float EaseOutBack(float t) { t = Clamp01(t); const float c1 = 1.70158f, c3 = c1 + 1.f; return 1.f + c3 * powf(t - 1.f, 3.f) + c1 * powf(t - 1.f, 2.f); }
inline float Remap01(float x, float a, float b) { return Clamp01((x - a) / (b - a)); }
inline float Damp(float cur, float target, float rate, float dt) { return target + (cur - target) * expf(-rate * dt); }
inline Vector3 Damp3(Vector3 c, Vector3 t, float rate, float dt) {
    float k = expf(-rate * dt);
    return Vector3Add(t, Vector3Scale(Vector3Subtract(c, t), k));
}
inline float WrapAngle(float a) { while (a > PI) a -= 2.f * PI; while (a < -PI) a += 2.f * PI; return a; }
inline float LerpAngle(float a, float b, float t) { return a + WrapAngle(b - a) * t; }
inline float DampAngle(float cur, float target, float rate, float dt) { return cur + WrapAngle(target - cur) * (1.f - expf(-rate * dt)); }

// ---------- colours ----------
inline Color RGBA(int r, int g, int b, int a = 255) { return Color{ (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a }; }
inline Color Shade(Color c, float k) {
    auto f = [&](unsigned char v) { float x = v * k; return (unsigned char)(x < 0 ? 0 : (x > 255 ? 255 : x)); };
    return Color{ f(c.r), f(c.g), f(c.b), c.a };
}
inline Color WithAlpha(Color c, float a) { c.a = (unsigned char)(Clamp01(a) * 255.f); return c; }
inline Color Mix(Color a, Color b, float t) { return ColorLerp(a, b, Clamp01(t)); }

// ---------- deterministic RNG (xorshift) ----------
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed = 12345u) : s(seed ? seed : 1u) {}
    uint32_t Next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float F() { return (Next() & 0xFFFFFF) / 16777216.0f; }
    float R(float a, float b) { return a + (b - a) * F(); }
    int I(int a, int b) { return a + (int)(Next() % (uint32_t)(b - a + 1)); }
};

// ---------- files / paths ----------
std::string AssetPath(const std::string& rel);   // <exe dir>/assets/rel (falls back to ./assets/rel)
std::string SavePath(const std::string& rel);    // <exe dir>/save/rel  (directory is created)
std::string ReadTextFile(const std::string& path, bool* ok = nullptr);
bool WriteTextFile(const std::string& path, const std::string& text);
bool FileExistsAt(const std::string& path);
void EnsureDir(const std::string& path);
std::string TempWorkDir();                       // %TEMP%/MallocsLibrary

// ---------- strings ----------
std::string Trim(const std::string& s);
std::vector<std::string> Split(const std::string& s, char d);
std::vector<std::string> SplitLines(const std::string& s);
std::string ToLower(std::string s);
bool StartsWith(const std::string& s, const std::string& p);
bool EndsWith(const std::string& s, const std::string& p);
std::string ReplaceAll(std::string s, const std::string& a, const std::string& b);
std::string Unescape(const std::string& s);      // turns \n into a newline, \t into a tab, \\ into a backslash
std::string CollapseSpaces(const std::string& s);
std::string FormatDate(long long epoch);         // "04 Nov 2026, 14:32"
std::string FormatDuration(float seconds);       // "12:05"
long long NowEpoch();

} // namespace util
