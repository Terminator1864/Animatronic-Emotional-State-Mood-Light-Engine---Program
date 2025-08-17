#include "SerialConsole.h"
#include "Config.h"
#include "ModeManager.h"
#include <ctype.h>   // tolower
#include "SensorInput.h"

// Add a pointer to SensorInput
SensorInput* sense = nullptr;

static bool equalsIgnoreCase(const char* a, const char* b) {
  while (*a && *b) {
    char ca = (char)tolower(*a++);
    char cb = (char)tolower(*b++);
    if (ca != cb) return false;
  }
  return *a == *b;
}

void SerialConsole::printHelp() {
  Serial.println(F("[CMD] N=Next  F=FreezeToggle  B:<0-255>  M:<name>  M#:<index>  EP:<0-255>|EP:?  ?=Help"));
  Serial.println(F("[BTN] Short=Next | Long(>=700ms)=Freeze | Frozen: VeryLong(>=1400ms)=Preset (Short=Cycle 1..6, Long=Apply+Exit)"));
  Serial.println(F("[CMD] MODE:ACTIVE | MODE:DEMO | MODE:?"));
  Serial.println(F("[CMD] SENSE:ON | SENSE:OFF | SENSE:? | SENSE:DIAG:ON|OFF"));
}

void SerialConsole::handle(uint32_t now) {
  (void)now;
  static char buf[64];
  static uint8_t len = 0;

  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      buf[len] = 0;
      len = 0;
      if (buf[0] == 0) return;

      // single char
      if ((buf[0] == 'N' || buf[0] == 'n') && buf[1] == 0) { engine.operatorNext(millis()); return; }
      if ((buf[0] == 'F' || buf[0] == 'f') && buf[1] == 0) {
        ml.freezeHold(!ml.isFrozen());
        Serial.print(F("[CMD] Freeze -> "));
        Serial.println(ml.isFrozen() ? F("ON") : F("OFF"));
        return;
      }
      if (buf[0] == '?' && buf[1] == 0) { printHelp(); return; }

      char* p = buf;

      // B:<0-255>
      if ((p[0]=='B'||p[0]=='b') && p[1]==':') {
        int v = atoi(p+2); if (v<0) v=0; if (v>255) v=255;
        ml.setGlobalBrightness((uint8_t)v);
        Serial.print(F("[CMD] Brightness=")); Serial.println(v);
        return;
      }

      // M:<name>  or  M#:<index>
      if ((p[0]=='M'||p[0]=='m') && p[1]==':') {
        if (p[2]=='#') {
          int idx = atoi(p+3);
          if (ml.setMoodByIndex((uint8_t)idx, millis())) {
            Serial.print(F("[CMD] Mood Index=")); Serial.println(idx);
          } else {
            Serial.println(F("[ERROR] Invalid mood index"));
          }
        } else {
          const char* name = p+2;
          if (ml.setMoodByName(name, millis())) {
            Serial.print(F("[CMD] Mood Name=")); Serial.println(name);
          } else {
            Serial.println(F("[ERROR] Unknown mood name"));
          }
        }
        return;
      }

      // EP:<0-255> or EP:?
      if ((p[0]=='E'||p[0]=='e') && (p[1]=='P'||p[1]=='p') && p[2]==':') {
        if (p[3]=='?') {
          Serial.print(F("[CMD] PatternPenalty="));
          Serial.println(engine.getPatternPenalty());
          return;
        }
        int v = atoi(p+3); if (v<0) v=0; if (v>255) v=255;
        engine.setPatternPenalty((uint8_t)v);
        Serial.print(F("[CMD] PatternPenalty=")); Serial.println(v);
        return;
      }

      // MODE:ACTIVE | MODE:DEMO | MODE:?
      if (p[0]=='M' && p[1]=='O' && p[2]=='D' && p[3]=='E' && p[4]==':') {
        if (!mode) { Serial.println(F("[ERROR] ModeManager not attached")); return; }
        const char* v = p+5;
        if (equalsIgnoreCase(v,"ACTIVE"))       { mode->set(RunMode::ACTIVE); }
        else if (equalsIgnoreCase(v,"DEMO"))    { mode->set(RunMode::DEMO); }
        else if (v[0]=='?' && v[1]==0)         { mode->printStatus(); }
        else { Serial.println(F("[ERROR] MODE:<ACTIVE|DEMO|?>")); }
        return;
      }

      if (p[0]=='S'&&p[1]=='E'&&p[2]=='N'&&p[3]=='S'&&p[4]=='E'&&p[5]==':') {
      if (!sense) { Serial.println(F("[ERROR] SensorInput not attached")); return; }
      const char* v = p+6;
      if      (equalsIgnoreCase(v,"ON"))               { sense->setEnabled(true);  Serial.println(F("[SENSE] ENABLED")); }
      else if (equalsIgnoreCase(v,"OFF"))              { sense->setEnabled(false); Serial.println(F("[SENSE] DISABLED")); }
      else if (v[0]=='?' && v[1]==0) {
        Serial.print(F("[SENSE] Enabled=")); Serial.print(sense->isEnabled()?F("YES"):F("NO"));
        Serial.print(F(" | AccelPresent=")); Serial.println(sense->isPresent()?F("YES"):F("NO"));
      } else if (v[0]=='D'&&v[1]=='I'&&v[2]=='A'&&v[3]=='G'&&v[4]==':') {
        const char* dv = v+5;
        if      (equalsIgnoreCase(dv,"ON"))  { sense->setDiag(true);  Serial.println(F("[SENSE] DIAG=ON")); }
        else if (equalsIgnoreCase(dv,"OFF")) { sense->setDiag(false); Serial.println(F("[SENSE] DIAG=OFF")); }
        else { Serial.println(F("[ERROR] SENSE:DIAG:ON|OFF")); }
      } else {
        Serial.println(F("[ERROR] SENSE:ON|OFF|?|DIAG:ON|OFF"));
      }
      return;
    }

      Serial.println(F("[CMD] Unknown. Type ? for help."));
      return;
    }
    if (len < sizeof(buf)-1) buf[len++] = c;
  }
}
