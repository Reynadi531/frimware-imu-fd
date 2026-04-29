#pragma once
#include <Arduino.h>
#include <MPU6500_WE.h>

enum CalibState {
  CALIB_IDLE,
  CALIB_COLLECTING,
  CALIB_FITTING,
  CALIB_SUCCESS,
  CALIB_FAILED
};

struct CalibParams {
  float offset[3];
  float correction[9];
  bool valid;
};

extern const char* CALIB_POSITIONS[6];

extern volatile CalibState g_calib_state;
extern volatile uint8_t g_calib_position;
extern volatile uint16_t g_calib_sample_idx;
extern CalibParams g_calib_params;

void beginCalibration();
bool runEllipsoidFit();
xyzFloat applyCalibration(xyzFloat raw);
bool saveCalibrationToEEPROM();
bool loadCalibrationFromEEPROM();