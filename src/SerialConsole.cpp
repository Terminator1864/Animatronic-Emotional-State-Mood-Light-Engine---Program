#include "SerialConsole.h"
#include "Config.h"

void SerialConsole::printHelp(){
  Serial.println(F("[CMD] N=Next  F=FreezeToggle  B:<0-255>  M:<name>  M#:<index>  EP:<0-255>=PatternPenalty  ?=Help"));
  Serial.println(F("[BTN] Short=Next | Long(≥700ms)=Freeze | While Frozen: VeryLong(≥1400ms)=Preset Select; Short=Cycle; Long=Apply+Exit"));
}

void SerialConsole::handle(uint32_t now){
  (void)now;
  static char buf[48];
  static uint8_t len=0;
  while (Serial.available()){
    char c=(char)Serial.read();
    if (c=='\r') continue;
    if (c=='\n'){
      buf[len]=0; len=0;
      if (buf[0]==0) return;
      char* p=buf; while(*p==' ') p++;

      if (p[0]=='?'){ printHelp(); return; }
      if (p[0]=='N'||p[0]=='n'){ ml.jumpToNext(millis()); Serial.println(F("[CMD] Next")); return; }
      if (p[0]=='F'||p[0]=='f'){ ml.freezeHold(!ml.isFrozen()); Serial.print(F("[CMD] Freeze -> ")); Serial.println(ml.isFrozen()?F("ON"):F("OFF")); return; }
      if ((p[0]=='B'||p[0]=='b') && p[1]==':'){
        int v=atoi(p+2); if (v<0) v=0; if (v>255) v=255; ml.setGlobalBrightness((uint8_t)v);
        Serial.print(F("[CMD] Brightness=")); Serial.println(v); return;
      }
      if ((p[0]=='M'||p[0]=='m') && p[1]=='#' && p[2]==':'){
        int idx=atoi(p+3); if (idx<0) idx=0; if (idx>=ml.moodCount()) idx=ml.moodCount()-1;
        if (!ml.setMoodByIndex((uint8_t)idx, millis())) Serial.println(F("[CMD] ERROR: Bad Index"));
        else { Serial.print(F("[CMD] Mood Index=")); Serial.println(idx); }
        return;
      }
      if ((p[0]=='M'||p[0]=='m') && p[1]==':'){
        if (!ml.setMoodByName(p+2, millis())) Serial.println(F("[CMD] ERROR: Unknown Mood Name"));
        else { Serial.print(F("[CMD] Mood Name=")); Serial.println(p+2); }
        return;
      }
      if ((p[0]=='E'||p[0]=='e') && (p[1]=='P'||p[1]=='p') && p[2]==':'){
        int v=atoi(p+3); if (v<0) v=0; if (v>255) v=255; engine.setPatternPenalty((uint8_t)v);
        Serial.print(F("[CMD] PatternPenalty=")); Serial.println(v); return;
      }

      Serial.println(F("[CMD] Unknown. Type ? for help."));
      return;
    }
    if (len < sizeof(buf)-1) buf[len++]=c;
  }
}
