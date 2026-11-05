# Malloc's Library - 8-bit edition (V1)

*An interactive C & C++ study hall in chunky pixel art. Written in C++17 with raylib.*

This is a **complete, self-contained** copy of the app: the lecture content, the quizzes, the coding exams, the
test harness, raylib and the build script are all in this folder. Nothing outside it is used, so the folder can be
copied to another computer as it is.

You sit at a desk in an old library. **Mr. Malloc**, the librarian, sits opposite. The services are on cards around the
edges of the screen. Pick one and he talks you through your options, climbs out of his chair, walks to the bookshelves
behind him, pulls out the right book, carries it back and sets it on the desk. The view tips down onto the book, it
opens, and the book *is* your tutorial, quiz or coding exam. When you finish you hand it back and he comments on how you did.

| Service | What it does |
|---|---|
| **Reading Room** | 12 chapters from the lecture notes, with memory diagrams, tips, warnings and exam-focus notes |
| **Multiple Choice** | 171 questions (topic quizzes or Exam 1 / Exam 2 / C++ / Everything mixes) |
| **Written Test** | 85 predict-the-output, fill-in-the-blank, fix-the-bug and short-answer questions |
| **Coding Exams** | 55 LeetCode-style problems in 3 workbooks, compiled and judged by real tests, plus a timed Mock Exam |
| **Problem Areas** | finds your weakest topic and drills the questions you keep missing |
| **Past Results** | a ledger of every session with a score trend |

---

## What makes this edition 8-bit

Everything on screen goes through a small retro "display chip" (`src/pixel.cpp`):

* **A fixed 540-pixel-high canvas.** The whole game - the room, Mr. Malloc, the menus and the book pages - is drawn
  onto a tiny canvas, then scaled up. The canvas width follows your window shape.
* **A 40-colour palette with ordered dithering.** A shader maps every pixel to the nearest of 40 fixed colours and uses a
  Bayer matrix to dither between them. Lamp light, shadows and the fade into the book become dithered gradients,
  the way they were on old machines. Every colour the interface uses is in the palette, so flat panels stay perfectly flat.
* **"Sharp bilinear" upscaling.** The canvas is scaled to the window with hard pixel edges, whatever the window size,
  so pixels stay crisp without needing a whole-number zoom.
* **A built-in 5x7 bitmap font** (`src/pixfont.cpp`) magnified in whole steps, with faux bold and italic. Long lines of
  code wrap instead of shrinking, because a bitmap font cannot be scaled smoothly.
* **Pixel-snapped widgets.** Buttons and panels have notched corners and hard 1-pixel borders. Any line thinner than a
  pixel is widened to one pixel.
* **Chiptune sound.** Mr. Malloc "talks" in square-wave blips and every effect is crushed to a few amplitude levels.

## Build and run

You need Windows 10/11 and **Visual Studio 2019 or later with "Desktop development with C++"** (or the Build Tools).
raylib is included in `lib/` and `include/`.

```bash
build.bat
```

```bash
MallocsLibrary_V1.exe
```

`build.bat debug` builds with a console window so you can see raylib's log. In VS Code, **Terminal > Run Build Task** does the same.

The prebuilt `MallocsLibrary_V1.exe` needs the Microsoft Visual C++ runtime (installed with Visual Studio, and with most
games and apps). The **Coding Exams** compile your code with the Visual Studio C++ compiler (or g++ / clang if found),
so Run and Submit need one of those installed; without a compiler the books still open and the app says so.

### Controls
| Key | Action |
|---|---|
| Mouse | choose cards, answers and buttons; the view drifts slightly with the mouse |
| `Esc` | return the book / close a menu |
| `Space` or hold right mouse | hurry the librarian |
| `1-4` / `A-D` | answer multiple choice |
| `Left` / `Right` | turn pages |
| `Ctrl+Enter` / `Ctrl+Shift+Enter` | run / submit code |
| `F11` / `F12` | full screen / screenshot (saved to `save/shots/`) |

Click Mr. Malloc, the bell on the desk or the cat for a surprise. Progress is saved in `save/` next to the program.

## Checking the content: `--selftest`

```bash
MallocsLibrary_V1.exe --selftest
```

Compiles and runs every model solution against its tests, checks every starter compiles, and runs every
"predict the output" question to prove its answer key is what the program really prints. Result: **201 / 201 checks passed.**
Add a word to test only some items, e.g. `--selftest e2.`

## How the code is organised

```
src/
  main.cpp          entry point and --selftest
  app.*             the director: phases (hub > fetching > presenting > reading > closing), choreography, HUD
  stage.*           the library set: shelves, floor, rug, chair, desk, props, lamp light map
  puppet.*          Mr. Malloc: poses, two-bone arms, walk cycle, face, lip-sync
  book2d.*          the book: painted cover, closed and open views, page-flip
  art.*             shape helpers (ellipses, polygons, glows, soft shadows, arm IK)
  pixel.*           the 8-bit display chip: canvas, palette + dither shader, upscale
  pixfont.*         the 5x7 bitmap font
  ui.*              text layout, widgets and icons (all pixel-snapped)
  dialogue.*        speech box with typewriter text and choice cards
  audio.*           synthesised chiptune voice and effects (no audio files)
  content.*         loads questions, chapters and problems from assets/content
  progress.*        save file, per-item history and mastery per topic
  harness.*, code_runner.*   build and run the student's code against tests with time limits
  syntax.*, code_editor.*    highlighting and the in-book code editor
  views/            the books: reading, multiple choice, written, coding, ledger
assets/content/     all learning content as plain text (format: assets/content/README.txt)
tools/              autopilot scripts that drive the app to record demos and screenshots
```

### Recording a demo
```bash
MallocsLibrary_V1.exe --script tools/shots_coding.txt
```
Scripts drive the real app (open a service, pick choices, answer, run code, turn pages) and save screenshots to
`save/shots/`. `shots_hub`, `shots_mcq`, `shots_written`, `shots_reading`, `shots_coding`, `shots_ledger` and `shots_misc`
cover every service.

## Related versions
* **V2** - the same app drawn as a polished, smooth 2D illustration.
* The 3D version (a first-person view with a real 3D librarian and book) lives in the main project folder.

All three share the same lessons, questions and coding exams, and the same marking.

raylib is (c) Ramon Santamaria, zlib licence (`lib/raylib-LICENSE.txt`).
