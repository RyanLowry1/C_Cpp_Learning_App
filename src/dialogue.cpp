// dialogue.cpp - speech box, typewriter effect, lip-sync and choice cards
#include "dialogue.h"
#include "audio.h"

using namespace ui;

void Dialogue::Say(const std::string& text) {
    current = text;
    shown = 0;
    charTimer = 0;
    queue.clear();
    choices.clear();
    onPick = nullptr;
    visible = true;
    idle = 0;
}

void Dialogue::Queue(const std::string& text) {
    if (!visible || current.empty()) { Say(text); return; }
    queue.push_back(text);
}

void Dialogue::Ask(const std::string& text, const std::vector<Choice>& ch, std::function<void(int)> cb, int cols, bool cancel) {
    Say(text);
    choices = ch;
    onPick = cb;
    columns = cols;
    allowCancel = cancel;
    choiceAppear = 0;
}

void Dialogue::Clear() {
    current.clear();
    queue.clear();
    choices.clear();
    onPick = nullptr;
    visible = false;
    shown = 0;
}

void Dialogue::Advance() {
    if (Typing()) { shown = (int)current.size(); return; }
    if (!queue.empty()) {
        current = queue.front();
        queue.erase(queue.begin());
        shown = 0;
        charTimer = 0;
    }
}

void Dialogue::Update(float dt) {
    appear = util::Damp(appear, visible ? 1.f : 0.f, 12.f, dt);
    if (!visible) { mouth = util::Damp(mouth, 0, 20, dt); return; }
    if (Typing()) {
        charTimer += dt;
        const float cps = 52.f;
        while (charTimer > 1.f / cps && shown < (int)current.size()) {
            charTimer -= 1.f / cps;
            char c = current[(size_t)shown];
            shown++;
            if (c == '.' || c == '!' || c == '?') charTimer -= 0.16f;
            else if (c == ',' || c == ';' || c == ':') charTimer -= 0.07f;
            bool vowel = std::string("aeiouAEIOU").find(c) != std::string::npos;
            if (isalpha((unsigned char)c)) {
                mouthTarget = vowel ? 1.f : 0.45f;
                if (++blipCounter % 3 == 0) sfx::Voice(vowel ? 1.0f : 0.94f);
            } else mouthTarget = 0.f;
        }
        idle = 0;
    } else {
        mouthTarget = 0.f;
        idle += dt;
        // auto-advance queued lines after a pause
        if (!queue.empty() && idle > 2.2f) Advance();
    }
    mouth = util::Damp(mouth, mouthTarget, 25.f, dt);
    if (ChoicesShowing()) choiceAppear = util::Damp(choiceAppear, 1.f, 10.f, dt);
}

void Dialogue::Draw(float W, float H, bool interactive) {
    if (appear < 0.01f) return;
    float a = appear;
    float bw = std::min(980.f, W - 640.f), bh = 158.f;
    if (bw < 640.f) bw = std::min(W - 60.f, 760.f);
    Rectangle box{ (W - bw) * 0.5f, H - bh - 26.f + (1.f - a) * 40.f, bw, bh };

    // ----- choice cards -----
    if (ChoicesShowing() && choiceAppear > 0.01f) {
        int n = (int)choices.size();
        int cols = std::min(columns, std::max(1, n));
        float cw = cols >= 4 ? 238.f : (cols == 3 ? 290.f : 360.f);
        float ch = 74.f, gap = 12.f;
        int rows = (n + cols - 1) / cols;
        float totalW = cols * cw + (cols - 1) * gap;
        float totalH = rows * ch + (rows - 1) * gap;
        float x0 = (W - totalW) * 0.5f, y0 = box.y - totalH - 22.f;
        if (y0 < 70.f) y0 = 70.f;
        DrawRectangleRounded(Rectangle{ x0 - 18, y0 - 16, totalW + 36, totalH + 32 }, 0.06f, 8, util::WithAlpha(BLACK, 0.35f * choiceAppear));
        for (int i = 0; i < n; i++) {
            const Choice& c = choices[(size_t)i];
            int r = i / cols, col = i % cols;
            // centre the last row
            int inRow = (r == rows - 1) ? n - r * cols : cols;
            float rowW = inRow * cw + (inRow - 1) * gap;
            float rx = (W - rowW) * 0.5f + col * (cw + gap);
            float t = util::Clamp01(choiceAppear * 1.6f - i * 0.05f);
            Rectangle card{ rx, y0 + r * (ch + gap) + (1 - t) * 16.f, cw, ch };
            bool hov = interactive && Hover(card);
            if (hov) { card.y -= 3; WantHand(); }
            DrawRectangleRounded(Rectangle{ card.x + 3, card.y + 4, card.width, card.height }, 0.14f, 6, util::WithAlpha(BLACK, 0.3f * t));
            Panel(card, util::WithAlpha(hov ? util::Shade(pal::Paper, 1.04f) : pal::Paper, t), util::WithAlpha(pal::Wood, t), 0.14f, 2.f);
            DrawRectangleRounded(Rectangle{ card.x, card.y, 10, card.height }, 0.6f, 4, util::WithAlpha(c.accent, t));
            float tx = card.x + 58;
            float maxW = card.width - 70;
            float labelW = card.x + card.width - 10 - tx;
            if (!c.badge.empty()) {
                // badge sits under the icon so the title keeps the full width
                DrawIcon(c.icon, Vector2{ card.x + 34, card.y + 27 }, 24, util::WithAlpha(c.accent, t));
                float bwid = std::min(44.f, MeasureWidth(FontId::SerifBold, c.badge, 11) + 10);
                Rectangle br{ card.x + 34 - bwid * 0.5f, card.y + ch - 26, bwid, 17 };
                DrawRectangleRounded(br, 0.5f, 4, util::WithAlpha(c.accent, 0.85f * t));
                TextCentered(FontId::SerifBold, c.badge, br, 11, util::WithAlpha(pal::Paper, t));
            } else {
                DrawIcon(c.icon, Vector2{ card.x + 34, card.y + ch * 0.5f }, 28, util::WithAlpha(c.accent, t));
            }
            float fs = 21.f;
            while (fs > 14 && MeasureWidth(FontId::SerifBold, c.label, fs) > labelW) fs -= 1;
            std::string label = Ellipsize(FontId::SerifBold, c.label, fs, labelW);
            Text(FontId::SerifBold, label, tx, card.y + 11 + (21.f - fs) * 0.5f, fs, util::WithAlpha(pal::Ink, t));
            if (!c.sub.empty()) Text(FontId::SerifItalic, Ellipsize(FontId::SerifItalic, c.sub, 15, card.x + card.width - 10 - tx), tx, card.y + 38, 15, util::WithAlpha(pal::InkSoft, t));
            if (c.mastery >= 0) {
                ProgressBar(Rectangle{ tx, card.y + ch - 14, maxW - 46, 6 }, c.mastery, util::Mix(pal::Red, pal::Green, c.mastery), util::WithAlpha(pal::PaperEdge, t));
                Text(FontId::Serif, TextFormat("%d%%", (int)(c.mastery * 100 + 0.5f)), tx + maxW - 40, card.y + ch - 21, 13, util::WithAlpha(pal::InkSoft, t));
            }
            if (interactive && Pressed(card)) {
                sfx::Play(sfx::Click);
                auto cb = onPick;
                choices.clear();
                onPick = nullptr;
                if (cb) cb(i);
                return;
            }
        }
        if (allowCancel) {
            Rectangle cancel{ W * 0.5f - 80, box.y - 18 - 4, 160, 0 };
            (void)cancel;
        }
    }

    // ----- speech box -----
    DrawRectangleRounded(Rectangle{ box.x + 4, box.y + 6, box.width, box.height }, 0.12f, 8, util::WithAlpha(BLACK, 0.35f * a));
    Panel(box, util::WithAlpha(pal::Paper, 0.97f * a), util::WithAlpha(pal::Wood, a), 0.12f, 3.f);
    DrawRectangleRoundedLinesEx(Rectangle{ box.x + 7, box.y + 7, box.width - 14, box.height - 14 }, 0.12f, 8, 1.f, util::WithAlpha(pal::Gold, 0.7f * a));
    // name plate
    float nw = MeasureWidth(FontId::SerifBold, speaker, 20) + 36;
    Rectangle np{ box.x + 26, box.y - 20, nw, 34 };
    Panel(np, util::WithAlpha(pal::Wood, a), util::WithAlpha(pal::Gold, a), 0.4f, 2.f);
    TextCentered(FontId::SerifBold, speaker, np, 20, util::WithAlpha(pal::GoldSoft, a));
    // text
    std::string vis = current.substr(0, (size_t)std::min<int>(shown, (int)current.size()));
    Rich(vis, box.x + 30, box.y + 26, box.width - 60, 23, util::WithAlpha(pal::Ink, a), 1.42f);
    // continue / skip affordance
    bool canAdvance = Typing() || !queue.empty();
    if (canAdvance && !Typing()) {
        float bob = sinf((float)GetTime() * 5.f) * 3.f;
        DrawTriangle(Vector2{ box.x + box.width - 38, box.y + box.height - 28 + bob }, Vector2{ box.x + box.width - 22, box.y + box.height - 28 + bob },
                     Vector2{ box.x + box.width - 30, box.y + box.height - 18 + bob }, util::WithAlpha(pal::Wood, a));
    }
    if (HasChoices() && allowCancel && !Typing()) {
        Rectangle nb{ box.x + box.width - 150, box.y + box.height - 40, 128, 28 };
        if (interactive && Button(nb, "Never mind", BtnLink, true, 18)) {
            auto cb = onPick;
            choices.clear();
            onPick = nullptr;
            if (cb) cb(-1);
            return;
        }
    }
    if (interactive && canAdvance && Pressed(box)) Advance();
}
