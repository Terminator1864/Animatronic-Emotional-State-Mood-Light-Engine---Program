#pragma once
#include <Arduino.h>
#include "EmotionEngine.h"
#include "MoodLight.h"

class SerialConsole {
public:
  SerialConsole(MoodLight& light, EmotionEngine& eng): ml(light), engine(eng) {}
  void printHelp();
  void handle(uint32_t now);

private:
  MoodLight& ml;
  EmotionEngine& engine;
};
