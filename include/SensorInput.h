#ifndef SENSOR_INPUT_H
#define SENSOR_INPUT_H

#include <Arduino.h>
#include <Wire.h>
#include "Config.h"

// Compact signal bundle for UNO footprint.
struct SensorSignals {
  uint8_t arousalBias;   // 0 calm .. 255 intense
  uint8_t valenceBias;   // 0 negative .. 255 positive
  bool    valid;
  SensorSignals() : arousalBias(0), valenceBias(128), valid(false) {}
};

class SensorInput {
public:
  void begin() {
    if (si_begun_) return;
    si_begun_ = true;

    Wire.begin();
    #if defined(WIRE_HAS_SET_CLOCK)
      Wire.setClock(100000UL); // 100 kHz for robustness
    #endif

    accel_present_ = accelInit_();
    if (!accel_present_) {
      Serial.println(F("[SENSE] LSM303 Accel: NOT detected; sensors disabled"));
      return;
    }
    Serial.println(F("[SENSE] LSM303 Accel: OK"));
  }

  SensorSignals sample(uint32_t nowMs) {
    SensorSignals out;                 // defaults valid=false
    if (!enabled_)       return out;   // SENSE:OFF → neutral
    if (!accel_present_) return out;   // sensor missing → neutral
    if ((nowMs - accel_last_ms_) < ACCEL_SAMPLE_INTERVAL_MS) return out;

    accel_last_ms_ = nowMs;

    int16_t x, y, z;
    if (!readAccel_(x, y, z)) {
      if (++i2c_fail_count_ == 3) {
        Serial.println(F("[SENSE] LSM303 Accel: I2C errors; muting until OK"));
      }
      return out;
    }
    i2c_fail_count_ = 0;

    // LSM303DLHC: 12-bit left-aligned → shift right 4
    x >>= 4; y >>= 4; z >>= 4;

    // Motion envelope vs slow baseline (EWMA)
    uint32_t l1 = (uint32_t)abs(x) + (uint32_t)abs(y) + (uint32_t)abs(z);
    if (!baseline_init_) {
      baseline_ = l1;
      baseline_init_ = true;
    } else {
      baseline_ = (uint32_t)((((uint64_t)baseline_ * (255 - ACCEL_EWMA_ALPHA)) +
                              ((uint64_t)l1        * ACCEL_EWMA_ALPHA)) / 255);
    }

    uint32_t delta = (l1 > baseline_) ? (l1 - baseline_) : (baseline_ - l1);

    // Noise gate
    if (delta <= ACCEL_GATE_MIN_DELTA) {
    if (diag_) {
        Serial.print(F("[SENSE] gated delta="));
        Serial.println((unsigned)delta);
    }
    out.valid = false;  // calm → no influence
    return out;
    }

    // Fast scale → 0..255 (tweak shift in Config.h)
    uint32_t scaled = (delta >> ACCEL_SCALE_SHIFT);
    if (scaled > 255) scaled = 255;

    out.arousalBias = (uint8_t)scaled; // 0 calm .. 255 intense
    out.valenceBias = 128;             // neutral
    out.valid = true;

    if (diag_) {
      Serial.print(F("[SENSE] arousal="));
      Serial.println(out.arousalBias);
    }

    return out;
  }

  // Controls/Status
  void setEnabled(bool e) { enabled_ = e; }
  bool isEnabled()  const { return enabled_; }
  void setDiag(bool on)   { diag_ = on; }
  bool isPresent()  const { return accel_present_; }

private:
  // Use address from Config.h so you can flip 0x19/0x18 there
  static constexpr uint8_t ACCEL_ADDR_  = LSM303_ACCEL_ADDR;
  static constexpr uint8_t WHO_AM_I_    = 0x0F; // expect 0x33
  static constexpr uint8_t CTRL_REG1_A_ = 0x20; // ODR + axes enable
  static constexpr uint8_t CTRL_REG4_A_ = 0x23; // high-res, range
  static constexpr uint8_t OUT_X_L_A_   = 0x28; // low byte; auto-inc bit set

  // Module state
  bool     si_begun_       = false;
  bool     accel_present_  = false;
  uint32_t accel_last_ms_  = 0;
  uint32_t baseline_       = 0;
  bool     baseline_init_  = false;
  uint8_t  i2c_fail_count_ = 0;

  // Feature toggles
  bool enabled_ = true;   // SENSE:ON by default
  bool diag_    = false;  // telemetry off by default

  // Init (100 Hz, High-Res, ±2g)
  bool accelInit_() {
    uint8_t who = 0;
    if (!readReg_(ACCEL_ADDR_, WHO_AM_I_, who)) return false;
    if (who != 0x33) {
      Serial.print(F("[SENSE] LSM303 WHO_AM_I=")); Serial.println(who, HEX);
      // continue; some variants misreport
    }
    if (!writeReg_(ACCEL_ADDR_, CTRL_REG1_A_, 0x57)) return false; // 100Hz, XYZ on
    if (!writeReg_(ACCEL_ADDR_, CTRL_REG4_A_, 0x08)) return false; // HR, ±2g
    delay(5);
    int16_t x,y,z; return readAccel_(x,y,z);
  }

  // I2C helpers
  bool writeReg_(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    return (Wire.endTransmission() == 0);
  }
  bool readReg_(uint8_t addr, uint8_t reg, uint8_t& out) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)addr, 1) != 1) return false;
    out = Wire.read();
    return true;
  }
  bool readAccel_(int16_t& x, int16_t& y, int16_t& z) {
    uint8_t start = OUT_X_L_A_ | 0x80; // auto-increment
    Wire.beginTransmission(ACCEL_ADDR_);
    Wire.write(start);
    if (Wire.endTransmission(false) != 0) return false;

    const uint8_t n = 6;
    if (Wire.requestFrom((int)ACCEL_ADDR_, (int)n) != n) return false;
    uint8_t b[6];
    for (uint8_t i=0;i<6;i++) b[i]=Wire.read();

    // little-endian: L then H
    x = (int16_t)((uint16_t)b[1] << 8 | b[0]);
    y = (int16_t)((uint16_t)b[3] << 8 | b[2]);
    z = (int16_t)((uint16_t)b[5] << 8 | b[4]);
    return true;
  }
};

#endif // SENSOR_INPUT_H
