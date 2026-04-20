/*
 * copied from iNav
 */
#include <math.h>
#include "sensor_calib.h"

void
sensorCalibrationResetState(sensor_calib_t * state)
{
  int i, j;

  for(i = 0; i < 4; i++)
  {
    for(j = 0; j < 4; j++)
    {
      state->XtX[i][j] = 0;
    }
    state->XtY[i] = 0;
  }
}

void
sensorCalibrationPushSampleForOffsetCalculation(sensor_calib_t * state, int32_t sample[3])
{
  state->XtX[0][0] += (float)sample[0] * sample[0];
  state->XtX[0][1] += (float)sample[0] * sample[1];
  state->XtX[0][2] += (float)sample[0] * sample[2];
  state->XtX[0][3] += (float)sample[0];

  state->XtX[1][0] += (float)sample[1] * sample[0];
  state->XtX[1][1] += (float)sample[1] * sample[1];
  state->XtX[1][2] += (float)sample[1] * sample[2];
  state->XtX[1][3] += (float)sample[1];

  state->XtX[2][0] += (float)sample[2] * sample[0];
  state->XtX[2][1] += (float)sample[2] * sample[1];
  state->XtX[2][2] += (float)sample[2] * sample[2];
  state->XtX[2][3] += (float)sample[2];

  state->XtX[3][0] += (float)sample[0];
  state->XtX[3][1] += (float)sample[1];
  state->XtX[3][2] += (float)sample[2];
  state->XtX[3][3] += 1;

  float squareSum = ((float)sample[0] * sample[0]) + ((float)sample[1] * sample[1]) + ((float)sample[2] * sample[2]);

  state->XtY[0] += sample[0] * squareSum;
  state->XtY[1] += sample[1] * squareSum;
  state->XtY[2] += sample[2] * squareSum;
  state->XtY[3] += squareSum;
}

void
sensorCalibrationPushSampleForScaleCalculation(sensor_calib_t * state, int axis, int32_t sample[3], int target)
{
  int i;

  for (i = 0; i < 3; i++)
  {
    float scaledSample = (float)sample[i] / (float)target;
    state->XtX[axis][i] += scaledSample * scaledSample;
    state->XtX[3][i] += scaledSample * scaledSample;
  }

  state->XtX[axis][3] += 1;
  state->XtY[axis] += 1;
  state->XtY[3] += 1;
}

static void
sensorCalibration_gaussLR(float mat[4][4])
{
  uint8_t n = 4;
  int i, j, k;
  for (i = 0; i < 4; i++)
  {
    // Determine R
    for (j = i; j < 4; j++)
    {
      for (k = 0; k < i; k++)
      {
        mat[i][j] -= mat[i][k] * mat[k][j];
      }
    }
    // Determine L
    for (j = i + 1; j < n; j++)
    {
      for (k = 0; k < i; k++)
      {
        mat[j][i] -= mat[j][k] * mat[k][i];
      }
      mat[j][i] /= mat[i][i];
    }
  }
}

void
sensorCalibration_ForwardSubstitution(float LR[4][4], float y[4], float b[4])
{
  int i, k;

  for (i = 0; i < 4; ++i)
  {
    y[i] = b[i];
    for (k = 0; k < i; ++k)
    {
      y[i] -= LR[i][k] * y[k];
    }
    //y[i] /= MAT_ELEM_AT(LR,i,i); //Do not use, LR(i,i) is 1 anyways and not stored in this matrix
  }
}

void
sensorCalibration_BackwardSubstitution(float LR[4][4], float x[4], float y[4])
{
  int i, k;
  for (i = 3 ; i >= 0; --i)
  {
    x[i] = y[i];
    for (k = i + 1; k < 4; ++k)
    {
      x[i] -= LR[i][k] * x[k];
    }
    x[i] /= LR[i][i];
  }
}

// solve linear equation
// https://en.wikipedia.org/wiki/Gaussian_elimination
static void
sensorCalibration_SolveLGS(float A[4][4], float x[4], float b[4])
{
  int i;
  float y[4];

  sensorCalibration_gaussLR(A);

  for (i = 0; i < 4; ++i)
  {
    y[i] = 0;
  }

  sensorCalibration_ForwardSubstitution(A, y, b);
  sensorCalibration_BackwardSubstitution(A, x, y);
}

#include <string.h>

void
sensorCalibrationSolveForOffset(sensor_calib_t * state, float result[3])
{
  float beta[4];
  float XtX_copy[4][4]; // Temporary buffer for the solver
  float XtY_copy[4];    // Temporary buffer for the solver
  int   i;

  // 1. CLONE the state into local memory
  // This prevents the LU decomposition from corrupting our running sums
  memcpy(XtX_copy, state->XtX, sizeof(XtX_copy));
  memcpy(XtY_copy, state->XtY, sizeof(XtY_copy));

  // 2. Solve the Linear Equation using the copy
  sensorCalibration_SolveLGS(XtX_copy, beta, XtY_copy);

  // 3. Extract the Hard-Iron offsets (Center of the sphere)
  for (i = 0; i < 3; i++)
  {
    result[i] = beta[i] / 2.0f;
  }
}

#include <string.h>
#include <math.h>

void
sensorCalibrationSolveForScale(sensor_calib_t * state, float result[3])
{
  float beta[4];
  float XtX_copy[4][4];
  float XtY_copy[4];
  int   i;

  // 1. CLONE the state into local memory
  // This is CRITICAL because sensorCalibration_SolveLGS (gaussLR) 
  // modifies the matrix in-place during LU decomposition.
  memcpy(XtX_copy, state->XtX, sizeof(XtX_copy));
  memcpy(XtY_copy, state->XtY, sizeof(XtY_copy));

  // 2. Solve the Linear Equation
  // If the sensor wasn't moved enough, XtX_copy[i][i] might be 0, 
  // causing a NaN (Not a Number) result here.
  sensorCalibration_SolveLGS(XtX_copy, beta, XtY_copy);

  // 3. Extract and Sanitize Results
  for (i = 0; i < 3; i++)
  {
    // sqrtf(beta[i]) gives the scale factor.
    // We add a check for NaN or negative values (which shouldn't happen 
    // with good data but can happen with noise/singular matrices).
    if (isnan(beta[i]) || beta[i] <= 0.0001f) {
        result[i] = 1.0f; // Fallback to neutral gain
    } else {
        result[i] = sqrtf(beta[i]);
    }
  }
}
