#pragma once

#include <stdbool.h>
#include <stdint.h>

#define LAMBDA 0.999f  // Forgetting factor (0.99 for fast tracking, 0.999 for stability)
#define DELTA  1000.0f // Initial covariance scale

typedef struct
{
  float theta[4];    // State: [offset_x, offset_y, offset_z, G_constant]
  float P[4][4];     // Covariance matrix
  bool radius_error;
  bool offset_error;
} RLS_SphereFit;

extern void rls_init(RLS_SphereFit* rls);
extern void rls_update(RLS_SphereFit* rls, float x, float y, float z);
extern void rls_get_results(RLS_SphereFit* rls, float* ox, float* oy, float* oz, float* radius);
