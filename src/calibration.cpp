#include "calibration.h"
#include <EEPROM.h>

const char* CALIB_POSITIONS[6] = {
  "X-Axis Arrow UP",
  "X-Axis Arrow DOWN",
  "Y-Axis Arrow UP",
  "Y-Axis Arrow DOWN",
  "Sensor Face UP",
  "Sensor Face DOWN"
};

#define CALIB_EEPROM_BASE 24
#define CALIB_MAGIC 0x43414C42

#define CALIB_SAMPLES_PER_POS 128
#define CALIB_TOTAL_SAMPLES (6 * CALIB_SAMPLES_PER_POS)

volatile CalibState g_calib_state = CALIB_IDLE;
volatile uint8_t g_calib_position = 0;
volatile uint16_t g_calib_sample_idx = 0;
CalibParams g_calib_params = {{0,0,0}, {1,0,0,0,1,0,0,0,1}, false};

static xyzFloat s_calib_samples[CALIB_TOTAL_SAMPLES];

void beginCalibration() {
  g_calib_state = CALIB_COLLECTING;
  g_calib_position = 0;
  g_calib_sample_idx = 0;
  g_calib_params.valid = false;
}

static void matrix3x3Inverse(const float m[9], float inv[9]) {
  float det = m[0]*(m[4]*m[8] - m[5]*m[7]) - m[1]*(m[3]*m[8] - m[5]*m[6]) + m[2]*(m[3]*m[7] - m[4]*m[6]);
  if (fabs(det) < 1e-10) return;
  float inv_det = 1.0f / det;
  inv[0] = (m[4]*m[8] - m[5]*m[7]) * inv_det;
  inv[1] = (m[2]*m[7] - m[1]*m[8]) * inv_det;
  inv[2] = (m[1]*m[5] - m[2]*m[4]) * inv_det;
  inv[3] = (m[5]*m[6] - m[3]*m[8]) * inv_det;
  inv[4] = (m[0]*m[8] - m[2]*m[6]) * inv_det;
  inv[5] = (m[2]*m[3] - m[0]*m[5]) * inv_det;
  inv[6] = (m[3]*m[7] - m[4]*m[6]) * inv_det;
  inv[7] = (m[1]*m[6] - m[0]*m[7]) * inv_det;
  inv[8] = (m[0]*m[4] - m[1]*m[3]) * inv_det;
}

static void matrix3x3Multiply(const float a[9], const float b[9], float r[9]) {
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      r[i*3+j] = a[i*3+0]*b[0*3+j] + a[i*3+1]*b[1*3+j] + a[i*3+2]*b[2*3+j];
    }
  }
}

static void matrix3x3Transpose(const float m[9], float t[9]) {
  t[0] = m[0]; t[1] = m[3]; t[2] = m[6];
  t[3] = m[1]; t[4] = m[4]; t[5] = m[7];
  t[6] = m[2]; t[7] = m[5]; t[8] = m[8];
}

static void vector3Cross(const float a[3], const float b[3], float r[3]) {
  r[0] = a[1]*b[2] - a[2]*b[1];
  r[1] = a[2]*b[0] - a[0]*b[2];
  r[2] = a[0]*b[1] - a[1]*b[0];
}

static float vector3Dot(const float a[3], const float b[3]) {
  return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

static void vector3Normalize(float v[3]) {
  float len = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
  if (len > 1e-10f) {
    v[0] /= len; v[1] /= len; v[2] /= len;
  }
}

static void computeEigenvalues3x3(const float Q[9], float eigenvals[3]) {
  float m = (Q[0] + Q[4] + Q[8]) / 3.0f;
  float a = Q[0] - m;
  float b = Q[4] - m;
  float c = Q[8] - m;
  float d = Q[1]*Q[1] + Q[2]*Q[2] + Q[5]*Q[5];
  float p = (a*a + b*b + c*c + 2.0f*d) / 6.0f;
  float det = Q[0]*(Q[4]*Q[8]-Q[5]*Q[5]) - Q[1]*(Q[1]*Q[8]-Q[5]*Q[2]) + Q[2]*(Q[1]*Q[5]-Q[4]*Q[2]);
  float r = det / 2.0f;
  float phi;
  if (p <= 0.0f) {
    eigenvals[0] = m; eigenvals[1] = m; eigenvals[2] = m;
    return;
  }
  if (r <= -sqrt(p*p*p)) {
    eigenvals[0] = m - sqrt(p); eigenvals[1] = m - sqrt(p); eigenvals[2] = m + 2.0f*sqrt(p);
    return;
  }
  if (r >= sqrt(p*p*p)) {
    eigenvals[0] = m + sqrt(p); eigenvals[1] = m + sqrt(p); eigenvals[2] = m - 2.0f*sqrt(p);
    return;
  }
  phi = acosf(r / sqrt(p*p*p)) / 3.0f;
  eigenvals[0] = m + 2.0f * sqrt(p) * cosf(phi);
  eigenvals[1] = m + 2.0f * sqrt(p) * cosf(phi + 2.09439510239f);
  eigenvals[2] = m + 2.0f * sqrt(p) * cosf(phi + 4.18879020478f);
}

static void computeEigenvector3x3(const float Q[9], float eigenval, float eigenvector[3]) {
  float Qm[9] = {Q[0]-eigenval, Q[1], Q[2], Q[3], Q[4]-eigenval, Q[5], Q[6], Q[7], Q[8]-eigenval};
  float a = Qm[0], b = Qm[1], c = Qm[3], d = Qm[4], e = Qm[2], f = Qm[5];
  if (fabs(e) > fabs(c)) {
    eigenvector[0] = d*e - b*f;
    eigenvector[1] = b*e - a*f;
    eigenvector[2] = a*d - b*c;
  } else {
    eigenvector[0] = d*b - e*c;
    eigenvector[1] = a*e - d*c;
    eigenvector[2] = a*d - b*c;
  }
  vector3Normalize(eigenvector);
}

static void matrixSquareRoot3x3(const float Q[9], float sqrtQ[9]) {
  float eigenvals[3];
  computeEigenvalues3x3(Q, eigenvals);
  float eigenvecs[9];
  float eigenvector[3];
  for (int i = 0; i < 3; i++) {
    computeEigenvector3x3(Q, eigenvals[i], eigenvector);
    eigenvecs[i*3+0] = eigenvector[0];
    eigenvecs[i*3+1] = eigenvector[1];
    eigenvecs[i*3+2] = eigenvector[2];
  }
  float eigenvecs_inv[9];
  matrix3x3Inverse(eigenvecs, eigenvecs_inv);
  float sqrtD[9] = {sqrtf(fmax(eigenvals[0], 0.0f)), 0, 0,
                    0, sqrtf(fmax(eigenvals[1], 0.0f)), 0,
                    0, 0, sqrtf(fmax(eigenvals[2], 0.0f))};
  float temp[9];
  matrix3x3Multiply(eigenvecs, sqrtD, temp);
  matrix3x3Multiply(temp, eigenvecs_inv, sqrtQ);
}

bool runEllipsoidFit() {
  int n = CALIB_TOTAL_SAMPLES;
  float D[10];
  for (int i = 0; i < n; i++) {
    float x = s_calib_samples[i].x;
    float y = s_calib_samples[i].y;
    float z = s_calib_samples[i].z;
    D[0] = x*x; D[1] = y*y; D[2] = z*z;
    D[3] = 2.0f*x*y; D[4] = 2.0f*x*z; D[5] = 2.0f*y*z;
    D[6] = 2.0f*x; D[7] = 2.0f*y; D[8] = 2.0f*z; D[9] = 1.0f;
  }
  float M[9] = {0};
  float C[10] = {0};
  for (int i = 0; i < n; i++) {
    float x = s_calib_samples[i].x;
    float y = s_calib_samples[i].y;
    float z = s_calib_samples[i].z;
    M[0] += x*x; M[1] += x*y; M[2] += x*z;
    M[3] += x*y; M[4] += y*y; M[5] += y*z;
    M[6] += x*z; M[7] += y*z; M[8] += z*z;
    C[0] += x*x; C[1] += y*y; C[2] += z*z;
    C[3] += 2.0f*x*y; C[4] += 2.0f*x*z; C[5] += 2.0f*y*z;
    C[6] += 2.0f*x; C[7] += 2.0f*y; C[8] += 2.0f*z; C[9] += 1.0f;
  }
  float MM[9];
  matrix3x3Inverse(M, MM);
  float v[3] = {-MM[0]*C[6] - MM[1]*C[7] - MM[2]*C[8],
                -MM[3]*C[6] - MM[4]*C[7] - MM[5]*C[8],
                -MM[6]*C[6] - MM[7]*C[7] - MM[8]*C[8]};
  float Qtemp[9] = {M[0], M[1], M[2], M[3], M[4], M[5], M[6], M[7], M[8]};
  float Qinv[9];
  matrix3x3Inverse(Qtemp, Qinv);
  float center[3] = {Qinv[0]*v[0] + Qinv[1]*v[1] + Qinv[2]*v[2],
                     Qinv[3]*v[0] + Qinv[4]*v[1] + Qinv[5]*v[2],
                     Qinv[6]*v[0] + Qinv[7]*v[1] + Qinv[8]*v[2]};
  float Q[9];
  Q[0] = M[0]; Q[1] = M[1]; Q[2] = M[2];
  Q[3] = M[3]; Q[4] = M[4]; Q[5] = M[5];
  Q[6] = M[6]; Q[7] = M[7]; Q[8] = M[8];
  float sqrtQ[9];
  matrixSquareRoot3x3(Q, sqrtQ);
  g_calib_params.offset[0] = center[0];
  g_calib_params.offset[1] = center[1];
  g_calib_params.offset[2] = center[2];
  for (int i = 0; i < 9; i++) {
    g_calib_params.correction[i] = sqrtQ[i];
  }
  float avg_r = 0.0f;
  for (int i = 0; i < n; i++) {
    float dx = s_calib_samples[i].x - center[0];
    float dy = s_calib_samples[i].y - center[1];
    float dz = s_calib_samples[i].z - center[2];
    float r = sqrtf(dx*dx + dy*dy + dz*dz);
    avg_r += r;
  }
  avg_r /= n;
  if (avg_r < 0.01f || isnan(avg_r) || isinf(avg_r)) {
    return false;
  }
  for (int i = 0; i < 9; i++) {
    g_calib_params.correction[i] /= avg_r;
  }
  g_calib_params.valid = true;
  return true;
}

xyzFloat applyCalibration(xyzFloat raw) {
  if (!g_calib_params.valid) return raw;
  float x = raw.x - g_calib_params.offset[0];
  float y = raw.y - g_calib_params.offset[1];
  float z = raw.z - g_calib_params.offset[2];
  xyzFloat corrected;
  corrected.x = g_calib_params.correction[0]*x + g_calib_params.correction[1]*y + g_calib_params.correction[2]*z;
  corrected.y = g_calib_params.correction[3]*x + g_calib_params.correction[4]*y + g_calib_params.correction[5]*z;
  corrected.z = g_calib_params.correction[6]*x + g_calib_params.correction[7]*y + g_calib_params.correction[8]*z;
  return corrected;
}

void collectCalibSample(xyzFloat sample) {
  if (g_calib_state != CALIB_COLLECTING) return;
  if (g_calib_position >= 6) return;
  int idx = g_calib_position * CALIB_SAMPLES_PER_POS + g_calib_sample_idx;
  if (idx >= CALIB_TOTAL_SAMPLES) return;
  s_calib_samples[idx] = sample;
  g_calib_sample_idx++;
  if (g_calib_sample_idx >= CALIB_SAMPLES_PER_POS) {
    g_calib_sample_idx = 0;
    g_calib_position++;
    if (g_calib_position >= 6) {
      g_calib_state = CALIB_FITTING;
    }
  }
}

uint8_t getCalibPosition() { return g_calib_position; }
uint16_t getCalibSampleIdx() { return g_calib_sample_idx; }
CalibState getCalibState() { return g_calib_state; }

bool saveCalibrationToEEPROM() {
  uint32_t magic = CALIB_MAGIC;
  uint8_t valid = g_calib_params.valid ? 1 : 0;
  EEPROM.put(CALIB_EEPROM_BASE, magic);
  EEPROM.put(CALIB_EEPROM_BASE + 4, g_calib_params.offset);
  EEPROM.put(CALIB_EEPROM_BASE + 4 + 12, g_calib_params.correction);
  EEPROM.put(CALIB_EEPROM_BASE + 4 + 12 + 36, valid);
  return EEPROM.commit();
}

bool loadCalibrationFromEEPROM() {
  uint32_t magic = 0;
  EEPROM.get(CALIB_EEPROM_BASE, magic);
  if (magic != CALIB_MAGIC) {
    g_calib_params.valid = false;
    return false;
  }
  EEPROM.get(CALIB_EEPROM_BASE + 4, g_calib_params.offset);
  EEPROM.get(CALIB_EEPROM_BASE + 4 + 12, g_calib_params.correction);
  uint8_t valid = 0;
  EEPROM.get(CALIB_EEPROM_BASE + 4 + 12 + 36, valid);
  g_calib_params.valid = (valid == 1);
  return g_calib_params.valid;
}