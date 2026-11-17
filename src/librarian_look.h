// librarian_look.h - what each librarian looks like: palette, features and the sculpted head and torso.
// Internal to the librarian_*.cpp files.
#pragma once
#include "common.h"

namespace look {

struct Spec {
    Color skin, skinShade, hair, brow, jacket, jacketDark, vest, shirt, accent, trousers, shoes, glasses, iris, lip;
    bool moustache, beard, hat, bowTie, chain, hood, lashes, labCoat;
    int glassesStyle;            // 0 none, 1 round, 2 half-moon, 3 rounded square, 4 large round
    float belly;                 // paunch 0..1
    float browSlope, browBulk;   // eyebrow character
};
Spec MakeSpec(const std::string& id);

// A polygonised sculpt plus a map of its front surface, so thin details can be laid on it.
struct Sculpt {
    std::vector<Mesh> meshes;
    std::vector<float> depth;
    float x0 = 0.f, x1 = 1.f, y0 = 0.f, y1 = 1.f;
    float Z(float x, float y) const;   // front surface at (x, y)
};

// Where the moving features (eyes, brows, mouth, glasses) sit on a sculpted head, in head space.
struct Face {
    float eyeX = 0.041f, eyeY = 0.131f, eyeZ = 0.08f, eyeR = 0.0215f;   // eyeball centre and radius
    float browY = 0.178f;
    float mouthY = 0.05f, mouthW = 0.03f, smile = 0.011f;
    float restOpen = 0.f;        // how far the mouth hangs open when silent (a standing grin)
    float lidOpen = 40.f;        // degrees: how high the upper eyelid rests
    float earX = 0.108f;         // where the arms of the glasses end
};
struct Head : Sculpt { Face face; };   // skin, hair, moustache and beard as one sculpt

const Head& GetHead(const std::string& id);                    // built the first time it is asked for
const Sculpt& GetTorso(const std::string& id, float muscle);   // rebuilt when the librarian's fitness moves a step
const Sculpt& GetHand(const std::string& id, int arm, bool grip);   // arm 0 = right, 1 = left; grip = closed round a dumbbell
void UnloadSculpts();

constexpr float kHeadScale = 1.28f;   // heads are drawn larger than life: a big head on a small body reads as friendly

// half the distance between the shoulder joints: narrower shoulders make a gentler figure
inline float ShoulderX(const std::string& id) { return id == "byte" ? 0.2f : (id == "ada" ? 0.14f : (id == "heap" ? 0.15f : 0.154f)); }

} // namespace look
