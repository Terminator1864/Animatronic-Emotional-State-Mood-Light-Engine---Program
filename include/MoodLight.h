#pragma once
#include <Arduino.h>
#include "Types.h"
#include "IMoodTarget.h"

struct MoodDef {
  Mood mood;
  const char* nameCStr;
  const char* baseNameCStr;
  const char* altNameCStr;
  Rgb8 baseColor;
  Rgb8 altColor;
  PatternType pattern;
  uint8_t amp0to255;
  uint16_t periodMs;
  uint16_t holdMs;
};

class MoodLight : public IMoodTarget {
public:
  static const MoodDef MOODS[(int)Mood::Count];

  MoodLight(uint8_t pinRedPwm, uint8_t pinGreenPwm, uint8_t pinBluePwm,
            uint16_t fadeDurationMs, uint16_t fadeStepMs, uint8_t globalBrightness0to255);

  // lifecycle
  void begin();
  void update(uint32_t nowMs);

  // IMoodTarget
  uint8_t moodCount() const override { return (uint8_t)Mood::Count; }
  uint8_t currentMoodIndex() const override { return moodIndex; }
  bool setMoodByIndex(uint8_t idx, uint32_t nowMs) override;
  bool setMoodByName(const char* name, uint32_t nowMs) override;
  PatternType patternOfIndex(uint8_t idx) const override;
  bool isFrozen() const override { return freezeMode; }

  // control / telemetry
  void setGlobalBrightness(uint8_t b);
  const char* currentMoodName() const;
  const char* currentPatternName() const;
  uint16_t currentPeriodMs() const;
  uint8_t  currentAmp() const;

  Rgb8 currentBaseColorScaled() const;
  Rgb8 currentAltColorScaled() const;
  void jumpToNext(uint32_t nowMs);
  void freezeHold(bool enable);

  // helpers
  static const char* patternName(PatternType p);

private:
  // pins
  uint8_t pinR, pinG, pinB;
  // config
  uint16_t fadeTotalMs, fadeStepIntervalMs;
  uint8_t  globalBrightness;
  // state
  bool isInit;
  uint8_t moodIndex;
  uint32_t lastStepMs, holdStartMs;
  bool isHolding;
  bool printedStatusThisHold;
  bool freezeMode;

  Rgb8 startColor, targetColor;
  uint16_t stepsPlanned, stepNumber;
  uint32_t lfsr;

  // internals
  void advanceToNextMood(uint32_t nowMs);
  void setTargetFromMood(uint8_t idx);
  void startFade(uint32_t nowMs);
  void stepFadeOnce();
  void updateHoldPattern(uint32_t nowMs);

  // hw helpers
  void writeCommonAnodePwm(const Rgb8& c);
  static uint8_t scaleAndClamp(uint8_t v, uint8_t s);
  static uint8_t triangleWave(uint32_t t, uint16_t p);
  static uint8_t pulseWave(uint32_t t, uint16_t p, uint8_t duty);
  static uint8_t heartbeatWave(uint32_t t, uint16_t p);
  uint8_t flickerJitter();

  // misc
  bool equalsIgnoreCase(const char* a, const char* b);
  void printStatusLine();
};
