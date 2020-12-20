/* Simple HTTP Server Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_eth.h"

#include <esp_http_server.h>


#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "websocket_server.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "lwip/api.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"


#include "string.h"

#include "websocket_server.h"

uint32_t adc_reading = 0;

static QueueHandle_t client_queue;
const static int client_queue_size = 10;

/* The examples use WiFi configuration that you can set via project configuration menu
   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      "TP-Link"
#define EXAMPLE_ESP_WIFI_PASS      "12345679"
#define EXAMPLE_ESP_MAXIMUM_RETRY  5


/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;

const char HTML[] = "<!DOCTYPE html><html><body><link rel=\'stylesheet\' href=\'https://maxcdn.bootstrapcdn.com/bootstrap/3.3.5/css/bootstrap.min.css\'><link rel=\'stylesheet\' href=\'https://maxcdn.bootstrapcdn.com/bootstrap/3.3.5/css/bootstrap-theme.min.css\'><script src=\'https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.9.0/Chart.min.js\'></script><div class=\'row container\'><div class=\'container col-sm-12\'><h1>ESP32 - ADC1</h1></div><div class=\'container col-sm-12\'><canvas id=\'myChart\' width=\'800\' height=\'400\'></canvas></div></div><script>var myLineChart;if(\'WebSocket\'in window){var DATA=[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],ws=new WebSocket(\'ws://\'+window.location.hostname+\':8080/\');ws.onopen=function(){console.log(\'WebSocket opened\')},ws.onmessage=function(e){var o=e.data;console.log(o),DATA.shift(),DATA.push(parseInt(o)),myLineChart.update()},ws.onclose=function(){console.log(\'WebSocket closed\')}}var ctxL=document.getElementById(\'myChart\').getContext(\'2d\'),myLineChart=new Chart(ctxL,{type:\'line\',data:{labels:[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100],datasets:[{label:\'\',data:DATA,backgroundColor:[\'rgba(105, 0, 132, .2)\'],borderColor:[\'rgba(200, 99, 132, .7)\'],borderWidth:2}]},options:{scales:{yAxes:[{ticks:{min:0,max:5e3,stepSize:100}}]},animation:{duration:0},scaleOverride:!0}});</script></body></html>";


#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling

static esp_adc_cal_characteristics_t *adc_chars;
#if CONFIG_IDF_TARGET_ESP32
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
#elif CONFIG_IDF_TARGET_ESP32S2
static const adc_channel_t channel = ADC_CHANNEL_6;     // GPIO7 if ADC1, GPIO17 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_13;
#endif
static const adc_atten_t atten = ADC_ATTEN_DB_0;
static const adc_unit_t unit = ADC_UNIT_1;


static void check_efuse(void)
{
#if CONFIG_IDF_TARGET_ESP32
    //Check if TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }
    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
#elif CONFIG_IDF_TARGET_ESP32S2
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("Cannot retrieve eFuse Two Point calibration values. Default calibration values will be used.\n");
    }
#else
#error "This example is configured for ESP32/ESP32S2."
#endif
}


static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

void websocket_callback(uint8_t num,WEBSOCKET_TYPE_t type,char* msg,uint64_t len) {
  const static char* TAG = "websocket_callback";
  int value;

  switch(type) {
    case WEBSOCKET_CONNECT:
      ESP_LOGI(TAG,"client %i connected!",num);
      break;
    case WEBSOCKET_DISCONNECT_EXTERNAL:
      ESP_LOGI(TAG,"client %i sent a disconnect message",num);
      break;
    case WEBSOCKET_DISCONNECT_INTERNAL:
      ESP_LOGI(TAG,"client %i was disconnected",num);
      break;
    case WEBSOCKET_DISCONNECT_ERROR:
      ESP_LOGI(TAG,"client %i was disconnected due to an error",num);
      break;
    case WEBSOCKET_TEXT:
      if(len) {
        switch(msg[0]) {
          case 'L':
            if(sscanf(msg,"L%i",&value)) {
              ESP_LOGI(TAG,"LED value: %i",value);
              ws_server_send_text_all_from_callback(msg,len); // broadcast it!
            }
        }
      }
      break;
    case WEBSOCKET_BIN:
      ESP_LOGI(TAG,"client %i sent binary message of size %i:\n%s",num,(uint32_t)len,msg);
      break;
    case WEBSOCKET_PING:
      ESP_LOGI(TAG,"client %i pinged us with message of size %i:\n%s",num,(uint32_t)len,msg);
      break;
    case WEBSOCKET_PONG:
      ESP_LOGI(TAG,"client %i responded to the ping",num);
      break;
  }
}

static void http_serve(struct netconn *conn) {
  const static char* TAG = "http_server";
  const static char HTML_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
//   const static char ERROR_HEADER[] = "HTTP/1.1 404 Not Found\nContent-type: text/html\n\n";
//   const static char JS_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/javascript\n\n";
//   const static char CSS_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/css\n\n";
  //const static char PNG_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/png\n\n";
//   const static char ICO_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/x-icon\n\n";
  //const static char PDF_HEADER[] = "HTTP/1.1 200 OK\nContent-type: application/pdf\n\n";
  //const static char EVENT_HEADER[] = "HTTP/1.1 200 OK\nContent-Type: text/event-stream\nCache-Control: no-cache\nretry: 3000\n\n";
  struct netbuf* inbuf;
  static char* buf;
  static uint16_t buflen;
  static err_t err;

  // default page
//   extern const uint8_t root_html_start[] asm("_binary_root_html_start");
//   extern const uint8_t root_html_end[] asm("_binary_root_html_end");
//   const uint32_t root_html_len = root_html_end - root_html_start;
  const uint8_t root_html_start[] = "Hello";
//   const uint8_t root_html_end[] asm("_binary_root_html_end");
  const uint32_t root_html_len = 5;

  // test.js
  // extern const uint8_t test_js_start[] asm("_binary_test_js_start");
  // extern const uint8_t test_js_end[] asm("_binary_test_js_end");
  // const uint32_t test_js_len = test_js_end - test_js_start;

  // test.css
  // extern const uint8_t test_css_start[] asm("_binary_test_css_start");
  // extern const uint8_t test_css_end[] asm("_binary_test_css_end");
  // const uint32_t test_css_len = test_css_end - test_css_start;

  // favicon.ico
  // extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
  // extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");
  // const uint32_t favicon_ico_len = favicon_ico_end - favicon_ico_start;

  // error page
  // extern const uint8_t error_html_start[] asm("_binary_error_html_start");
  // extern const uint8_t error_html_end[] asm("_binary_error_html_end");
  // const uint32_t error_html_len = error_html_end - error_html_start;

  netconn_set_recvtimeout(conn,1000); // allow a connection timeout of 1 second
  ESP_LOGI(TAG,"reading from client...");
  err = netconn_recv(conn, &inbuf);
  ESP_LOGI(TAG,"read from client");
  if(err==ERR_OK) {
    netbuf_data(inbuf, (void**)&buf, &buflen);
    if(buf) {
        printf("Received = %s\n", buf);

      // default page
      if     (strstr(buf,"GET / ")
          && !strstr(buf,"Upgrade: websocket")) {
        ESP_LOGI(TAG,"Sending /");
        netconn_write(conn, HTML_HEADER, sizeof(HTML_HEADER)-1,NETCONN_NOCOPY);
        netconn_write(conn, root_html_start,root_html_len,NETCONN_NOCOPY);
        netconn_close(conn);
        netconn_delete(conn);
        netbuf_delete(inbuf);
      }

    //   default page websocket
      else if(strstr(buf,"GET / ")
           && strstr(buf,"Upgrade: websocket")) {
        ESP_LOGI(TAG,"Requesting websocket on /");
        ws_server_add_client(conn,buf,buflen,"/",websocket_callback);
        netbuf_delete(inbuf);
      }

      // else if(strstr(buf,"GET /test.js ")) {
      //   ESP_LOGI(TAG,"Sending /test.js");
      //   netconn_write(conn, JS_HEADER, sizeof(JS_HEADER)-1,NETCONN_NOCOPY);
      //   netconn_write(conn, test_js_start, test_js_len,NETCONN_NOCOPY);
      //   netconn_close(conn);
      //   netconn_delete(conn);
      //   netbuf_delete(inbuf);
      // }

      // else if(strstr(buf,"GET /test.css ")) {
      //   ESP_LOGI(TAG,"Sending /test.css");
      //   netconn_write(conn, CSS_HEADER, sizeof(CSS_HEADER)-1,NETCONN_NOCOPY);
      //   netconn_write(conn, test_css_start, test_css_len,NETCONN_NOCOPY);
      //   netconn_close(conn);
      //   netconn_delete(conn);
      //   netbuf_delete(inbuf);
      // }

      // else if(strstr(buf,"GET /favicon.ico ")) {
      //   ESP_LOGI(TAG,"Sending favicon.ico");
      //   netconn_write(conn,ICO_HEADER,sizeof(ICO_HEADER)-1,NETCONN_NOCOPY);
      //   netconn_write(conn,favicon_ico_start,favicon_ico_len,NETCONN_NOCOPY);
      //   netconn_close(conn);
      //   netconn_delete(conn);
      //   netbuf_delete(inbuf);
      // }

      // else if(strstr(buf,"GET /")) {
      //   ESP_LOGI(TAG,"Unknown request, sending error page: %s",buf);
      //   netconn_write(conn, ERROR_HEADER, sizeof(ERROR_HEADER)-1,NETCONN_NOCOPY);
      //   netconn_write(conn, error_html_start, error_html_len,NETCONN_NOCOPY);
      //   netconn_close(conn);
      //   netconn_delete(conn);
      //   netbuf_delete(inbuf);
      // }

      // else {
      //   ESP_LOGI(TAG,"Unknown request");
      //   netconn_close(conn);
      //   netconn_delete(conn);
      //   netbuf_delete(inbuf);
      // }
    }
    else {
      ESP_LOGI(TAG,"Unknown request (empty?...)");
      netconn_close(conn);
      netconn_delete(conn);
      netbuf_delete(inbuf);
    }
  }
  else { // if err==ERR_OK
    ESP_LOGI(TAG,"error on read, closing connection");
    netconn_close(conn);
    netconn_delete(conn);
    netbuf_delete(inbuf);
  }
}

// handles clients when they first connect. passes to a queue
static void server_task(void* pvParameters) {
  const static char* TAG = "server_task";
  struct netconn *conn, *newconn;
  static err_t err;
  client_queue = xQueueCreate(client_queue_size,sizeof(struct netconn*));

  conn = netconn_new(NETCONN_TCP);
  netconn_bind(conn,NULL,8080);
  netconn_listen(conn);
  ESP_LOGI(TAG,"server listening");
  do {
    err = netconn_accept(conn, &newconn);
    ESP_LOGI(TAG,"new client");
    if(err == ERR_OK) {
      xQueueSendToBack(client_queue,&newconn,portMAX_DELAY);
      //http_serve(newconn);
    }
  } while(err == ERR_OK);
  netconn_close(conn);
  netconn_delete(conn);
  ESP_LOGE(TAG,"task ending, rebooting board");
  esp_restart();
}

// receives clients from queue, handles them
static void server_handle_task(void* pvParameters) {
  const static char* TAG = "server_handle_task";
  struct netconn* conn;
  ESP_LOGI(TAG,"task starting");
  for(;;) {
    xQueueReceive(client_queue,&conn,portMAX_DELAY);
    if(!conn) continue;
    http_serve(conn);
  }
  vTaskDelete(NULL);
}

static void count_task(void* pvParameters) {
  const static char* TAG = "count_task";
  char out[20];
  int len;
  int clients;
  const static char* word = "%i";
  uint8_t n = 0;
  const int DELAY = 100 / portTICK_PERIOD_MS; // 0.1 second

  ESP_LOGI(TAG,"starting task");
  for(;;) {
    len = sprintf(out,word,adc_reading);
    clients = ws_server_send_text_all(out,len);
    if(clients > 0) {
      //ESP_LOGI(TAG,"sent: \"%s\" to %i clients",out,clients);
    }
    n++;
    vTaskDelay(DELAY);
  }
}



static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));
    vEventGroupDelete(s_wifi_event_group);
}

/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */

// static const char *TAG = "example";

static esp_err_t chart_get_handler(httpd_req_t *req)
{
    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

static const httpd_uri_t chart = {
    .uri       = "/chart",
    .method    = HTTP_GET,
    .handler   = chart_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = HTML
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &chart);
        // httpd_register_uri_handler(server, &echo);
        // httpd_register_uri_handler(server, &ctrl);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}



static void adc_task(void *pvParameters) {
  while (1) {
        // uint32_t adc_reading = 0;
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            if (unit == ADC_UNIT_1) {
                adc_reading += adc1_get_raw((adc1_channel_t)channel);
            } else {
                int raw;
                adc2_get_raw((adc2_channel_t)channel, width, &raw);
                adc_reading += raw;
            }
        }
        adc_reading /= NO_OF_SAMPLES;
        //Convert adc_reading to voltage in mV
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
        // printf("Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


void app_main(void)
{
    static httpd_handle_t server = NULL;

    ESP_ERROR_CHECK(nvs_flash_init());

    check_efuse();

    //Configure ADC
    if (unit == ADC_UNIT_1) {
        adc1_config_width(width);
        adc1_config_channel_atten(channel, atten);
    } else {
        adc2_config_channel_atten((adc2_channel_t)channel, atten);
    }

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);
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
    // printf("[DEBUG] Starting web server...\n");
    xTaskCreate(&server_task, "server_task", 3000, NULL, 9, NULL);
    xTaskCreate(&server_handle_task, "server_handle_task", 4000, NULL, 6, NULL);
    xTaskCreate(&count_task, "count_task", 6000, NULL, 2, NULL);
    xTaskCreate(&adc_task, "adc_task", 6000, NULL, 2, NULL);
    server = start_webserver();
    
}