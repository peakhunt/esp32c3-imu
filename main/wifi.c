#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip4_addr.h"

#include "wifi.h"
#include "imu_config.h"

static const char *TAG = "wifi";

static void
wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
    esp_wifi_connect();
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    ESP_LOGI(TAG, "retry to connect to the AP");
    esp_wifi_connect();
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));

    ESP_LOGI(TAG, "Got IP. Killing the shitty AP mode to save 1kHz timing...");
    esp_wifi_set_mode(WIFI_MODE_STA);
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
  {
    wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
    ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
        MAC2STR(event->mac), event->aid);
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
  {
    wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
    ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d",
        MAC2STR(event->mac), event->aid, event->reason);
  }
}

void
wifi_init(void)
{
  imu_wifi_config_t cfg_nvs;
  imu_config_get_wifi_config(&cfg_nvs);

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // 1. Setup Netifs
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
  (void)sta_netif; // Created and registered, handle not used locally

  // 2. Configure AP Static IP
  esp_netif_ip_info_t ip_info;
  memset(&ip_info, 0, sizeof(esp_netif_ip_info_t));
  esp_netif_str_to_ip4(cfg_nvs.ap_ip, &ip_info.ip);
  esp_netif_str_to_ip4(cfg_nvs.ap_mask, &ip_info.netmask);
  ip_info.gw.addr = ip_info.ip.addr; // Set gateway as self

  esp_netif_dhcps_stop(ap_netif);
  esp_netif_set_ip_info(ap_netif, &ip_info);
  esp_netif_dhcps_start(ap_netif);

  // 3. WiFi Init
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        NULL,
        NULL));

  // Setup AP configuration
  wifi_config_t ap_config = { 0 };
  strlcpy((char *)ap_config.ap.ssid, cfg_nvs.ap_ssid, sizeof(ap_config.ap.ssid));
  ap_config.ap.ssid_len = strlen(cfg_nvs.ap_ssid);
  ap_config.ap.channel = cfg_nvs.channel;
  strlcpy((char *)ap_config.ap.password, cfg_nvs.ap_password, sizeof(ap_config.ap.password));
  ap_config.ap.max_connection = 4;
  ap_config.ap.authmode = (strlen(cfg_nvs.ap_password) == 0) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

  // Setup STA if enabled
  if (cfg_nvs.sta_enabled)
  {
    wifi_config_t sta_config = { 0 };
    strlcpy((char *)sta_config.sta.ssid, cfg_nvs.sta_ssid, sizeof(sta_config.sta.ssid));
    strlcpy((char *)sta_config.sta.password, cfg_nvs.sta_password, sizeof(sta_config.sta.password));
    sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    sta_config.sta.pmf_cfg.capable = true;
    sta_config.sta.pmf_cfg.required = false;
#if 0
    sta_config.sta.listen_interval = 1;  // wake up for EVERRY beacon, no skipping
#endif

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
#if 0   // my fucking router sucks with 802.11n
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G));
#endif
  }
  else
  {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
  }

  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

  ESP_LOGI(TAG, "wifi_init complete. AP SSID:%s, Mode:%s", 
      cfg_nvs.ap_ssid, cfg_nvs.sta_enabled ? "AP+STA" : "AP Only");
}
