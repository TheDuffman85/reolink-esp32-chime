#pragma once
#include <cstdint>

namespace chime {
// V2's playback table contains ten entries (music ID 0..9). The vendor
// handler also admits 10, but that indexes the following pointer, not a tune.
constexpr uint8_t DoorbellSounds = 10;
constexpr uint8_t Melodies = DoorbellSounds, Patterns = DoorbellSounds;

// Select per ring: the doorbell can use different sounds for different events.
// Do not overwrite local defaults or write flash on every ring. Validate the
// full u16 before narrowing; an unknown ID must never wrap to a known sound.
inline bool doorbellSound(uint16_t id, uint8_t &melody, uint8_t &pattern) {
  if (id >= DoorbellSounds) return false;
  melody = pattern = uint8_t(id);
  return true;
}

// Synthesized alternatives, not recovered Reolink recordings. Keep the first
// three local sounds unchanged so existing saved choices retain their sound.
struct Note { uint16_t hz, ms; };
constexpr uint8_t MaxNotes = 8;
inline const Note *melodyScore(uint8_t id) {
  static const Note scores[Melodies][MaxNotes] = {
    {{784,240},{659,520}},
    {{523,180},{659,180},{784,180},{1047,400}},
    {{880,180},{0,100},{880,180},{0,100}},
    {{1047,180},{784,180},{659,180},{523,400}},
    {{880,150},{0,100},{880,150},{0,100},{880,300}},
    {{659,240},{523,240},{587,240},{392,600}},
    {{523,250},{0,80},{659,250},{0,80},{784,550}},
    {{784,250},{0,80},{659,250},{0,80},{523,550}},
    {{659,200},{784,200},{659,200},{784,200},{659,400}},
    {{1047,120},{0,100},{784,120},{0,100},{1047,120},{0,100},{784,400}}
  };
  return id < Melodies ? scores[id] : nullptr;
}

struct StrikeScore {
  uint8_t count = 1;
  uint16_t onMs = 300;
  uint16_t gaps[5]{};
};
inline StrikeScore strikeScore(uint8_t id, uint16_t pulse, uint16_t gap) {
  StrikeScore score;
  score.onMs = pulse;
  for (auto &g : score.gaps) g = gap;
  switch (id) {
    case 1: score.count = 2; break;
    case 2: score.count = 3; break;
    case 3: score.onMs = uint16_t(uint32_t(pulse) * 4 > 3000 ? 3000 : pulse * 4); break;
    case 4:
      score.count = 5; score.onMs = pulse / 2 < 50 ? 50 : pulse / 2;
      for (auto &g : score.gaps) g = gap / 2 < 100 ? 100 : gap / 2;
      break;
    case 5: case 6:
      score.count = id == 5 ? 2 : 3;
      for (auto &g : score.gaps) g = uint16_t(uint32_t(gap) * 2 > 3000 ? 3000 : gap * 2);
      break;
    case 7: score.count = 4; break;
    case 8: case 9:
      score.count = id == 8 ? 4 : 6;
      for (uint8_t i = 0; i < 5; ++i)
        score.gaps[i] = i % 2 ? gap : (gap / 2 < 100 ? 100 : gap / 2);
      break;
    default: break;
  }
  return score;
}
}
