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
  bool    startled;
  SensorSignals() : arousalBias(0), valenceBias(128), valid(false), startled(false) {}
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
                                ((uint64_t)l1       * ACCEL_EWMA_ALPHA)) / 255);
    }

    uint32_t delta = (l1 > baseline_) ? (l1 - baseline_) : (baseline_ - l1);

    // ---- Startle detection (rising-edge, confirm, cooldown) ----
    static uint32_t startleUntil = 0;
    static uint32_t startleCooldownUntil = 0;
    static uint32_t lastDelta = 0;
    static uint8_t  startleArm = 0;

    // Use Config.h knobs
    const uint16_t ABS_ON      = STARTLE_ABS_ON;
    const uint16_t JERK_ON     = STARTLE_JERK_ON;
    const uint8_t  CONFIRM_N   = STARTLE_CONFIRM_SAMPLES;
    const uint16_t DUR_MS      = STARTLE_MS;
    const uint16_t COOLDOWN_MS = STARTLE_COOLDOWN;

    // Consider startle only when clearly active (above the Schmitt 'on' threshold)
    const uint16_t ACTIVE_GATE_ON = ACCEL_GATE_MIN_DELTA + 6;

    uint32_t jerk = (delta > lastDelta) ? (delta - lastDelta) : 0;
    lastDelta = delta;

    bool risingHit = (delta >= ACTIVE_GATE_ON) && (delta >= ABS_ON || jerk >= JERK_ON);

    if (!risingHit) {
        startleArm = 0;
    } else {
        if ((uint8_t)(startleArm + 1u) >= CONFIRM_N) {
            if ((int32_t)(nowMs - startleCooldownUntil) >= 0) {
                startleUntil = nowMs + DUR_MS;
                startleCooldownUntil = nowMs + COOLDOWN_MS;
            }
            startleArm = 0;
        } else {
            startleArm++;
        }
    }

    // Robust to millis() wraparound
    out.startled = (int32_t)(startleUntil - nowMs) > 0;

// ---- Schmitt (hysteresis) gate - replaces old "Noise gate" block ----
    static bool gateOpen = false;
    const uint16_t SCHMITT_TH_ON  = ACCEL_GATE_MIN_DELTA + 6;  // enter active
    const uint16_t SCHMITT_TH_OFF = ACCEL_GATE_MIN_DELTA + 2;  // leave active

    if (!gateOpen) {
        if (delta < SCHMITT_TH_ON) {
        if (diag_) {
            Serial.print(F("[SENSE] idle delta="));
            Serial.println((unsigned)delta);
        }
        out.valid = false;      // stay calm
        return out;
        }
        gateOpen = true;          // crossed into active
    } else {
        if (delta < SCHMITT_TH_OFF) {
        gateOpen = false;       // drop back to calm
        if (diag_) {
            Serial.print(F("[SENSE] close delta="));
            Serial.println((unsigned)delta);
        }
        out.valid = false;
        return out;
        }
    }
    // (fall-through when gate is open)

    // ---- Scale arousal to 0..255 ----
    uint32_t scaled = (delta >> ACCEL_SCALE_SHIFT);
    if (scaled > 255) scaled = 255;

    out.arousalBias = (uint8_t)scaled; // 0 calm .. 255 intense
    out.valenceBias = 128;             // neutral for now
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
