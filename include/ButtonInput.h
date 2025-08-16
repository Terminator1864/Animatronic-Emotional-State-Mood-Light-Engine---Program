#pragma once
#include <Arduino.h>
#include "Config.h"
#include "MoodLight.h"
#include "EmotionEngine.h"
#include "PresetSelector.h"

struct PresetState {
  bool active=false;
  uint8_t sel=1;
  uint32_t lastActivity=0;
};

void handleButton(uint32_t now, MoodLight& ml, EmotionEngine& engine, PresetState& ps);
