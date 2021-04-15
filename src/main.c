/* Reference from Simple HTTP Server Example provided by Espressif */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sys/param.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_http_server.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/api.h"

#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

/* App includes */
#include "websocket_server.h"
#include "app_misc.h"
#include "app_config.h"
#include "app_websocket.h"
#include "app_wifi.h"
#include "app_http.h"
#include "app_adc.h"

/* FreeRTOS user task */
static void count_task(void *pvParameters)
{
  const static char *TAG = "count_task";
  char out[20];
  int len;
  int clients;
  const static char *word = "%i";
  uint8_t n = 0;
  const int DELAY = 100 / portTICK_PERIOD_MS; // 0.1 second

  ESP_LOGI(TAG, "starting task");
  for (;;)
  {
    len = sprintf(out, word, getADCRawValue());
    clients = ws_server_send_text_all(out, len);
    if (clients > 0)
    {
      //ESP_LOGI(TAG,"sent: \"%s\" to %i clients",out,clients);
    }
    n++;
    vTaskDelay(DELAY);
  }
}

void app_main(void)
{
  ESP_ERROR_CHECK(nvs_flash_init());

  check_efuse();
  adcInit();

  // ESP_ERROR_CHECK(esp_netif_init());
  // ESP_ERROR_CHECK(esp_event_loop_create_default());

  /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
     * and re-start it upon connection.
     */
  // ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
  // ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));

  /* Start the server for the first time */
  printf("[DEBUG] Starting WiFi...\n");
  wifi_init_sta();
  ws_server_start();
  printf("[DEBUG] Starting web server...\n");
  xTaskCreate(&server_task, "server_task", 3000, NULL, 9, NULL);
  xTaskCreate(&server_handle_task, "server_handle_task", 4000, NULL, 6, NULL);
  xTaskCreate(&count_task, "count_task", 6000, NULL, 2, NULL);
  xTaskCreate(&adc_task, "adc_task", 6000, NULL, 2, NULL);
  (void)start_webserver();
}