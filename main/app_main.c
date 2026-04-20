/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "imu_task.h"
#include "wifi.h"
#include "mount.h"
#include "web_server.h"
#include "imu_config.h"

static const char *TAG = "app_main";

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO

static led_strip_handle_t led_strip;

static uint32_t t = 0;  // time counter

size_t total_sram_in_KB = 0;
volatile uint32_t cpu_usage = 0;

static void
change_led(void)
{
  // Use sine waves with phase offsets to generate flamboyant colors
  uint8_t r = (uint8_t)(127.5 * (1 + sin(0.1 * t)));
  uint8_t g = (uint8_t)(127.5 * (1 + sin(0.1 * t + 2.0 * M_PI / 3.0)));
  uint8_t b = (uint8_t)(127.5 * (1 + sin(0.1 * t + 4.0 * M_PI / 3.0)));

  led_strip_set_pixel(led_strip, 0, r, g, b);
  led_strip_refresh(led_strip);

  t++;  // advance time
}

#if 0
static void
turnoff_led(void)
{
  led_strip_set_pixel(led_strip, 0, 0, 0, 0);
  led_strip_refresh(led_strip);
}
#endif

static void
configure_led(void)
{
  ESP_LOGI(TAG, "Example configured to blink addressable LED!");
  /* LED strip initialization with the GPIO and pixels number*/
  led_strip_config_t strip_config = {
    .strip_gpio_num = BLINK_GPIO,
    .max_leds = 1, // at least one LED on board
  };

  led_strip_rmt_config_t rmt_config = {
    .resolution_hz = 10 * 1000 * 1000, // 10MHz
    .flags.with_dma = false,
  };

  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
  /* Set all LED off to clear all pixels */
  led_strip_clear(led_strip);
}

static esp_err_t
print_real_time_stats(TickType_t xTicksToWait, uint32_t* usage)
{
  TaskStatus_t *start_array = NULL, *end_array = NULL;
  UBaseType_t start_array_size, end_array_size;
  configRUN_TIME_COUNTER_TYPE start_run_time, end_run_time;
  esp_err_t ret;

  //Allocate array to store current task states
  #define ARRAY_SIZE_OFFSET   5

  *usage = 0;

  start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
  start_array = malloc(sizeof(TaskStatus_t) * start_array_size);
  if (start_array == NULL) {
    ret = ESP_ERR_NO_MEM;
    goto exit;
  }
  //Get current task states
  start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
  if (start_array_size == 0) {
    ret = ESP_ERR_INVALID_SIZE;
    goto exit;
  }

  vTaskDelay(xTicksToWait);

  //Allocate array to store tasks states post delay
  end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
  end_array = malloc(sizeof(TaskStatus_t) * end_array_size);
  if (end_array == NULL) {
    ret = ESP_ERR_NO_MEM;
    goto exit;
  }
  //Get post delay task states
  end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
  if (end_array_size == 0) {
    ret = ESP_ERR_INVALID_SIZE;
    goto exit;
  }

  //Calculate total_elapsed_time in units of run time stats clock period.
  uint32_t total_elapsed_time = (end_run_time - start_run_time);
  if (total_elapsed_time == 0) {
    ret = ESP_ERR_INVALID_STATE;
    goto exit;
  }

  //Match each task in start_array to those in the end_array
  for (int i = 0; i < start_array_size; i++) {
    int k = -1;
    for (int j = 0; j < end_array_size; j++) {
      if (start_array[i].xHandle == end_array[j].xHandle) {
        k = j;
        //Mark that task have been matched by overwriting their handles
        start_array[i].xHandle = NULL;
        end_array[j].xHandle = NULL;
        break;
      }
    }
    //Check if matching task found
    if (k >= 0) {
      uint32_t task_elapsed_time = end_array[k].ulRunTimeCounter - start_array[i].ulRunTimeCounter;
      uint32_t percentage_time = (task_elapsed_time * 100UL) / (total_elapsed_time * CONFIG_FREERTOS_NUMBER_OF_CORES);

      // printf("| %s | %"PRIu32" | %"PRIu32"%%\n", start_array[i].pcTaskName, task_elapsed_time, percentage_time);
      //
      if(strcmp(start_array[i].pcTaskName, "IDLE") == 0)
      {
        *usage = (uint32_t)(100 - percentage_time);
        break;
      }
    }
  }

  ret = ESP_OK;

exit:    //Common return path
  free(start_array);
  free(end_array);
  return ret;
}

static void
stats_task(void *arg)
{
  uint32_t usage;

  //Print real time stats periodically
  while (1) {
    if (print_real_time_stats(pdMS_TO_TICKS(1000), &usage) == ESP_OK) {
      // printf("Real time stats obtained %ld\n", usage);
      // ESP_LOGI(TAG, "CPU Usage %ld", usage);
      cpu_usage = usage;
    } else {
      // printf("Error getting real time stats\n");
    }
    // vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void
app_main(void)
{
  total_sram_in_KB = esp_get_free_internal_heap_size() / 1024;
  ESP_LOGI(TAG, "Free heap %dKB", total_sram_in_KB);

  imu_config_init();

  configure_led();

  imu_task_init();

  ESP_LOGI(TAG, "Free heap after init %dKB", esp_get_free_internal_heap_size() / 1024);
  xTaskCreatePinnedToCore(stats_task, "stats", 4096, NULL, 3, NULL, tskNO_AFFINITY);

  wifi_init();
  mount_storage("/spiffs");
  web_server_init();

  while (1)
  {
    change_led();
    vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
  }
}
