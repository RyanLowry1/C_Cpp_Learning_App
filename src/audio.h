// audio.h - procedurally synthesised sound effects (no external audio files needed)
#pragma once
#include "common.h"

namespace sfx {

enum Id { Footstep, Thud, Page, Correct, Wrong, Click, Hover, Bell, Open, Drawer, Success, Count };

void Init();
void Shutdown();
void Play(Id id, float volume = 1.f, float pitch = 1.f);
void Voice(float pitch = 1.f);         // one syllable of the librarian's "mumble" voice
void SetMasterVolume(float v);
void SetVoiceEnabled(bool on);
bool Ready();

} // namespace sfx
