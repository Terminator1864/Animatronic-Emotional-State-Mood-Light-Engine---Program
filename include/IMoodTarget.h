#pragma once
#include "Types.h"

class IMoodTarget {
public:
  virtual ~IMoodTarget() {}
  virtual uint8_t moodCount() const = 0;
  virtual uint8_t currentMoodIndex() const = 0;
  virtual bool setMoodByIndex(uint8_t idx, uint32_t nowMs) = 0;
  virtual bool setMoodByName(const char* name, uint32_t nowMs) = 0;
  virtual PatternType patternOfIndex(uint8_t idx) const = 0;
  virtual bool isFrozen() const = 0;
};
