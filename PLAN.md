# plasma-swipe-keyboard — Implementation Plan

A Qt Virtual Keyboard input method plugin that adds **swipe/gesture typing** to KDE Plasma's
on-screen keyboard. Long term goal: upstream into `KDE/plasma-keyboard`. Short term: a standalone
plugin that loads alongside the existing keyboard.

---

## Why this works as a plugin

`plasma-keyboard` (KDE 6.6, Feb 2026) is a thin wrapper around **Qt Virtual Keyboard** (Qt VKB).
Qt VKB already exposes everything needed for swipe input:

- `QVirtualKeyboardTrace` — captures the raw `(x, y)` touch path
- `TraceInputKey` (QML) — a `MultiPointTouchArea` that calls `traceBegin()` / `traceEnd()`
- `TraceCanvas` — renders the swipe trail visually
- `QVirtualKeyboardAbstractInputMethod::traceEnd()` — virtual hook that receives the finished trace
- `wordCandidateListModel` — the candidate-bar UI is already wired up; we just feed it strings

What's missing is purely the **path → word** algorithm. Qt VKB ships only directional swipe
detection (left = backspace, etc.) and commercial-only handwriting recognizers (T9Write, MyScript).

Plugins are discovered automatically from `/usr/lib64/qt6/qml/QtQuick/VirtualKeyboard/Plugins/<Name>/`.
Reference: `Hunspell/`, `Pinyin/`, `Thai/` are already installed on this system.

---

## Environment (verified working as of 2026-04-28)

- Fedora 44 KDE, Plasma 6.x
- Qt 6.10.3 (`qt6-qtbase-devel`, `qt6-qtdeclarative-devel`, `qt6-qtvirtualkeyboard-devel`, `qt6-qtwayland-devel`)
- CMake 4.3.0, Ninja 1.13.2, GCC 16.0.1
- Qt plugin path: `/usr/lib64/qt6/plugins`
- Qt QML path: `/usr/lib64/qt6/qml`
- Reference repos cloned at:
  - `~/Projects/plasma-keyboard/` — KDE plasma-keyboard source (read-only reference)
  - `~/Projects/qtvirtualkeyboard/` — Qt VirtualKeyboard source (read-only reference)
- This project lives at `~/Projects/plasma-swipe-keyboard/`

Settings.json has `Bash(*)` allowed with `rm`/`rmdir` denied — no permission prompts during dev.

---

## Architecture

```
  ┌───────────────────── plasma-keyboard (unchanged) ─────────────────────┐
  │                                                                       │
  │    user finger swipes across keys                                     │
  │              │                                                        │
  │              ▼                                                        │
  │    TraceInputKey (QML, in our layout)                                 │
  │              │                                                        │
  │              ▼ traceBegin() / addPoint() × N / traceEnd()             │
  │    QVirtualKeyboardInputEngine                                        │
  │              │                                                        │
  └──────────────┼────────────────────────────────────────────────────────┘
                 ▼
  ┌──────────── plasma-swipe-keyboard plugin (this repo) ────────────────┐
  │                                                                      │
  │    SwipeInputMethod : QVirtualKeyboardAbstractInputMethod            │
  │      ├── traceBegin()  → store new QVirtualKeyboardTrace             │
  │      └── traceEnd()    → run matcher, push candidates                │
  │                                                                      │
  │    WordMatcher                                                       │
  │      ├── KeyboardLayout: QChar → (x, y) center                       │
  │      ├── Templates: word → resampled ideal trace                     │
  │      └── score(userTrace) → top-N (word, score) by SHARK²-style DTW  │
  │                                                                      │
  │    Dictionary (frequency-ranked wordlist)                            │
  │                                                                      │
  └──────────────────────────────────────────────────────────────────────┘
                 │
                 ▼ wordCandidateListModel
        candidate bar UI (already in Qt VKB)
                 │
                 ▼ user taps choice or hits space
        commitText() to focused widget
```

---

## Phase 0 — Project skeleton (build a do-nothing plugin that loads)

**Goal:** plugin compiles, installs, is loaded by Qt VKB, prints a debug line on `traceEnd()`.
This is the most important phase because it proves the integration works before any algorithm work.

**Files to create in `~/Projects/plasma-swipe-keyboard/`:**

```
CMakeLists.txt
src/
  swipeinputmethod.h
  swipeinputmethod.cpp
  swipeplugin.cpp           # Q_IMPORT_QML_PLUGIN registration
qml/
  layouts/
    en_US/
      swipe.qml             # KeyboardLayout that uses TraceInputKey + createInputMethod()
qmldir                      # at root, declares the plugin module URI
```

**Plugin URI:** `Plasma.SwipeKeyboard` (avoid `QtQuick.VirtualKeyboard.Plugins.*` since that's
Qt-internal namespace).

**Reference for CMake:** `~/Projects/qtvirtualkeyboard/src/plugins/example/hwr/CMakeLists.txt` —
but **do not copy verbatim**. That file uses `qt_internal_add_qml_module` which only works inside
Qt's source tree. For a standalone plugin use `qt_add_qml_module` (public API).

Skeleton:

```cmake
cmake_minimum_required(VERSION 3.21)
project(plasma-swipe-keyboard VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(Qt6 6.10 REQUIRED COMPONENTS Core Gui Qml Quick VirtualKeyboard)
qt_standard_project_setup(REQUIRES 6.10)

qt_add_qml_module(plasma_swipe_keyboard
    URI Plasma.SwipeKeyboard
    VERSION 1.0
    PLUGIN_TARGET plasma_swipe_keyboard
    SOURCES
        src/swipeinputmethod.h
        src/swipeinputmethod.cpp
    QML_FILES
        qml/layouts/en_US/swipe.qml
)
target_link_libraries(plasma_swipe_keyboard PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::VirtualKeyboard
)
```

**`SwipeInputMethod` minimum viable subclass** — must override the four pure virtuals and
`traceBegin()` / `traceEnd()`:

```cpp
QList<InputMode> inputModes(const QString &) override { return { InputMode::Latin }; }
bool setInputMode(const QString &, InputMode) override { return true; }
bool setTextCase(TextCase) override { return true; }
bool keyEvent(Qt::Key, const QString &text, Qt::KeyboardModifiers) override {
    inputContext()->sendKeyClick(...);  // pass through normal key taps
    return true;
}
QList<PatternRecognitionMode> patternRecognitionModes() const override {
    return { PatternRecognitionMode::Handwriting };
}
QVirtualKeyboardTrace *traceBegin(...) override { /* store and return */ }
bool traceEnd(QVirtualKeyboardTrace *trace) override {
    qDebug() << "Got trace with" << trace->length() << "points";
    return true;
}
```

**QML layout `qml/layouts/en_US/swipe.qml`** — needs a `createInputMethod()` function that returns
our `SwipeInputMethod`:

```qml
function createInputMethod() {
    return Qt.createQmlObject(
        'import QtQuick; import Plasma.SwipeKeyboard; SwipeInputMethod {}', parent);
}
```

Reference: `~/Projects/qtvirtualkeyboard/src/layouts/fallback/handwriting.qml`.

**Install target:** `${QT_INSTALL_QML}/Plasma/SwipeKeyboard/`. For development don't install —
use `QML_IMPORT_PATH` and `QT_PLUGIN_PATH` env vars pointing at the build dir.

**Verification:**
1. Build: `cmake -B build -G Ninja && cmake --build build`
2. Run a Qt VKB demo app with our build dir on the import path
3. Switch to our keyboard layout and swipe — confirm `qDebug()` fires

---

## Phase 1 — Trace inspection (sanity check)

Before any algorithm, dump the raw trace data so we can see what we're working with.

In `traceEnd()`, log:
- Total point count
- Bounding box of the trace
- The list of `(x, y)` points
- DPI from `traceCaptureDeviceInfo`
- Screen geometry from `traceScreenInfo`

This tells us the coordinate space (likely keyboard-local in pixels, but verify).

---

## Phase 2 — Hardcoded QWERTY layout + tiny matcher

**Goal:** type 5 specific words by swiping, with a hardcoded 50-word dictionary.

### 2a. Key position table

Create `KeyboardLayout` class with hardcoded QWERTY centers as **normalized** coordinates
(`[0, 1] × [0, 1]`), then scale to the real keyboard size at runtime using `traceScreenInfo`
or the `TraceInputKey` size.

```
Row 0 (y=0.166):  q w e r t y u i o p   (x = 0.05, 0.15, ..., 0.95)
Row 1 (y=0.500):  a s d f g h j k l     (x = 0.10, 0.20, ..., 0.90)
Row 2 (y=0.833):  z x c v b n m         (x = 0.20, 0.30, ..., 0.80)
```

### 2b. Trace normalization

Normalize both user trace and template traces before comparison:
1. Convert raw points to normalized `[0, 1]` coordinates (divide by keyboard width/height)
2. Resample to fixed N=64 equidistant points using arc-length parameterization
3. Optionally: subtract centroid + scale to unit bounding box (this loses absolute key location;
   the SHARK² algorithm keeps location for a reason — start with location-aware, no centering)

### 2c. Template generation

For each word in dictionary:
1. Map each character to its key center: `"hello"` → `[(h.x, h.y), (e.x, e.y), (l.x, l.y), (l.x, l.y), (o.x, o.y)]`
2. Collapse consecutive duplicate positions (double letters): `(l.x, l.y), (l.x, l.y)` → one point
   (the swipe doesn't pause for repeated letters)
3. Resample to N=64 points along the polyline

Cache templates on first use.

### 2d. Matching algorithm (SHARK² simplified)

For each candidate word, compute three sub-scores:

1. **Location score:** mean Euclidean distance between resampled user trace and template,
   point-by-point (since both have N=64 points, this is straightforward — no DTW needed for
   the simplified version).

2. **Shape score:** same as location score but after centering+scaling both traces. This
   captures gesture *shape* independent of position. Helps when user starts the swipe slightly
   off the first key.

3. **Length score:** `|len(user) - len(template)| / len(template)` — penalizes very short
   traces matching long words and vice versa.

Combined score: `α * location + β * shape + γ * length - δ * log(frequency + 1)`.
Start with `α=1.0, β=0.5, γ=0.3, δ=0.1` and tune by hand.

Lower is better. Return top 5 candidates.

For **first pass** skip DTW entirely — point-to-point with both resampled to N=64 works
surprisingly well and is much simpler. Add DTW later if quality plateaus.

### 2e. Candidate emission

In `traceEnd()`:
```cpp
m_wordCandidates = matcher.match(trace);  // QStringList of top 5
m_activeWordIndex = 0;
inputContext()->setPreeditText(m_wordCandidates.first());
emit selectionListChanged(WordCandidateList);
emit selectionListActiveItemChanged(WordCandidateList, 0);
```

Then implement the `selectionList*` virtuals so the candidate bar shows our list.
Reference: end of `~/Projects/qtvirtualkeyboard/src/plugins/example/hwr/examplehwrinputmethod.cpp`.

On space or candidate tap → `commitText()`, clear preedit.

### 2f. 50-word seed dictionary

Hardcode in C++: the 50 most common English words (`the`, `of`, `to`, `and`, `a`, `in`, `is`,
`it`, `you`, `that`, ...) with frequency = rank inverted. Enough to verify end-to-end before
loading a real wordlist.

---

## Phase 3 — Real dictionary

Switch to a frequency-ranked wordlist. Recommended sources:

1. **SymSpell `frequency_dictionary_en_82_765.txt`** — 82k words, real frequencies from Google
   Web Trillion Word Corpus. Format: `word freq` per line. License: MIT.
   <https://github.com/wolfgarbe/SymSpell/blob/master/SymSpell/frequency_dictionary_en_82_765.txt>

2. **Peter Norvig `count_1w.txt`** — 333k words, Google web counts. Public domain.
   <https://norvig.com/ngrams/count_1w.txt>

Start with SymSpell (smaller, better-curated). Bundle as a Qt resource (`.qrc`).

Filter at load time: lowercase only, length 2–20, ASCII a-z.

For the candidate bar, pre-filter by length: only score words with length within
`±2` of the number of "corners" in the trace (estimated from direction changes). Saves
~95% of the matcher work.

---

## Phase 4 — Tap handling and polish

When the trace is very short (e.g., < 8mm or < 5 points), treat it as a tap on the nearest key
center, not a swipe. This is what the existing keyboard already does for non-swipe input — we
just need to detect "this trace is actually a tap" before running the word matcher.

Refinement passes (do these in order, stop when quality is acceptable):
1. Lookup-table for common bigrams to bias matching (e.g., "th" is common, "tg" isn't)
2. Velocity-aware sampling (people slow down at letter corners) — Bezier-corner detection
3. Real DTW instead of fixed-N point-to-point
4. Personal frequency learning (track which candidates the user picks)

---

## Phase 5 — Layout polish and floating window

This is what the user actually wants for tablet mode: a small, repositionable keyboard near
the right edge of the screen for thumb swipe-typing.

The QML layout is purely cosmetic from the algorithm's perspective. Two pieces of work:

1. **Visual layout** — show key labels in a QWERTY grid; overlay a transparent `TraceInputKey`
   covering the whole grid. Reference visual style: `BreezeKeyPanel.qml` in plasma-keyboard.

2. **Window sizing** — plasma-keyboard's `inputpanelwindow.cpp` controls window geometry. For
   a phase-1 floating mode, use KWin's "Special Window Settings" (manual config) before
   touching plasma-keyboard. Touching the window code is upstream territory — leave it for
   the upstream PR.

---

## Phase 6 — Upstream

Once standalone plugin is working well:

1. Open a KDE Discuss thread with a demo video to gauge interest from plasma-keyboard maintainers.
2. Migration is straightforward — the plugin is already a `qt_add_qml_module`. Move
   `src/` and `qml/` into plasma-keyboard's tree, switch to `qt_internal_add_qml_module`,
   add to `src/CMakeLists.txt`.
3. Discuss the floating-window mode separately — that's a UX-policy decision with broader
   implications (screen reader, touch-target sizing, accessibility settings).

KDE Frameworks contribution guide: <https://community.kde.org/Get_Involved/development>.

---

## Build / test workflow (use throughout all phases)

```bash
cd ~/Projects/plasma-swipe-keyboard
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Run against a Qt VKB demo (no install needed)
export QML_IMPORT_PATH=$PWD/build:$QML_IMPORT_PATH
export QT_PLUGIN_PATH=$PWD/build:$QT_PLUGIN_PATH
export QT_IM_MODULE=qtvirtualkeyboard
export QT_LOGGING_RULES="qt.virtualkeyboard.*=true;plasma.swipe.*=true"

# Test 1: Qt's built-in demo
qt6-virtualkeyboard-example   # if installed; otherwise build from ~/Projects/qtvirtualkeyboard/examples

# Test 2: any Qt6 app with a text field
qdbusviewer6   # has text fields, tap to bring up keyboard
```

Don't try to test in the actual plasma-keyboard until Phase 0 works against a demo app.
plasma-keyboard runs as a Wayland input method which is harder to debug — get the algorithm
right against a regular app first.

---

## Reference: key files in cloned repos

**plasma-keyboard (already cloned at `~/Projects/plasma-keyboard/`):**
- `src/main.cpp` — app entry point, registers QML types
- `src/inputlisteneritem.cpp` — touch event capture
- `src/qml/main.qml` — wraps Qt VKB's `InputPanel`
- `src/layouts/fallback/main.qml` — example QWERTY KeyboardLayout
- `src/qmlplugin/BreezeKeyPanel.qml` — Breeze-styled key visual

**Qt VKB (already cloned at `~/Projects/qtvirtualkeyboard/`):**
- `src/virtualkeyboard/qvirtualkeyboardabstractinputmethod.h` — base class we subclass
- `src/virtualkeyboard/qvirtualkeyboardtrace.h` — trace data API
- `src/virtualkeyboard/qvirtualkeyboardinputengine.h` — InputMode + PatternRecognitionMode enums
- `src/plugins/example/hwr/examplehwrinputmethod.cpp` — best reference for plugin shape;
  **read this carefully before starting Phase 0** (especially `traceBegin`/`traceEnd` logic
  and the `selectionList*` overrides for candidate bar integration)
- `src/layouts/fallback/handwriting.qml` — minimal layout with `TraceInputKey` and
  `createInputMethod()`
- `src/components/TraceInputKey.qml` — the touch capture component
- `src/styles/TraceCanvas.qml` — visualises trace as you draw (we get this for free)

**Don't read every file** — read just what's needed when you need it. The example HWR plugin is
the single most important reference.

---

## Open questions (defer until Phase 2)

1. Coordinate space of `Trace::points()` — pixels relative to `TraceInputKey`? Pixels relative
   to keyboard window? Or DPI-independent millimeters? Phase 1 logging will answer this.
2. Should we support multi-stroke traces (lifting and re-pressing)? Phase 1: no — discard.
3. How do we detect end-of-word? Currently the trace ends when finger lifts. Good enough.
4. Capitalization, punctuation, numbers — out of scope for v1. User taps for those.
