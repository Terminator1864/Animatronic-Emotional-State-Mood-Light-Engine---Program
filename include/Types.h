#pragma once
#include <Arduino.h>

struct Rgb8 { uint8_t r,g,b; };

enum class PatternType : uint8_t {
  Static=0, Breathe, Pulse, Heartbeat, Flicker, BlinkAlt
};

enum class Mood : uint8_t {
  Serenity, Joy, Excitement, Love, Pride, Determination, Playful, Curiosity,
  Confusion, Surprise, Sadness, Melancholy, Anger, Panic, Fear, Sleepy,
  Count
};
