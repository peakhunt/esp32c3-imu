#include <math.h>

#include "mag_calibration.h"
#include "sensor_calib.h"

static int32_t          _mag_prev[3];
static int16_t          _mag_offset[3];
static int16_t          _mag_min[3];
static int16_t          _mag_max[3];

static sensor_calib_t   _cal_state;

void
mag_calibration_init(void)
{
  _mag_min[0] = _mag_min[1] = _mag_min[2] =  32767;
  _mag_max[0] = _mag_max[1] = _mag_max[2] = -32768;

  _mag_prev[0] = 
  _mag_prev[1] = 
  _mag_prev[2] = 0;

  sensorCalibrationResetState(&_cal_state);
}

void
mag_calibration_update(int16_t mx, int16_t my, int16_t mz)
{
  float     diffMag = 0;
  float     avgMag = 0;
  int32_t   mag_data[3];

  mag_data[0] = mx;
  mag_data[1] = my;
  mag_data[2] = mz;

  for (int axis = 0; axis < 3; axis++) {
    diffMag += (mag_data[axis] - _mag_prev[axis]) * (mag_data[axis] - _mag_prev[axis]);
    avgMag += (mag_data[axis] + _mag_prev[axis]) * (mag_data[axis] + _mag_prev[axis]) / 4.0f;
  }

  // Check if sensor has rotated enough to accept a new calibration point
  if ((avgMag > 0.01f) && ((diffMag / avgMag) > (0.14f * 0.14f))) {
    
    // CAPTURE BOUNDARIES FOR SOFT IRON
    for (int axis = 0; axis < 3; axis++) {
      if (mag_data[axis] < _mag_min[axis]) _mag_min[axis] = (int16_t)mag_data[axis];
      if (mag_data[axis] > _mag_max[axis]) _mag_max[axis] = (int16_t)mag_data[axis];
    }

    sensorCalibrationPushSampleForOffsetCalculation(&_cal_state, mag_data);

    for (int axis = 0; axis < 3; axis++) {
      _mag_prev[axis] = mag_data[axis];
    }
  }
}


void
mag_calibration_finish(int16_t offsets[3])
{
  float magZerof[3];

  sensorCalibrationSolveForOffset(&_cal_state, magZerof);

  for (int axis = 0; axis < 3; axis++) 
  {
    _mag_offset[axis] = lrintf(magZerof[axis]);
  }

  offsets[0] = _mag_offset[0];
  offsets[1] = _mag_offset[1];
  offsets[2] = _mag_offset[2];
}

void
mag_calibration_finish_with_soft_iron(int16_t offsets[3], int16_t scales[3])
{
  float magZerof[3];
  float axis_delta[3];
  float avg_delta = 0.0f;

  // 1. Hard Iron: Solve for the center of the sphere
  // (Assuming your SolveForOffset uses the memcpy fix we discussed)
  sensorCalibrationSolveForOffset(&_cal_state, magZerof);

  // 2. Soft Iron: Calculate the spread of each axis
  for (int i = 0; i < 3; i++) {
    axis_delta[i] = (float)(_mag_max[i] - _mag_min[i]);
    avg_delta += axis_delta[i];
  }

  // Calculate average diameter (the target sphere size)
  avg_delta /= 3.0f; 

  // 3. Finalize results
  for (int axis = 0; axis < 3; axis++) 
  {
    // Save Offset (Hard Iron)
    _mag_offset[axis] = lrintf(magZerof[axis]);
    offsets[axis] = _mag_offset[axis];

    // Save Scale (Soft Iron) normalized to 4096 (1.0 ratio)
    // This stretches the "egg" back into a sphere
    if (axis_delta[axis] > 0.001f) {
      scales[axis] = lrintf((avg_delta / axis_delta[axis]) * 4096.0f);
    } else {
      scales[axis] = 4096; // Fallback to 1.0 gain
    }
  }
}
