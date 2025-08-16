#include "EmotionEngine.h"
#include "MoodLight.h" // for PatternType names if needed

void EmotionEngine::begin(uint32_t nowMs){
  (void)nowMs;
  rng ^= (uint16_t)micros();
  for (uint8_t i=0;i<HIST_N;i++) history[i]=255;
}

void EmotionEngine::operatorNext(uint32_t nowMs){
  (void)nowMs;
  const uint8_t cur = currentIdx();
  const uint8_t count = target.moodCount();
  uint16_t w[32]; // enough

  for (uint8_t i=0;i<count;i++){
    uint16_t total = 0;
    total += baseWeight(i);
    total += adjacencyWeight(cur,i);
    total += recencyWeight(i);
    total += patternRecencyWeight(cur,i);
    w[i] = total;
  }
  uint8_t next = pickWeighted(w, count);

  // No-immediate-repeat vs current and last
  if (next == cur) next = (uint8_t)((cur + 1) % count);
  uint8_t last = history[(historyIdx + HIST_N - 1) % HIST_N];
  if (last != 255 && next == last) next = (uint8_t)((next + 1) % count);

  if (target.setMoodByIndex(next, millis())) pushHistory(next);
}

void EmotionEngine::pushHistory(uint8_t idx){
  history[historyIdx++] = idx;
  if (historyIdx >= HIST_N) historyIdx = 0;
}

uint16_t EmotionEngine::urand(){
  // x^16 + x^14 + x^13 + x^11 taps
  uint16_t lfsr = rng;
  uint16_t bit = ((lfsr>>0) ^ (lfsr>>2) ^ (lfsr>>3) ^ (lfsr>>5)) & 1u;
  lfsr = (uint16_t)((lfsr>>1) | (bit<<15));
  rng = lfsr;
  return lfsr;
}

uint8_t EmotionEngine::pickWeighted(uint16_t* w, uint8_t count){
  uint32_t sum = 0;
  for (uint8_t i=0;i<count;i++) sum += w[i];
  if (sum == 0) return (uint8_t)(urand() % count);
  uint32_t r = urand();
  uint32_t pickScaled = (r * sum) >> 16;
  uint32_t acc = 0;
  for (uint8_t i=0;i<count;i++){
    acc += w[i];
    if (pickScaled < acc) return i;
  }
  return (uint8_t)(count-1);
}

EmotionVec EmotionEngine::moodVec(uint8_t i){
  switch ((Mood)i){
    case Mood::Serenity:     return { +40,  -30 };
    case Mood::Joy:          return { +80,  +10 };
    case Mood::Excitement:   return { +70,  +80 };
    case Mood::Love:         return { +65,  +30 };
    case Mood::Pride:        return { +50,  +20 };
    case Mood::Determination:return { +20,  +60 };
    case Mood::Playful:      return { +70,  +60 };
    case Mood::Curiosity:    return { +25,  +40 };
    case Mood::Confusion:    return { -10,  +45 };
    case Mood::Surprise:     return { +10,  +90 };
    case Mood::Sadness:      return { -70,  -30 };
    case Mood::Melancholy:   return { -50,  -50 };
    case Mood::Anger:        return { -80,  +85 };
    case Mood::Panic:        return { -90,  +95 };
    case Mood::Fear:         return { -80,  +70 };
    case Mood::Sleepy:       return {  -5,  -80 };
    default:                 return {   0,    0 };
  }
}

uint16_t EmotionEngine::baseWeight(uint8_t toIdx) const {
  (void)toIdx; return 200;
}

uint16_t EmotionEngine::adjacencyWeight(uint8_t fromIdx, uint8_t toIdx) const {
  if (fromIdx == toIdx) return 40;
  EmotionVec a = const_cast<EmotionEngine*>(this)->moodVec(fromIdx);
  EmotionVec b = const_cast<EmotionEngine*>(this)->moodVec(toIdx);
  int16_t dv = (int16_t)a.valence - (int16_t)b.valence;
  int16_t da = (int16_t)a.arousal - (int16_t)b.arousal;
  uint16_t dist = (uint16_t)(abs(dv) + abs(da));
  int16_t w = 255 - (int16_t)(dist * 2);
  if (w < 0) w = 0;
  return (uint16_t)w;
}

uint16_t EmotionEngine::recencyWeight(uint8_t toIdx) const {
  uint16_t pen = 0;
  for (uint8_t k=0;k<HIST_N;k++){
    if (history[k] == toIdx) {
      uint8_t factor = (uint8_t)((HIST_N - k) * 40); // stronger push to diversify
      pen += factor;
    }
  }
  uint16_t maxPen = 240;
  uint16_t p = (pen > maxPen) ? maxPen : pen;
  return (uint16_t)(maxPen - p);
}

uint16_t EmotionEngine::patternRecencyWeight(uint8_t fromIdx, uint8_t toIdx) const {
  PatternType pf = target.patternOfIndex(fromIdx);
  PatternType pt = target.patternOfIndex(toIdx);
  if (pf != pt) return 200;
  // same pattern -> penalty
  uint16_t pen = (uint16_t)patternPenalty;
  return (uint16_t)(200 - pen); // lower is worse; combined with others
}
