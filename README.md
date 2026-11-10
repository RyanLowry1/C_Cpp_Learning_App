# Malloc's Library - 2D edition (V2)

*An interactive C & C++ study hall as a polished, hand-painted 2D illustration. Written in C++17 with raylib.*

This is a **complete, self-contained** copy of the app: the lecture content, the quizzes, the coding exams, the
test harness, raylib and the build script are all in this folder. Nothing outside it is used, so the folder can be
copied to another computer as it is.

You sit at a desk in an old library. **Mr. Malloc**, the librarian, sits opposite. The services are on cards around the
edges of the screen. Pick one and he talks you through your options, climbs out of his chair, walks to the bookshelves
behind him, pulls out the right book, carries it back and sets it on the desk. The camera swoops down onto the book,
it opens, and the book *is* your tutorial, quiz or coding exam. When you finish you hand it back and he comments on how you did.

| Service | What it does |
|---|---|
| **Reading Room** | 12 chapters from the lecture notes, with memory diagrams, tips, warnings and exam-focus notes |
| **Multiple Choice** | 171 questions (topic quizzes or Exam 1 / Exam 2 / C++ / Everything mixes) |
| **Written Test** | 85 predict-the-output, fill-in-the-blank, fix-the-bug and short answer questions |
| **Coding Exams** | 55 LeetCode-style problems in 3 workbooks, compiled and judged by real tests, plus a timed Mock Exam |
| **Problem Areas** | finds your weakest topic and drills the questions you keep missing |
| **Past Results** | a ledger of every session with a score trend |

---

## What makes this edition special

* **A painted, lit set.** The library is drawn entirely from vector shapes: about 700 books on the shelves, a wall clock
  that tells the real time, a tufted leather chair, a rug, a brass-and-green desk lamp, an inkwell with a quill, a service bell and a
  sleeping cat who wakes if you click him. Nothing is loaded from image files.
* **Real 2D lighting.** A small light map (ambient glow, the desk lamp that flickers, a soft light that follows the librarian)
  is multiplied over the whole scene, with drifting dust catching the lamp light.
* **Parallax.** Move the mouse and the wall, floor, chair and desk slide at different speeds, so the flat scene feels like a
  place you are looking around.
* **A puppet librarian.** Mr. Malloc is built from layered shapes and animated with poses and two-bone arms (inverse kinematics):
  he breathes, blinks, follows your mouse with his eyes and head, gestures while he talks, lip-syncs to his own voice,
  stands up, pushes his chair back, walks round the end of the desk, reaches for the right shelf, carries the book (the book
  is painted with its own cover), slides it across the desk and sits down again. He is hidden behind the desk or the chair
  correctly as he moves through the room.
* **A cinematic "pan down to the book".** The camera zooms and tilts onto the desk while the cover swings open across the spine
  and the page spread fades in - the pages are live interface, not a picture.
* **The same lessons.** Quizzes, written tests, tutorials and coding exams are identical to the other editions.

## Build and run

You need Windows 10/11 and **Visual Studio 2019 or later with "Desktop development with C++"** (or the Build Tools).
raylib is included in `lib/` and `include/`.

```bash
build.bat
```

```bash
MallocsLibrary_V2.exe
```

`build.bat debug` builds with a console window so you can see raylib's log. In VS Code, **Terminal > Run Build Task** does the same.

The prebuilt `MallocsLibrary_V2.exe` needs the Microsoft Visual C++ runtime (installed with Visual Studio, and with most
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
MallocsLibrary_V2.exe --selftest
```

Compiles and runs every model solution against its tests, checks every starter compiles, and runs every
"predict the output" question to prove its answer key is what the program really prints. Result: **201 / 201 checks passed.**
Add a word to test only some items, e.g. `--selftest e2.`

## How the code is organised

```
src/
  main.cpp          entry point and --selftest
  app.*             the director: phases (hub > fetching > presenting > reading > closing), choreography, camera zoom, HUD
  stage.*           the library set: shelves, floor, rug, chair, desk, props, light map, dust
  puppet.*          Mr. Malloc: poses, two-bone arms, walk cycle, face, lip-sync
  book2d.*          the book: painted cover, closed book, open spread with the swinging cover
  art.*             shape helpers (ellipses, polygons, glows, soft shadows, arm IK)
  ui.*              fonts (Georgia / Consolas from Windows), text layout, widgets and icons
  dialogue.*        speech box with typewriter text and choice cards
  audio.*           synthesised voice and effects (no audio files)
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
MallocsLibrary_V2.exe --script tools/shots_coding.txt
```
Scripts drive the real app (open a service, pick choices, answer, run code, turn pages) and save screenshots to
`save/shots/`. `shots_hub`, `shots_mcq`, `shots_written`, `shots_reading`, `shots_coding`, `shots_ledger` and `shots_misc`
cover every service.

## Related versions
* **V1** - the same app in 8-bit pixel art (low-res canvas, 40-colour palette, bitmap font).
* The 3D version (a first-person view with a real 3D librarian and book) lives in the main project folder.

All three share the same lessons, questions and coding exams, and the same marking.

raylib is (c) Ramon Santamaria, zlib licence (`lib/raylib-LICENSE.txt`).
