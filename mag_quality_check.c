#include <stdio.h>
#include <math.h>

/*
  Magnitude Check: If the real-time radius deviates from the expected local Earth's magnetic field (e.g., ~50µT)
  by more than 10%, warn of environmental interference.
  Offset Check: If ox, oy, or oz drift significantly from zero (since your input is already calibrated),
  it indicates a permanent change in the hard-iron environment (e.g., a new metal object nearby).
*/

#define LAMBDA 0.999f  // Forgetting factor (0.99 for fast tracking, 0.999 for stability)
#define DELTA  1000.0f // Initial covariance scale

typedef struct
{
  float theta[4];    // State: [offset_x, offset_y, offset_z, G_constant]
  float P[4][4];     // Covariance matrix
} RLS_SphereFit;

/**
 * Initialize the RLS state.
 */
void rls_init(RLS_SphereFit* rls)
{
  for (int i = 0; i < 4; i++)
  {
    rls->theta[i] = 0.0f;
    for (int j = 0; j < 4; j++) {
      rls->P[i][j] = (i == j) ? DELTA : 0.0f;
    }
  }
}

/**
 * Update the fit with a new calibrated magnetometer reading (x, y, z).
 */
void
rls_update(RLS_SphereFit* rls, float x, float y, float z)
{
  // h is the measurement vector derived from the sphere equation
  float h[4] = {2.0f * x, 2.0f * y, 2.0f * z, 1.0f};
  float w = x*x + y*y + z*z;

  // 1. Calculate P * h
  float Ph[4] = {0};
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) Ph[i] += rls->P[i][j] * h[j];
  }

  // 2. Calculate denominator (lambda + h' * P * h)
  float hPh = 0;
  for (int i = 0; i < 4; i++) hPh += h[i] * Ph[i];
  float denominator = LAMBDA + hPh;

  // 3. Compute Gain K = Ph / denominator
  float K[4];
  for (int i = 0; i < 4; i++) K[i] = Ph[i] / denominator;

  // 4. Update State (theta = theta + K * error)
  float hTheta = 0;
  for (int i = 0; i < 4; i++) hTheta += h[i] * rls->theta[i];
  float error = w - hTheta;
  for (int i = 0; i < 4; i++) rls->theta[i] += K[i] * error;

  // 5. Update Covariance (P = (P - K * h' * P) / lambda)
  float newP[4][4];
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      newP[i][j] = (rls->P[i][j] - K[i] * Ph[j]) / LAMBDA;
    }
  }
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) rls->P[i][j] = newP[i][j];
  }
}

/**
 * Extract physical parameters from the state vector.
 */
void
rls_get_results(RLS_SphereFit* rls, float* ox, float* oy, float* oz, float* radius)
{
  *ox = rls->theta[0];
  *oy = rls->theta[1];
  *oz = rls->theta[2];
  // G = B^2 - ox^2 - oy^2 - oz^2
  *radius = sqrtf(rls->theta[3] + (*ox)*(*ox) + (*oy)*(*oy) + (*oz)*(*oz));
}
