#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "mpu6500.h"
#include "qmc5883l.h"
#include "imu.h"
#include "imu_task.h"
#include "web_server.h"
#include "imu_config.h"
#include "esp_attr.h"

#define TARGET_SAMPLE_RATE        1000
#define TARGET_MAG_SAMPLE_RATE    100
#define IMU_DATA_SEND_SKIP        (TARGET_SAMPLE_RATE / 50)       // 50Hz


static const char *TAG = "imu_task";

static mpu6500_t              _mpu6500;
static qmc5883lMag            _mag;
static imu_t                  _imu;
static SemaphoreHandle_t      _mutex;
static SemaphoreHandle_t      _mag_mutex;

volatile uint32_t             _sample_rate = 0;

static int64_t                      _cal_start_time = 0;
static int64_t                      _cal_duration = 0;
static const int64_t                CAL_DURATION_US = 60 * 1000000; // 60 seconds in microseconds
static const int64_t                CAL_DURATION_ACCEL_US = 10 * 1000000; // 60 seconds in microseconds
static imu_task_calib_complete_cb   _calib_cb = NULL;
static void*                        _calib_cb_arg = NULL;

static QueueHandle_t                _imu_buf;

static int16_t  _mag_data_buf[3] = { 0, 0, 0};

static inline void
measure_sample_rate(void)
{
  static bool first = true;
  static uint32_t sample_count = 0;
  static uint64_t last_time = 0;
  uint64_t now;

  if(first)
  {
    first = false;
    last_time = esp_timer_get_time(); // microsecond
  }
  sample_count++;
  now = esp_timer_get_time();

  if (now - last_time >= 1000000ULL)
  {
    _sample_rate = (uint32_t)((float)sample_count / ((now - last_time) / 1000000.0f));
    // ESP_LOGI(TAG, "Sampling rate: %ld Hz", _sample_rate);
    sample_count = 0;
    last_time = now;
  }
}

static inline void
throttled_imu_data_send(void)
{
  static int skip_count = 0;

  // send data at 50Hz
  if (++skip_count >= IMU_DATA_SEND_SKIP)
  { 
    imu_telemetry_pkt_t pkt;
    extern volatile uint32_t cpu_usage;

    pkt.roll        = _imu.data.orientation[0];
    pkt.pitch       = _imu.data.orientation[1];
    pkt.yaw         = _imu.data.orientation[2];

    pkt.gx          = _imu.data.gyro[0];
    pkt.gy          = _imu.data.gyro[1];
    pkt.gz          = _imu.data.gyro[2];

    pkt.ax          = _imu.data.accel[0];
    pkt.ay          = _imu.data.accel[1];
    pkt.az          = _imu.data.accel[2];

    pkt.mx          = _imu.data.mag[0];
    pkt.my          = _imu.data.mag[1];
    pkt.mz          = _imu.data.mag[2];

    pkt.cpu_usage       = cpu_usage;
    pkt.sample_rate     = _sample_rate;
    pkt.sensor_trx      = _mpu6500.num_transactions + _mag.num_transactions;  
    pkt.sensor_trx_fail = _mpu6500.num_failed + _mag.num_failed;

    // ws_broadcast_imu_update(&pkt);
    xQueueSend(_imu_buf, &pkt, 0); 
    skip_count = 0;
  }
}

static void
imu_task_start_calib_timer(uint64_t duration)
{
  _cal_duration = duration;
  _cal_start_time = esp_timer_get_time();
}

static bool
imu_task_handle_calibration_timer(void)
{
  int64_t current_time = esp_timer_get_time();
  int64_t elapsed = current_time - _cal_start_time;

  if(elapsed >= _cal_duration)
  {
    switch(_imu.mode)
    {
    case imu_mode_mag_calibrating:
      imu_mag_calibration_finish(&_imu);
      ESP_LOGI(TAG, "finishing mag calibration %d %d %d %d %d %d",
          _imu.cal.mag_bias[0],
          _imu.cal.mag_bias[1],
          _imu.cal.mag_bias[2],
          _imu.cal.mag_scale[0],
          _imu.cal.mag_scale[1],
          _imu.cal.mag_scale[2]);
      imu_config_update_mag_calib(_imu.cal.mag_bias, _imu.cal.mag_scale);
      break;

    case imu_mode_gyro_calibrating:
      imu_gyro_calibration_finish(&_imu);
      ESP_LOGI(TAG, "finishing gyro calibration %d %d %d",
          _imu.cal.gyro_off[0],
          _imu.cal.gyro_off[1],
          _imu.cal.gyro_off[2]);
      imu_config_update_gyro_calib(_imu.cal.gyro_off);
      break;

    case imu_mode_accel_calibrating:
      imu_accel_calibration_step_stop(&_imu);
      ESP_LOGI(TAG, "stopping accel sensor step calibration");
      break;

    default:
      break;
    }
    return true;
  }
  return false;
}

static void
update_mag_sensor(void)
{
  xSemaphoreTake(_mag_mutex, portMAX_DELAY);
  _imu.raw.mag[0] = _mag_data_buf[0];
  _imu.raw.mag[1] = _mag_data_buf[1];
  _imu.raw.mag[2] = _mag_data_buf[2];
  xSemaphoreGive(_mag_mutex);
}

static void
imu_task_init_all(void)
{
  imu_engine_config_t e_cfg;

  imu_config_get_imu_engine_config(&e_cfg);

  xSemaphoreTake(_mutex, portMAX_DELAY);

  imu_init(&_imu, &e_cfg, TARGET_SAMPLE_RATE,
      imu_sensor_align_cw_90_flip,
      imu_sensor_align_cw_90_flip,
      imu_sensor_align_cw_90_flip);

  mpu6500_init(&_mpu6500, MPU6500_Accelerometer_8G, MPU6500_Gyroscope_1000s, &_imu.lsb); 
  qmc5883l_init(&_mag, &_imu.lsb.mag_lsb);

  imu_config_get_sensor_config(&_imu.cal);

  ESP_LOGI(TAG, "ahrs alg: %s", e_cfg.ahrs_mode == IMU_AHRS_MODE_MAHONY ? "mahony" : "madgwick");
  ESP_LOGI(TAG, "beta: %.3f", e_cfg.madgwick_beta);
  ESP_LOGI(TAG, "twoKp: %.3f, twoKi: %.3f", e_cfg.mahony_kp, e_cfg.mahony_ki);
  ESP_LOGI(TAG, "accel_off %d, %d, %d", _imu.cal.accel_off[0], _imu.cal.accel_off[1], _imu.cal.accel_off[2]);
  ESP_LOGI(TAG, "accel_scale %d, %d, %d", _imu.cal.accel_scale[0], _imu.cal.accel_scale[1], _imu.cal.accel_scale[2]);
  ESP_LOGI(TAG, "gyro_off %d, %d, %d", _imu.cal.gyro_off[0], _imu.cal.gyro_off[1], _imu.cal.gyro_off[2]);
  ESP_LOGI(TAG, "mag_bias %d, %d, %d", _imu.cal.mag_bias[0], _imu.cal.mag_bias[1], _imu.cal.mag_bias[2]);
  ESP_LOGI(TAG, "mag_scale %d, %d, %d", _imu.cal.mag_scale[0], _imu.cal.mag_scale[1], _imu.cal.mag_scale[2]);
  ESP_LOGI(TAG, "mag_declination %f", _imu.engine_cfg.mag_declination);

  xSemaphoreGive(_mutex);
}

static void
imu_task(void *arg)
{
  ESP_LOGI(TAG, "Starting IMU task...");

  imu_task_init_all();

  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(1000/TARGET_SAMPLE_RATE); // 2 ms → 500 Hz
  bool shouldNotify = false;

  imu_task_calib_complete_cb   calib_cb;
  void*                        calib_cb_arg;

  while (1)
  {
    mpu6500_read_gyro_accel(&_mpu6500, &_imu.raw);
    update_mag_sensor();

    xSemaphoreTake(_mutex, portMAX_DELAY);
    imu_update(&_imu);

    if(_imu.mode !=imu_mode_normal)
    {
      if(imu_task_handle_calibration_timer() == true)
      {
        calib_cb      = _calib_cb;
        calib_cb_arg  = _calib_cb_arg;
        shouldNotify  = true;
      }
    }
    xSemaphoreGive(_mutex);

    if(shouldNotify)
    {
      shouldNotify = false;
      calib_cb(calib_cb_arg);
    }

    measure_sample_rate();

    throttled_imu_data_send();

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

static void
imu_telemetry_task(void* arg)
{
  imu_telemetry_pkt_t received_pkt;
  while (1)
  {
    // This blocks until a packet arrives, but since it's Prio 5, 
    // it won't steal CPU from your 1KHz IMU sampling.
    if (xQueueReceive(_imu_buf, &received_pkt, portMAX_DELAY)) {
      ws_broadcast_imu_update(&received_pkt);
    }
  }
}

static void
imu_mag_read_task(void* arg)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(1000/TARGET_MAG_SAMPLE_RATE);
  int16_t  buf[3] = { 0, 0, 0};
                                                  //
  while(true)
  {
    qmc5883l_read(&_mag, buf);

    xSemaphoreTake(_mag_mutex, portMAX_DELAY);
    _mag_data_buf[0] = buf[0];
    _mag_data_buf[1] = buf[1];
    _mag_data_buf[2] = buf[2];
    xSemaphoreGive(_mag_mutex);

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void
imu_task_init(void)
{
  _mutex      = xSemaphoreCreateMutex();
  _mag_mutex  = xSemaphoreCreateMutex();

  _imu_buf = xQueueCreate(10, sizeof(imu_telemetry_pkt_t));

  xTaskCreate(imu_task, "imu_task", 4096, NULL, 15, NULL);
  xTaskCreate(imu_telemetry_task, "imu_telemetry_task", 2048, NULL, 10, NULL);
  xTaskCreate(imu_mag_read_task, "imu_mag_read_task", 1024, NULL, 10, NULL);
}

void
imu_task_get_data(imu_data_t* data)
{
  xSemaphoreTake(_mutex, portMAX_DELAY);
  memcpy(data, &_imu.data, sizeof(imu_data_t));
  xSemaphoreGive(_mutex);
}

void
imu_task_get_attitude(float* roll, float* pitch, float* yaw)
{
  xSemaphoreTake(_mutex, portMAX_DELAY);
  *roll = _imu.data.orientation[0];
  *pitch = _imu.data.orientation[1];
  *yaw = _imu.data.orientation[2];
  xSemaphoreGive(_mutex);
}

void
imu_task_start_gyro_calibration(imu_task_calib_complete_cb cb, void* data)
{
  xSemaphoreTake(_mutex, portMAX_DELAY);
  _calib_cb = cb;
  _calib_cb_arg = data;
  imu_gyro_calibration_start(&_imu);
  imu_task_start_calib_timer(CAL_DURATION_US);
  ESP_LOGI(TAG, "starting gyro calibration");
  xSemaphoreGive(_mutex);
}

void
imu_task_get_gyro_calibration(float offset[3])
{
  xSemaphoreTake(_mutex, portMAX_DELAY);
  imu_gyro_get_calibration(&_imu, offset);
  xSemaphoreGive(_mutex);
}

void
imu_task_start_mag_calibration(imu_task_calib_complete_cb cb, void* data)
{
  xSemaphoreTake(_mutex, portMAX_DELAY);
  _calib_cb = cb;
  _calib_cb_arg = data;
  imu_mag_calibration_start(&_imu);
  imu_task_start_calib_timer(CAL_DURATION_US);
  ESP_LOGI(TAG, "starting mag calibration");
  xSemaphoreGive(_mutex);
}

void
imu_task_get_mag_calibration(float bias[3], float scale[3])
{
  xSemaphoreTake(_mutex, portMAX_DELAY);
  
  imu_mag_get_calibration(&_imu, bias, scale);
  
  xSemaphoreGive(_mutex);
}

void
imu_task_start_accel_calibration(bool reset, imu_task_calib_complete_cb cb, void* data)
{
  xSemaphoreTake(_mutex, portMAX_DELAY);

  _calib_cb = cb;
  _calib_cb_arg = data;

  if(reset)
  {
    ESP_LOGI(TAG, "resetting accel sensor calibration");
    imu_accel_calibration_init(&_imu);
  }

  imu_accel_calibration_step_start(&_imu);
  ESP_LOGI(TAG, "starting accel sensor step calibration");
  imu_task_start_calib_timer(CAL_DURATION_ACCEL_US);

  xSemaphoreGive(_mutex);
}

bool
imu_task_finish_accel_calibration(void)
{
  bool status;
  xSemaphoreTake(_mutex, portMAX_DELAY);
  status = imu_accel_calibration_finish(&_imu);
  ESP_LOGI(TAG, "finishing accel sensor calibration, status %d", status);
  if (status)
  {
    ESP_LOGI(TAG, "finishing accel calibration %d %d %d %d %d %d",
        _imu.cal.accel_off[0],
        _imu.cal.accel_off[1],
        _imu.cal.accel_off[2],
        _imu.cal.accel_scale[0],
        _imu.cal.accel_scale[1],
        _imu.cal.accel_scale[2]);
    imu_config_update_accel_calib(_imu.cal.accel_off, _imu.cal.accel_scale);
  }
  xSemaphoreGive(_mutex);
  return status;
}

void
imu_task_get_accel_calibration(float offset[3], float scale[3])
{
  xSemaphoreTake(_mutex, portMAX_DELAY);
  
  imu_accel_get_calibration(&_imu, offset, scale);
  
  xSemaphoreGive(_mutex);
}

void
imu_task_config_ahrs(imu_engine_config_t* cfg)
{
  xSemaphoreTake(_mutex, portMAX_DELAY);
  
  imu_config_engine(&_imu, cfg);

  imu_config_update_ahrs(cfg);
  
  xSemaphoreGive(_mutex);
}
