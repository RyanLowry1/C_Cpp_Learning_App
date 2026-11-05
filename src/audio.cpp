// audio.cpp - tiny software synth used to generate every sound in the app
#include "audio.h"
#include <cstring>

namespace sfx {

static const int kRate = 22050;
static Sound g_sounds[Count];
static Sound g_voice[8];
static bool g_ready = false;
static bool g_voiceOn = true;
static float g_master = 0.8f;
static util::Rng g_rng(777);

// Build a mono 16-bit wave from a sample generator
static Sound MakeSound(float seconds, const std::function<float(int, float)>& gen) {
    int frames = (int)(seconds * kRate);
    Wave w{};
    w.frameCount = (unsigned int)frames;
    w.sampleRate = kRate;
    w.sampleSize = 16;
    w.channels = 1;
    short* data = (short*)MemAlloc((unsigned int)(frames * sizeof(short)));
    for (int i = 0; i < frames; i++) {
        float v = gen(i, (float)i / kRate);
        if (v > 1.f) v = 1.f; if (v < -1.f) v = -1.f;
        v = floorf(v * 7.f + 0.5f) / 7.f;          // crush to a handful of levels, like an 8-bit sound chip
        data[i] = (short)(v * 32000.f);
    }
    w.data = data;
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}

// simple biquad band-pass for formants
struct BandPass {
    float b0, b1, b2, a1, a2, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    BandPass(float freq, float q) {
        float w0 = 2.f * PI * freq / kRate, alpha = sinf(w0) / (2.f * q);
        float a0 = 1.f + alpha;
        b0 = alpha / a0; b1 = 0; b2 = -alpha / a0;
        a1 = -2.f * cosf(w0) / a0; a2 = (1.f - alpha) / a0;
    }
    float Run(float x) {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

void Init() {
    InitAudioDevice();
    if (!IsAudioDeviceReady()) { TraceLog(LOG_WARNING, "SFX: audio device not available"); return; }

    // --- librarian voice: little square-wave blips, like a text box in an old console game ---
    for (int k = 0; k < 8; k++) {
        float f0 = 140.f + k * 24.f;
        float duty = (k % 2) ? 0.25f : 0.5f;
        g_voice[k] = MakeSound(0.07f, [=](int, float t) {
            float ph = fmodf(t * f0 * (1.f + 0.05f * sinf(t * 70.f)), 1.f);
            float sq = ph < duty ? 1.f : -1.f;
            float env = util::Clamp01(t / 0.006f) * util::Clamp01((0.07f - t) / 0.03f);
            return sq * env * 0.34f;
        });
    }

    // --- footstep: low filtered thump ---
    {
        float lp = 0;
        g_sounds[Footstep] = MakeSound(0.12f, [&](int, float t) {
            float n = g_rng.R(-1, 1);
            lp += (n - lp) * 0.08f;
            float env = expf(-t * 38.f);
            return (lp * 2.2f + sinf(2 * PI * 70.f * t) * 0.4f) * env;
        });
    }
    // --- thud: book placed on desk ---
    {
        float lp = 0;
        g_sounds[Thud] = MakeSound(0.28f, [&](int, float t) {
            float n = g_rng.R(-1, 1);
            lp += (n - lp) * 0.12f;
            float env = expf(-t * 16.f);
            return (sinf(2 * PI * (95.f - t * 60.f) * t) * 0.8f + lp * 1.2f) * env;
        });
    }
    // --- page turn: swishing band-limited noise ---
    {
        float lp = 0, hp = 0, prev = 0;
        g_sounds[Page] = MakeSound(0.32f, [&](int, float t) {
            float n = g_rng.R(-1, 1);
            float k = 0.25f + 0.5f * sinf(t / 0.32f * PI);
            lp += (n - lp) * k;
            hp = lp - prev; prev = lp;
            float env = sinf(util::Clamp01(t / 0.32f) * PI);
            env *= env;
            return (hp * 2.5f + lp * 0.4f) * env * 0.55f;
        });
    }
    // --- correct: two-note chime ---
    g_sounds[Correct] = MakeSound(0.7f, [&](int, float t) {
        float a = sinf(2 * PI * 784.f * t) * expf(-t * 6.f);
        float t2 = t - 0.09f;
        float b = t2 > 0 ? sinf(2 * PI * 1175.f * t2) * expf(-t2 * 5.f) : 0.f;
        float shimmer = sinf(2 * PI * 2350.f * t) * expf(-t * 14.f) * 0.15f;
        return (a * 0.45f + b * 0.5f + shimmer) * util::Clamp01(t / 0.004f);
    });
    // --- wrong: soft descending low tone ---
    g_sounds[Wrong] = MakeSound(0.42f, [&](int, float t) {
        float f = 196.f - t * 90.f;
        float s = sinf(2 * PI * f * t) + 0.35f * sinf(2 * PI * f * 2.f * t);
        return s * 0.42f * expf(-t * 5.5f) * util::Clamp01(t / 0.01f);
    });
    // --- click: wooden tick ---
    {
        float lp = 0;
        g_sounds[Click] = MakeSound(0.05f, [&](int, float t) {
            float n = g_rng.R(-1, 1);
            lp += (n - lp) * 0.5f;
            return (lp + sinf(2 * PI * 1800.f * t) * 0.5f) * expf(-t * 120.f) * 0.7f;
        });
    }
    g_sounds[Hover] = MakeSound(0.03f, [&](int, float t) { return sinf(2 * PI * 2400.f * t) * expf(-t * 200.f) * 0.25f; });
    // --- desk bell: inharmonic partials ---
    g_sounds[Bell] = MakeSound(1.6f, [&](int, float t) {
        const float p[5] = { 1.0f, 2.76f, 5.40f, 8.93f, 13.34f };
        const float a[5] = { 0.5f, 0.3f, 0.18f, 0.1f, 0.06f };
        float s = 0;
        for (int i = 0; i < 5; i++) s += sinf(2 * PI * 1250.f * p[i] * t) * a[i] * expf(-t * (2.0f + i * 1.6f));
        return s * util::Clamp01(t / 0.002f);
    });
    // --- book opening: creak + rustle ---
    {
        float lp = 0;
        g_sounds[Open] = MakeSound(0.5f, [&](int, float t) {
            float n = g_rng.R(-1, 1);
            lp += (n - lp) * 0.3f;
            float env = sinf(util::Clamp01(t / 0.5f) * PI);
            float creak = sinf(2 * PI * (300.f + 80.f * sinf(t * 30.f)) * t) * 0.15f * expf(-t * 4.f);
            return (lp * 0.6f + creak) * env * 0.6f;
        });
    }
    // --- drawer slide ---
    {
        float lp = 0;
        g_sounds[Drawer] = MakeSound(0.45f, [&](int, float t) {
            float n = g_rng.R(-1, 1);
            lp += (n - lp) * 0.06f;
            float env = util::Clamp01(t / 0.05f) * expf(-t * 4.f);
            float knock = t > 0.36f ? sinf(2 * PI * 120.f * t) * expf(-(t - 0.36f) * 40.f) : 0.f;
            return (lp * 3.0f * env + knock * 0.6f);
        });
    }
    // --- success fanfare (arpeggio) ---
    g_sounds[Success] = MakeSound(1.1f, [&](int, float t) {
        const float notes[4] = { 523.25f, 659.25f, 783.99f, 1046.5f };
        float s = 0;
        for (int i = 0; i < 4; i++) {
            float st = t - i * 0.11f;
            if (st > 0) s += sinf(2 * PI * notes[i] * st) * expf(-st * 3.5f) * 0.28f;
        }
        return s;
    });

    g_ready = true;
    SetMasterVolume(g_master);
}

void Shutdown() {
    if (!g_ready) return;
    for (auto& s : g_sounds) UnloadSound(s);
    for (auto& s : g_voice) UnloadSound(s);
    CloseAudioDevice();
    g_ready = false;
}

bool Ready() { return g_ready; }

void Play(Id id, float volume, float pitch) {
    if (!g_ready) return;
    Sound& s = g_sounds[id];
    SetSoundVolume(s, util::Clamp01(volume));
    SetSoundPitch(s, pitch);
    PlaySound(s);
}

void Voice(float pitch) {
    if (!g_ready || !g_voiceOn) return;
    Sound& s = g_voice[g_rng.I(0, 7)];
    SetSoundVolume(s, 0.55f);
    SetSoundPitch(s, pitch * g_rng.R(0.92f, 1.08f));
    PlaySound(s);
}

void SetMasterVolume(float v) { g_master = util::Clamp01(v); if (g_ready) ::SetMasterVolume(g_master); }
void SetVoiceEnabled(bool on) { g_voiceOn = on; }

} // namespace sfx
