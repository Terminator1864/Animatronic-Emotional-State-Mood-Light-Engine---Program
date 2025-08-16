#pragma once
#include <Arduino.h>

// 6 basic emotions mapped to mood names we already have
inline const char* presetNameByIndex(uint8_t i){
  switch(i){
    case 1: return "Fear";
    case 2: return "Anger";
    case 3: return "Sadness";
    case 4: return "Curiosity"; // Disgust alias
    case 5: return "Joy";       // Happiness alias
    case 6: return "Playful";   // Joy alias
    default: return "Joy";
  }
}
inline const char* presetDisplayName(uint8_t i){
  switch(i){
    case 1: return "Fear";
    case 2: return "Anger";
    case 3: return "Sadness";
    case 4: return "Disgust (alias: Curiosity)";
    case 5: return "Happiness (alias: Joy)";
    case 6: return "Joy (alias: Playful)";
    default: return "Joy";
  }
}
