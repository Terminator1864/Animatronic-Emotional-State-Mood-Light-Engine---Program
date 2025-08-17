#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include <Arduino.h>

enum class RunMode : uint8_t { ACTIVE = 0, DEMO = 1 };

class ModeManager {
public:
  ModeManager() : current(RunMode::ACTIVE) {}

  RunMode get() const { return current; }

  bool set(RunMode m) {
    if (m == current) return false;
    current = m;
    logMode_();
    return true;
  }

  bool toggle() {
    current = (current == RunMode::ACTIVE) ? RunMode::DEMO : RunMode::ACTIVE;
    logMode_();
    return true;
  }

  void printStatus() const {
    Serial.print(F("[MODE] "));
    Serial.println((current == RunMode::ACTIVE) ? F("ACTIVE") : F("DEMO"));
  }

  static void printHelp() {
    Serial.println(F("[HELP] MODE:ACTIVE | MODE:DEMO | MODE:?"));
  }

private:
  RunMode current;

  void logMode_() const {
    Serial.print(F("[MODE] "));
    Serial.println((current == RunMode::ACTIVE) ? F("ACTIVE") : F("DEMO"));
  }
};

#endif
