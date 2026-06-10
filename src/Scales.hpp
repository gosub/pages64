#pragma once

// ── Shared scale math ─────────────────────────────────────────────────────────
// Used by the 64Notes / 8Notes companion modules (and any future page module
// that needs to map degrees to semitones).

namespace P64 {

struct Scale {
    const char* name;
    int size;
    int deg[12];
};

static const Scale SCALES[] = {
    {"Major",            7, {0, 2, 4, 5, 7, 9, 11}},
    {"Natural minor",    7, {0, 2, 3, 5, 7, 8, 10}},
    {"Harmonic minor",   7, {0, 2, 3, 5, 7, 8, 11}},
    {"Dorian",           7, {0, 2, 3, 5, 7, 9, 10}},
    {"Phrygian",         7, {0, 1, 3, 5, 7, 8, 10}},
    {"Lydian",           7, {0, 2, 4, 6, 7, 9, 11}},
    {"Mixolydian",       7, {0, 2, 4, 5, 7, 9, 10}},
    {"Major pentatonic", 5, {0, 2, 4, 7, 9}},
    {"Minor pentatonic", 5, {0, 3, 5, 7, 10}},
    {"Blues",            6, {0, 3, 5, 6, 7, 10}},
    {"Whole tone",       6, {0, 2, 4, 6, 8, 10}},
    {"Chromatic",       12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
};
static constexpr int NUM_SCALES = sizeof(SCALES) / sizeof(SCALES[0]);

// Scale degree (any integer; wraps octaves, handles negatives) → semitone
// offset from the root.
inline int degreeToSemitone(const Scale& s, int d) {
    int oct = d / s.size;
    int idx = d % s.size;
    if (idx < 0) {
        idx += s.size;
        oct -= 1;
    }
    return oct * 12 + s.deg[idx];
}

static const char* NOTE_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

} // namespace P64
