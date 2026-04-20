#include "freertos/FreeRTOS.h"
#include "nvs_helper.h"
#include "esp_log.h"
#include "imu_config.h"

#define NVS_MEMBER(struct_type, member, nvs_key, type, len) \
    { nvs_key, type, offsetof(struct_type, member), len }

#define IMU_SCHEMA_COUNT (sizeof(_schema) / sizeof(_schema[0]))

static const char* TAG = "imu_config";
static SemaphoreHandle_t _nvs_lock;

//
// schema definition
//
static nvs_helper_schema_def_t    _schema[] =
{
  NVS_MEMBER(imu_config_t, magic,                   "magic",      NVS_TYPE_U32,  0),
  NVS_MEMBER(imu_config_t, revision,                "rev",        NVS_TYPE_U16,  0),
  
  // --- SENSOR CALIBRATION ---
  NVS_MEMBER(imu_config_t, sensor.accel_off,        "acc_off",    NVS_TYPE_BLOB, 6),
  NVS_MEMBER(imu_config_t, sensor.accel_scale,      "acc_scale",  NVS_TYPE_BLOB, 6),
  NVS_MEMBER(imu_config_t, sensor.gyro_off,         "gyro_off",   NVS_TYPE_BLOB, 6),
  NVS_MEMBER(imu_config_t, sensor.mag_bias,         "mag_bias",   NVS_TYPE_BLOB, 6),
  NVS_MEMBER(imu_config_t, sensor.mag_scale,        "mag_scale",  NVS_TYPE_BLOB, 6),

  // --- IMU ENGINE ---
  NVS_MEMBER(imu_config_t, engine.ahrs_mode,        "imu_mode",   NVS_TYPE_U8,   0),
  NVS_MEMBER(imu_config_t, engine.madgwick_beta,    "imu_beta",   NVS_TYPE_BLOB, 4),
  NVS_MEMBER(imu_config_t, engine.mahony_kp,        "imu_kp",     NVS_TYPE_BLOB, 4),
  NVS_MEMBER(imu_config_t, engine.mahony_ki,        "imu_ki",     NVS_TYPE_BLOB, 4),
  NVS_MEMBER(imu_config_t, engine.mag_declination,  "mag_dec",    NVS_TYPE_BLOB, 4),

  // --- WIFI CONFIG ---
  NVS_MEMBER(imu_config_t, wifi.sta_enabled,        "sta_en",     NVS_TYPE_U8,   0),
  NVS_MEMBER(imu_config_t, wifi.sta_ssid,           "sta_ssid",   NVS_TYPE_STR,  32),
  NVS_MEMBER(imu_config_t, wifi.sta_password,       "sta_pw",     NVS_TYPE_STR,  64),
  NVS_MEMBER(imu_config_t, wifi.ap_ssid,            "ap_ssid",    NVS_TYPE_STR,  32),
  NVS_MEMBER(imu_config_t, wifi.ap_password,        "ap_pw",      NVS_TYPE_STR,  64),
  NVS_MEMBER(imu_config_t, wifi.ap_ip,              "ap_ip",      NVS_TYPE_STR,  16),
  NVS_MEMBER(imu_config_t, wifi.ap_mask,            "ap_mask",    NVS_TYPE_STR,  16),
  NVS_MEMBER(imu_config_t, wifi.channel,            "wifi_ch",    NVS_TYPE_U8,   0),
};

//
// default config
//
static const imu_config_t   _default_cfg = 
{
  .magic    = IMU_CONFIG_MAGIC,
  .revision = IMU_CONFIG_REVISION,

  // --- SENSOR DEFAULTS ---
  .sensor = 
  {
    .accel_off   = { 0, 0, 0 },
    .accel_scale = { 4096, 4096, 4096 },
    .gyro_off    = { 0, 0, 0 },
    .mag_bias    = { 0, 0, 0 },
    .mag_scale   = { 4096, 4096, 4096 },
  },

  // --- IMU ENGINE DEFAULTS ---
  .engine = 
  {
    .ahrs_mode     = IMU_AHRS_MODE_MADGWICK,
    .madgwick_beta = 2.25f,
    .mahony_kp     = 2.0f * 0.5f,
    .mahony_ki     = 2.0f * 0.0f, 
    .mag_declination = 0.0f,
  },

  // --- WIFI DEFAULTS ---
  .wifi = 
  {
    .sta_enabled  = false,
    .sta_ssid     = "apname",
    .sta_password = "",

    .ap_ssid      = "imu",
    .ap_password  = "",
    .ap_ip        = "192.168.4.1",
    .ap_mask      = "255.255.255.0",
    .channel      = 1,
  }
};


//
// live in memory config
//
static imu_config_t _live_cfg;

void
imu_config_init(void)
{
  _nvs_lock = xSemaphoreCreateMutex();
  ESP_ERROR_CHECK(_nvs_lock ? ESP_OK : ESP_FAIL);

  nvs_helper_init("nvs");    // this will crash if it fails

  // basic sanity & upgrade check
  // read magic and revision
  if ((nvs_helper_read_item(&_schema[0], &_live_cfg) != ESP_OK ||
      nvs_helper_read_item(&_schema[1], &_live_cfg) != ESP_OK) ||
      _live_cfg.magic != IMU_CONFIG_MAGIC)
  {
    // invalid storage
    ESP_LOGW(TAG, "invalid storage detected. resetting NVS storage");
    ESP_ERROR_CHECK(nvs_helper_write_all(_schema, IMU_SCHEMA_COUNT, &_default_cfg));
    memcpy(&_live_cfg, &_default_cfg, sizeof(imu_config_t));
    ESP_LOGW(TAG, "resetting NVS storage complete");
  }
  else if (_live_cfg.revision != _default_cfg.revision)
  {
    // upgrade necessary
    ESP_LOGW(TAG, "revision mismatch found. current %d, flash %d. upgrading", _default_cfg.revision, _live_cfg.revision);
    ESP_ERROR_CHECK(nvs_helper_read_all_with_defaults(_schema, IMU_SCHEMA_COUNT, &_live_cfg, &_default_cfg));
    _live_cfg.revision = _default_cfg.revision;
    ESP_ERROR_CHECK(nvs_helper_write_item(&_schema[1], &_live_cfg));
    ESP_LOGW(TAG, "upgrading complete");
  } else {
    ESP_LOGI(TAG, "reading system config");
    ESP_ERROR_CHECK(nvs_helper_read_all(_schema, IMU_SCHEMA_COUNT, &_live_cfg));
    ESP_LOGI(TAG, "reading system config complete");
  }
  ESP_LOGI(TAG, "config magic: %08x, revision: %d", _live_cfg.magic, _live_cfg.revision);
}

static void
imu_conifg_write_all(void)
{
  ESP_ERROR_CHECK(nvs_helper_write_all(_schema, IMU_SCHEMA_COUNT, &_live_cfg));
}

void
imu_config_update_accel_calib(int16_t off[3], int16_t scale[3])
{
  xSemaphoreTake(_nvs_lock, portMAX_DELAY);

  _live_cfg.sensor.accel_off[0] = off[0];
  _live_cfg.sensor.accel_off[1] = off[1];
  _live_cfg.sensor.accel_off[2] = off[2];
  _live_cfg.sensor.accel_scale[0] = scale[0];
  _live_cfg.sensor.accel_scale[1] = scale[1];
  _live_cfg.sensor.accel_scale[2] = scale[2];

  imu_conifg_write_all();
  xSemaphoreGive(_nvs_lock);
}

void
imu_config_update_gyro_calib(int16_t off[3])
{
  xSemaphoreTake(_nvs_lock, portMAX_DELAY);

  _live_cfg.sensor.gyro_off[0] = off[0];
  _live_cfg.sensor.gyro_off[1] = off[1];
  _live_cfg.sensor.gyro_off[2] = off[2];

  imu_conifg_write_all();
  xSemaphoreGive(_nvs_lock);
}

void
imu_config_update_mag_calib(int16_t bias[3], int16_t scale[3])
{
  xSemaphoreTake(_nvs_lock, portMAX_DELAY);

  _live_cfg.sensor.mag_bias[0] = bias[0];
  _live_cfg.sensor.mag_bias[1] = bias[1];
  _live_cfg.sensor.mag_bias[2] = bias[2];

  _live_cfg.sensor.mag_scale[0] = scale[0];
  _live_cfg.sensor.mag_scale[1] = scale[1];
  _live_cfg.sensor.mag_scale[2] = scale[2];

  ESP_LOGI(TAG, "updating mag calibration %d %d %d %d %d %d",
      bias[0], bias[1], bias[2],
      scale[0], scale[1], scale[2]);

  imu_conifg_write_all();
  xSemaphoreGive(_nvs_lock);
}

void
imu_config_update_ahrs(imu_engine_config_t* cfg)
{
  xSemaphoreTake(_nvs_lock, portMAX_DELAY);

  memcpy(&_live_cfg.engine, cfg, sizeof(imu_engine_config_t));

  imu_conifg_write_all();

  xSemaphoreGive(_nvs_lock);
}

void
imu_config_get_sensor_config(imu_sensor_calib_data_t* cfg)
{
  xSemaphoreTake(_nvs_lock, portMAX_DELAY);

  memcpy(cfg, &_live_cfg.sensor, sizeof(imu_sensor_calib_data_t));

  xSemaphoreGive(_nvs_lock);
}

void
imu_config_get_imu_engine_config(imu_engine_config_t* cfg)
{
  xSemaphoreTake(_nvs_lock, portMAX_DELAY);

  memcpy(cfg, &_live_cfg.engine, sizeof(imu_engine_config_t));

  xSemaphoreGive(_nvs_lock);
}

void
imu_config_get_wifi_config(imu_wifi_config_t* cfg)
{
  xSemaphoreTake(_nvs_lock, portMAX_DELAY);

  memcpy(cfg, &_live_cfg.wifi, sizeof(imu_wifi_config_t));

  xSemaphoreGive(_nvs_lock);
}

void
imu_config_update_wifi_config(imu_wifi_config_t* cfg)
{
  xSemaphoreTake(_nvs_lock, portMAX_DELAY);

  memcpy(&_live_cfg.wifi, cfg, sizeof(imu_wifi_config_t));
  imu_conifg_write_all();

  xSemaphoreGive(_nvs_lock);
}
