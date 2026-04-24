#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <netinet/tcp.h>
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"
#include "esp_tls_crypto.h"
#include "esp_http_server.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_check.h"
#include <time.h>
#include <sys/time.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include "nvs_flash.h"
#include "esp_eth.h"
#include "esp_vfs.h"
#include "web_server.h"
#include "imu_task.h"
#include "imu_config.h"
#include "jsmn.h"
#include "lwip/sockets.h"  // For setsockopt and socket functions
#include "lwip/tcp.h"      // For TCP_NODELAY constant
#include "esp_attr.h"

#define WEB_SERVER_HTTP_QUERY_KEY_MAX_LEN  (64)

/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (200*1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  8192

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

struct file_server_data
{
  /* Base path of file storage */
  char base_path[ESP_VFS_PATH_MAX + 1];

  /* Scratch buffer for temporary storage during file transfer */
  char scratch[SCRATCH_BUFSIZE];
};

static const char* TAG = "web_server";
static httpd_handle_t _server = NULL;
static struct file_server_data *server_data = NULL;

static int
jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) 
  {
    return 0;
  }
  return -1;
}

static esp_err_t
set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
  const char *type = "text/plain";
  if (CHECK_FILE_EXTENSION(filepath, ".html")) {
    type = "text/html";
  } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
    type = "application/javascript";
  } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
    type = "text/css";
  } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
    type = "image/png";
  } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
    type = "image/x-icon";
  } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
    type = "image/svg+xml";
  }
  return httpd_resp_set_type(req, type);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// HTTP GET default Handler for file serving
//
////////////////////////////////////////////////////////////////////////////////////////////////////////
static esp_err_t
default_get_handler(httpd_req_t *req)
{
  char filepath[FILE_PATH_MAX];

  struct file_server_data *s_data = (struct file_server_data *)req->user_ctx;

  strlcpy(filepath, s_data->base_path, sizeof(filepath));

  if (req->uri[strlen(req->uri) - 1] == '/') {
    strlcat(filepath, "/index.html", sizeof(filepath));
  } else {
    strlcat(filepath, req->uri, sizeof(filepath));
  }
  set_content_type_from_file(req, filepath);

  ESP_LOGI(TAG, "static file request: %s", filepath);

  int fd = open(filepath, O_RDONLY, 0);
  if (fd == -1) {
    //
    // append ".gz" for compressed files
    //
    strlcat(filepath, ".gz", sizeof(filepath));
    fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
      ESP_LOGE(TAG, "Failed to open file : %s", filepath);
      /* Respond with 500 Internal Server Error */
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
      return ESP_FAIL;
    }
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  }

  httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  httpd_resp_set_hdr(req, "Pragma", "no-cache");

  char *chunk = s_data->scratch;
  ssize_t read_bytes;
  do {
    /* Read file in chunks into the scratch buffer */
    read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
    if (read_bytes == -1) {
      ESP_LOGE(TAG, "Failed to read file : %s", filepath);
    } else if (read_bytes > 0) {
      /* Send the buffer contents as HTTP response chunk */
      if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
        close(fd);
        ESP_LOGE(TAG, "File sending failed : %s", filepath);
        /* Abort sending file */
        httpd_resp_sendstr_chunk(req, NULL);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
        return ESP_FAIL;
      }
    }
  } while (read_bytes > 0);
  /* Close file after sending complete */
  close(fd);
  ESP_LOGI(TAG, "File sending complete: %s", filepath);
  /* Respond with an empty chunk to signal HTTP response completion */
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;

}

static httpd_uri_t common_get_uri = {
  .uri = "/*",
  .method = HTTP_GET,
  .handler = default_get_handler,
  .user_ctx = NULL,
};

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Gyro Calibration
//
////////////////////////////////////////////////////////////////////////////////////////////////////////
static void
gyro_calibration_complete_response(void *arg)
{
  httpd_req_t *req = (httpd_req_t *)arg;
  float offset[3];

  imu_task_get_gyro_calibration( offset);

  char resp_json[128];
  snprintf(resp_json, sizeof(resp_json),
      "{\"status\":\"success\",\"offsets\":{\"x\":%.4f,\"y\":%.4f,\"z\":%.4f}}",
      offset[0], offset[1], offset[2]);

  // 3. Set content type and send
  httpd_resp_set_type(req, "application/json"); // Explicitly set as JSON
  httpd_resp_send(req, resp_json, HTTPD_RESP_USE_STRLEN);

  // 4. Finalize the async request
  httpd_req_async_handler_complete(req);
}

static void
on_gyro_calibration_complete_cb(void *arg)
{
  httpd_queue_work(_server, gyro_calibration_complete_response, arg);
  //gyro_calibration_complete_response(arg);
}


static esp_err_t
gyro_cal_post_handler(httpd_req_t *req)
{
  // 1. Create a copy of the request for asynchronous handling
  httpd_req_t *async_req;
  esp_err_t err = httpd_req_async_handler_begin(req, &async_req);
  if (err != ESP_OK) return err;

  imu_task_start_gyro_calibration(on_gyro_calibration_complete_cb, async_req);

  // 3. Return ESP_OK immediately. This frees the server task
  // so it can continue sending WebSocket frames.
  return ESP_OK;
}

static const
httpd_uri_t gyro_cal_uri =
{
  .uri       = "/api/calibrate/gyro",
  .method    = HTTP_POST,
  .handler   = gyro_cal_post_handler,
  .user_ctx  = NULL
};

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Magnetometer calibration
//
////////////////////////////////////////////////////////////////////////////////////////////////////////
static void
mag_calibration_complete_response(void *arg)
{
  httpd_req_t *req = (httpd_req_t *)arg;
  float bias[3];
  float scale[3];

  // Fetch the raw or scaled results from your IMU task
  imu_task_get_mag_calibration(bias, scale);

  char resp_json[256];
  snprintf(resp_json, sizeof(resp_json),
      "{\"status\":\"success\","
      "\"bias\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},"
      "\"scale\":{\"x\":%.4f,\"y\":%.4f,\"z\":%.4f}}",
      bias[0], bias[1], bias[2],
      scale[0], scale[1], scale[2]);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, resp_json, HTTPD_RESP_USE_STRLEN);
  httpd_req_async_handler_complete(req);
}

static void
on_mag_calibration_complete_cb(void *arg)
{
  httpd_queue_work(_server, mag_calibration_complete_response, arg);
  //mag_calibration_complete_response(arg);
}

static esp_err_t
mag_cal_post_handler(httpd_req_t *req)
{
  httpd_req_t *async_req;
  esp_err_t err = httpd_req_async_handler_begin(req, &async_req);
  if (err != ESP_OK) return err;

  // Start the Magnetometer collection mode
  imu_task_start_mag_calibration(on_mag_calibration_complete_cb, async_req);

  return ESP_OK;
}

static const httpd_uri_t mag_cal_uri =
{
  .uri       = "/api/calibrate/mag",
  .method    = HTTP_POST,
  .handler   = mag_cal_post_handler,
  .user_ctx  = NULL
};

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Accelerometer calibration
//
////////////////////////////////////////////////////////////////////////////////////////////////////////
static void
accel_capture_done_worker(void *arg)
{
  httpd_req_t *async_req = (httpd_req_t *)arg;

  // We are now safe in the HTTP context
  httpd_resp_set_type(async_req, "application/json");
  httpd_resp_sendstr(async_req, "{\"status\":\"captured\"}");

  // Finalize the async handle
  httpd_req_async_handler_complete(async_req);
}

static void
on_accel_calibration_complete_cb(void *arg)
{
  httpd_queue_work(_server, accel_capture_done_worker, arg);
}

static esp_err_t
accel_cal_post_handler(httpd_req_t *req)
{
  char query[32]; // Tight and stack-safe
  bool reset = false;
  char side[8] = {0}; 

  // 1. Extract the query string from the URI
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {

    // 2. Parse 'reset' (returns ESP_OK if found)
    char r_val[4];
    if (httpd_query_key_value(query, "reset", r_val, sizeof(r_val)) == ESP_OK) {
      reset = (r_val[0] == '1');
    }

    // 3. Parse 'side' (e.g., "+X", "-Z")
    httpd_query_key_value(query, "side", side, sizeof(side));
  }

  // 4. Enter Async Mode
  httpd_req_t *async_req;
  esp_err_t err = httpd_req_async_handler_begin(req, &async_req);
  if (err != ESP_OK) return err;

  imu_task_start_accel_calibration(reset, on_accel_calibration_complete_cb, async_req);

  return ESP_OK;
}

static esp_err_t
accel_cal_finish_post_handler(httpd_req_t *req)
{
  bool status;
  float offset[3], scale[3];
  char resp_json[256];

  // 1. Run the solver logic (LU decomposition/Sphere Fit)
  status = imu_task_finish_accel_calibration();

  if (status) {
    // 2. SUCCESS: Get the fresh params
    imu_task_get_accel_calibration(offset, scale);

    // Build success response
    snprintf(resp_json, sizeof(resp_json), 
        "{\"status\":\"success\",\"offset\":[%.4f,%.4f,%.4f],\"scale\":[%.4f,%.4f,%.4f]}",
        offset[0], offset[1], offset[2], scale[0], scale[1], scale[2]);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp_json);

    ESP_LOGI(TAG, "Accel calibration successful and saved.");
  } 
  else {
    // 3. FAILURE: Math failed or data incomplete
    const char *error_msg = "{\"status\":\"error\",\"message\":\"Math failure: singular matrix or insufficient data\"}";

    // Set 400 Bad Request or 500 Internal Error so Vue 'response.ok' becomes false
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, error_msg);

    ESP_LOGW(TAG, "Accel calibration failed.");
  }

  return ESP_OK;
}

static const httpd_uri_t accel_cal_uri =
{
  .uri       = "/api/calibrate/accel",
  .method    = HTTP_POST,
  .handler   = accel_cal_post_handler,
  .user_ctx  = NULL
};

static const httpd_uri_t accel_cal_finish_uri =
{
  .uri       = "/api/calibrate/accel/finish",
  .method    = HTTP_POST,
  .handler   = accel_cal_finish_post_handler,
  .user_ctx  = NULL
};

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// HTTP Web Settings 
//
////////////////////////////////////////////////////////////////////////////////////////////////////////
static esp_err_t
settings_get_handler(httpd_req_t *req)
{
  float acc_off[3], acc_scale[3];
  float gyro_off[3];
  float mag_bias[3], mag_scale[3];
  imu_engine_config_t e;
  imu_wifi_config_t w;

  // Use a slightly larger buffer (1024) to safely fit all WiFi strings and floats
  static char json[1024];

  // Fetch all Hardware Truth
  imu_task_get_accel_calibration(acc_off, acc_scale);
  imu_task_get_gyro_calibration(gyro_off);
  imu_task_get_mag_calibration(mag_bias, mag_scale);

  imu_config_get_imu_engine_config(&e);
  imu_config_get_wifi_config(&w);

  int len = snprintf(json, sizeof(json),
      "{"
      "\"calibration\":{"
      "\"accel_off\":[%.3f,%.3f,%.3f],\"accel_scale\":[%.3f,%.3f,%.3f],"
      "\"gyro_off\":[%.3f,%.3f,%.3f],\"mag_bias\":[%.3f,%.3f,%.3f],"
      "\"mag_scale\":[%.3f,%.3f,%.3f]"
      "},"
      "\"imu\":{"
      "\"ahrs_mode\":\"%s\",\"beta\":%.3f,\"twoKp\":%.3f,\"twoKi\":%.3f,\"mag_declination\":%.4f"
      "},"
      "\"wifi\":{"
      "\"sta_enabled\":%s,\"sta_ssid\":\"%s\",\"sta_password\":\"%s\","
      "\"ap_ssid\":\"%s\",\"ap_password\":\"%s\",\"ap_ip\":\"%s\","
      "\"ap_mask\":\"%s\",\"channel\":%d"
      "}"
      "}",
      // Calibration
      acc_off[0], acc_off[1], acc_off[2], acc_scale[0], acc_scale[1], acc_scale[2],
      gyro_off[0], gyro_off[1], gyro_off[2], mag_bias[0], mag_bias[1], mag_bias[2],
      mag_scale[0], mag_scale[1], mag_scale[2],
      // IMU Engine
      (e.ahrs_mode == IMU_AHRS_MODE_MADGWICK) ? "Madgwick" : "Mahony",
      e.madgwick_beta, e.mahony_kp, e.mahony_ki, e.mag_declination,
      // WiFi
      w.sta_enabled ? "true" : "false", w.sta_ssid, w.sta_password,
      w.ap_ssid, w.ap_password, w.ap_ip, w.ap_mask, w.channel
        );

  if (len < 0 || len >= sizeof(json))
  {
    ESP_LOGE(TAG, "JSON buffer overflow! Required: %d", len);
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json, len);
}

static const httpd_uri_t settings_get_uri =
{
  .uri       = "/api/settings",
  .method    = HTTP_GET,
  .handler   = settings_get_handler,
  .user_ctx  = NULL
};

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// HTTP IMU AHRS config 
//
////////////////////////////////////////////////////////////////////////////////////////////////////////
static esp_err_t
imu_settings_post_handler(httpd_req_t *req)
{
  static char buf[512]; // Stack buffer for IMU JSON
  int ret, remaining = req->content_len;

  if (remaining >= sizeof(buf)) return httpd_resp_send_500(req);

  // 1. Ingest Payload
  while (remaining > 0)
  {
    if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf) - 1))) <= 0) return ESP_FAIL;
    remaining -= ret;
  }
  buf[req->content_len] = '\0';

  // 2. JSMN Setup
  jsmn_parser p;
  jsmntok_t t[32]; // 32 tokens is plenty for the IMU block

  jsmn_init(&p);
  int r = jsmn_parse(&p, buf, strlen(buf), t, 32);
  if (r < 0) return ESP_FAIL;

  // 3. Prepare the engine config struct
  imu_engine_config_t engine_cfg;

  // 4. SURGICAL PARSE (Matching Vue keys)
  for (int i = 1; i < r; i++)
  {
    if (jsoneq(buf, &t[i], "ahrs_mode") == 0)
    {
      engine_cfg.ahrs_mode = (strncmp(buf + t[i+1].start, "Madgwick", 8) == 0) 
        ? IMU_AHRS_MODE_MADGWICK : IMU_AHRS_MODE_MAHONY;
      i++;
    } 
    else if (jsoneq(buf, &t[i], "beta") == 0)
    {
      engine_cfg.madgwick_beta = strtof(buf + t[i+1].start, NULL);
      i++;
    }
    else if (jsoneq(buf, &t[i], "twoKp") == 0)
    {
      engine_cfg.mahony_kp = strtof(buf + t[i+1].start, NULL);
      i++;
    }
    else if (jsoneq(buf, &t[i], "twoKi") == 0)
    {
      engine_cfg.mahony_ki = strtof(buf + t[i+1].start, NULL);
      i++;
    }
    else if (jsoneq(buf, &t[i], "mag_declination") == 0)
    {
      engine_cfg.mag_declination = strtof(buf + t[i+1].start, NULL);
      i++;
    }
  }

  imu_task_config_ahrs(&engine_cfg);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, buf, strlen(buf));
}

static const httpd_uri_t imu_settings = 
{
  .uri       = "/api/settings/imu",
  .method    = HTTP_POST,
  .handler   = imu_settings_post_handler,
  .user_ctx  = NULL
};

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// HTTP WIFI Config
//
////////////////////////////////////////////////////////////////////////////////////////////////////////
static void
reboot_task(void *pvParameters)
{
  vTaskDelay(pdMS_TO_TICKS(2000));
  ESP_LOGI("web_server", "Rebooting system for WiFi changes...");
  esp_restart();
}

static esp_err_t
wifi_settings_post_handler(httpd_req_t *req)
{
  static char buf[1024]; // WiFi JSON needs a larger buffer for SSIDs/PWs
  int ret, remaining = req->content_len;

  if (remaining >= sizeof(buf)) return httpd_resp_send_500(req);

  // 1. Ingest Payload
  while (remaining > 0)
  {
    if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf) - 1))) <= 0) return ESP_FAIL;
    remaining -= ret;
  }
  buf[req->content_len] = '\0';

  // 2. JSMN Setup
  jsmn_parser p;
  jsmntok_t t[64]; // More tokens for nested WiFi object

  jsmn_init(&p);
  int r = jsmn_parse(&p, buf, strlen(buf), t, 64);

  if (r < 0) return ESP_FAIL;

  // 3. Prepare local WiFi config
  imu_wifi_config_t w_cfg;

  for (int i = 1; i < r; i++)
  {
    int len = t[i+1].end - t[i+1].start;
    char *val = buf + t[i+1].start;

    if (jsoneq(buf, &t[i], "sta_enabled") == 0)
    {
      w_cfg.sta_enabled = (strncmp(val, "true", 4) == 0);
      i++;
    }
    else if (jsoneq(buf, &t[i], "sta_ssid") == 0)
    {
      snprintf(w_cfg.sta_ssid, sizeof(w_cfg.sta_ssid), "%.*s", len, val);
      i++;
    }
    else if (jsoneq(buf, &t[i], "sta_password") == 0)
    {
      snprintf(w_cfg.sta_password, sizeof(w_cfg.sta_password), "%.*s", len, val);
      i++;
    }
    else if (jsoneq(buf, &t[i], "ap_ssid") == 0)
    {
      snprintf(w_cfg.ap_ssid, sizeof(w_cfg.ap_ssid), "%.*s", len, val);
      i++;
    }
    else if (jsoneq(buf, &t[i], "ap_password") == 0)
    {
      snprintf(w_cfg.ap_password, sizeof(w_cfg.ap_password), "%.*s", len, val);
      i++;
    }
    else if (jsoneq(buf, &t[i], "ap_ip") == 0)
    {
      snprintf(w_cfg.ap_ip, sizeof(w_cfg.ap_ip), "%.*s", len, val);
      i++;
    }
    else if (jsoneq(buf, &t[i], "ap_mask") == 0)
    {
      snprintf(w_cfg.ap_mask, sizeof(w_cfg.ap_mask), "%.*s", len, val);
      i++;
    }
    else if (jsoneq(buf, &t[i], "channel") == 0)
    {
      w_cfg.channel = atoi(val);
      i++;
    }
  }

  // 5. COMMIT & REBOOT
  imu_config_update_wifi_config(&w_cfg);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  esp_err_t res = httpd_resp_send(req, buf, strlen(buf));

  // 6. DELAYED RESET
  // We start a small task to wait 2 seconds before rebooting,
  // giving the web client enough time to receive the HTTP 200 OK.
  xTaskCreate(reboot_task, "reboot_task", 2048, NULL, 5, NULL);

  return res;
}

static httpd_uri_t wifi_settings = 
{
  .uri       = "/api/settings/wifi",
  .method    = HTTP_POST,
  .handler   = wifi_settings_post_handler,
  .user_ctx  = NULL
};

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// HTTP Web Socket Handler for Realtime IMU data
//
////////////////////////////////////////////////////////////////////////////////////////////////////////
static esp_err_t
ws_imu_handler(httpd_req_t *req)
{
  if (req->method == HTTP_GET)
  {
    // This is the handshake; return ESP_OK to switch protocols
    ESP_LOGI(TAG, "Handshake done, client connected");

    /*
     if it takes a long time even after setting short keep-alive
     during http server initialization.

     #include <lwip/sockets.h>

     int keep_idle = 5;    // 5s idle
     int keep_intvl = 2;   // 2s interval
     int keep_cnt = 3;     // 3 fails
     int keep_alive = 1;   // Enable

     setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keep_alive, sizeof(int));
     setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(int));
     setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keep_intvl, sizeof(int));
     setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keep_cnt, sizeof(int));
     */
    int fd = httpd_req_to_sockfd(req);
    if (fd < 0) return ESP_FAIL;

    // 2. Disable Nagle's Algorithm (TCP_NODELAY)
    int nodelay = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) < 0)
    {
      ESP_LOGE(TAG, "failed to set socket nodelay");
    }

    return ESP_OK;
  }


  httpd_ws_frame_t ws_pkt;
  uint8_t *buf = NULL;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

  // First call to get the frame length
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK)
  {
    return ret;
  }

  if (ws_pkt.len > 0)
  {
    buf = calloc(1, ws_pkt.len + 1);
    ws_pkt.payload = buf;

    // Second call to actually receive data
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret == ESP_OK)
    {
      // ESP_LOGI("WS", "Received packet: %s", ws_pkt.payload);
    }
    free(buf);
  }
  return ret;
}

//
// this runs in HTTPD task
//
static void
imu_tx_telemetry_queue_work(void* arg)
{
  size_t clients = 8; // Max clients to check
  int fds[8];
  httpd_ws_frame_t packet = 
  {
    .payload = (uint8_t *)arg,
    .len = sizeof(imu_telemetry_pkt_t),
    .type = HTTPD_WS_TYPE_BINARY,
    .final = true
  };

  // Get list of all secure/active file descriptors
  if (httpd_get_client_list(_server, &clients, fds) == ESP_OK)
  {
    for (size_t i = 0; i < clients; i++)
    {
      // Only send if the descriptor is actually a WebSocket
      if (httpd_ws_get_fd_info(_server, fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET)
      {
        esp_err_t ret = httpd_ws_send_frame_async(_server, fds[i], &packet);
        if (ret != ESP_OK)
        {
          ESP_LOGW(TAG, "Killing Ghost Client FD: %d",fds[i]);
          httpd_sess_trigger_close(_server, fds[i]); 
        }
      }
    }
  }
  free(arg);
}

void
ws_broadcast_imu_update(imu_telemetry_pkt_t* pkt)
{
  imu_telemetry_pkt_t* copy;

  copy = malloc(sizeof(imu_telemetry_pkt_t));
  if(copy == NULL)
  {
    ESP_LOGE(TAG, "failed to malloc telemetry pkt");
    return;
  }

  memcpy(copy, pkt, sizeof(imu_telemetry_pkt_t));

  esp_err_t ret = httpd_queue_work(_server, imu_tx_telemetry_queue_work, copy);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "failed to queue telemetry work");
    free(copy);
  }
}

static httpd_uri_t ws_imu_uri = {
  .uri = "/ws_imu",
  .method = HTTP_GET,
  .handler = ws_imu_handler,
  .user_ctx = NULL,
  .is_websocket = true,
};

void
web_server_init(void)
{
  /* Allocate memory for server data */
  server_data = calloc(1, sizeof(struct file_server_data));
  if (!server_data)
  {
    ESP_LOGE(TAG, "Failed to allocate memory for server data");
    return;
  }
  strlcpy(server_data->base_path, "/spiffs", sizeof(server_data->base_path));

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  config.keep_alive_enable = true;
  config.keep_alive_idle = 5;       // How long to wait of total silence before probing (Seconds)
  config.keep_alive_interval = 2;   // Interval between individual probes (Seconds)
  config.keep_alive_count = 3;      // How many failed probes before killing the socket

  config.max_uri_handlers   = 32;
  config.max_open_sockets   = 20;
  config.lru_purge_enable   = true;
  config.uri_match_fn = httpd_uri_match_wildcard;

  ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
  if (httpd_start(&_server, &config) == ESP_OK)
  {
    common_get_uri.user_ctx = server_data;

    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(_server, &gyro_cal_uri);
    httpd_register_uri_handler(_server, &mag_cal_uri);
    httpd_register_uri_handler(_server, &accel_cal_uri);
    httpd_register_uri_handler(_server, &accel_cal_finish_uri);
    httpd_register_uri_handler(_server, &ws_imu_uri);
    httpd_register_uri_handler(_server, &settings_get_uri);
    httpd_register_uri_handler(_server, &imu_settings);
    httpd_register_uri_handler(_server, &wifi_settings);
    httpd_register_uri_handler(_server, &common_get_uri);
  }
}
