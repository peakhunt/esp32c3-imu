#pragma once

#include <stdint.h>

typedef struct __attribute__((packed)) {
    float roll, pitch, yaw;   // 0-11
    float gx, gy, gz;         // 12-23
    float ax, ay, az;         // 24-35
    float mx, my, mz;         // 36-47
    int32_t cpu_usage;        // 48-51
    int32_t sample_rate;      // 52-55
    uint64_t sensor_trx;      // 56-63
    uint64_t sensor_trx_fail; // 64-71
} imu_telemetry_pkt_t;        // 72 Bytes

extern void web_server_init(void);
extern void ws_broadcast_imu_update(imu_telemetry_pkt_t* pkt);
