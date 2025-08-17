#ifndef EMOTION_ENGINE_H
#define EMOTION_ENGINE_H

#include <Arduino.h>
#include "Types.h"
#include "IMoodTarget.h"

struct EmotionVec { int8_t valence; int8_t arousal; };

class EmotionEngine {
public:
  explicit EmotionEngine(IMoodTarget& tgt) : target(tgt) {}

  void begin(uint32_t nowMs);
  void setRandomAdvance(bool en){ randomAdvance = en; }    // kept for console compatibility
  bool isAutoAdvanceEnabled() const { return randomAdvance; }

  void setPatternPenalty(uint8_t p){ patternPenalty = p; }
  uint8_t getPatternPenalty() const { return patternPenalty; }

  // External inputs
  void setExternalBias(uint8_t arousalBias, uint8_t valenceBias, bool valid);
  void setStartleBoost(uint8_t strength, uint16_t ms);

  // Pick & set a new mood
  void operatorNext(uint32_t nowMs);

private:
  IMoodTarget& target;
  bool randomAdvance = true;
  uint16_t rng = 0xBEEF;

  static constexpr uint8_t HIST_N = 6;
  uint8_t history[HIST_N] = {255,255,255,255,255,255};
  uint8_t historyIdx = 0;

  uint8_t patternPenalty = 120;

  // External bias (smoothed)
  uint8_t extArousal = 128;  // 0..255 (128 = neutral)
  uint8_t extValence = 128;  // 0..255 (128 = neutral)
  bool    extBiasValid = false;

  // helpers
  uint8_t currentIdx() const { return target.currentMoodIndex(); }
  void pushHistory(uint8_t idx);
  uint16_t urand();
  uint8_t pickWeighted(uint16_t* w, uint8_t count);
  EmotionVec moodVec(uint8_t i) const;
  uint16_t baseWeight(uint8_t toIdx) const;
  uint16_t adjacencyWeight(uint8_t fromIdx, uint8_t toIdx) const;
  uint16_t recencyWeight(uint8_t toIdx) const;
  uint16_t patternRecencyWeight(uint8_t fromIdx, uint8_t toIdx) const;
  uint16_t biasWeight(uint8_t toIdx) const;

  uint32_t startleUntilMs = 0;
  uint8_t  startleStrength = 0;
};

#endif
