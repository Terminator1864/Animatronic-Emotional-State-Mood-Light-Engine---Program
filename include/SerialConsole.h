#ifndef SERIAL_CONSOLE_H
#define SERIAL_CONSOLE_H

#include <Arduino.h>
#include "EmotionEngine.h"
#include "MoodLight.h"

class ModeManager; 
class SensorInput;

class SerialConsole {
public:
  SerialConsole(MoodLight& light, EmotionEngine& eng) : ml(light), engine(eng) {}
  void printHelp();
  void handle(uint32_t now);

  // Inject optional ModeManager for MODE commands
  void attachModeManager(ModeManager* mm) { mode = mm; }
  void attachSensorInput(SensorInput* si) { sense = si; } 

private:
  MoodLight& ml;
  EmotionEngine& engine;
  ModeManager* mode = nullptr;
  SensorInput* sense = nullptr; 
};

#endif 