#include "esp_log.h"
#include "esp_err.h"
#include "app_http.h"

static const char *TAG = "http";

/* WEB APP SPECIFIC CODE STARTS HERE */
const char HTML[] = "<!DOCTYPE html><html><body><link rel=\'stylesheet\' href=\'https://maxcdn.bootstrapcdn.com/bootstrap/3.3.5/css/bootstrap.min.css\'><link rel=\'stylesheet\' href=\'https://maxcdn.bootstrapcdn.com/bootstrap/3.3.5/css/bootstrap-theme.min.css\'><script src=\'https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.9.0/Chart.min.js\'></script><div class=\'row container\'><div class=\'container col-sm-12\'><h1>ESP32 - ADC1</h1></div><div class=\'container col-sm-12\'><canvas id=\'myChart\' width=\'800\' height=\'400\'></canvas></div></div><script>var myLineChart;if(\'WebSocket\'in window){var DATA=[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],ws=new WebSocket(\'ws://\'+window.location.hostname+\':8080/\');ws.onopen=function(){console.log(\'WebSocket opened\')},ws.onmessage=function(e){var o=e.data;console.log(o),DATA.shift(),DATA.push(parseInt(o)),myLineChart.update()},ws.onclose=function(){console.log(\'WebSocket closed\')}}var ctxL=document.getElementById(\'myChart\').getContext(\'2d\'),myLineChart=new Chart(ctxL,{type:\'line\',data:{labels:[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100],datasets:[{label:\'\',data:DATA,backgroundColor:[\'rgba(105, 0, 132, .2)\'],borderColor:[\'rgba(200, 99, 132, .7)\'],borderWidth:2}]},options:{scales:{yAxes:[{ticks:{min:0,max:5e3,stepSize:100}}]},animation:{duration:0},scaleOverride:!0}});</script></body></html>";

static esp_err_t chart_get_handler(httpd_req_t *req)
{
  /* Set some custom headers */
  httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
  httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

  /* Send response with custom headers and body set as the
     * string passed in user context*/
  const char *resp_str = (const char *)req->user_ctx;
  httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

  /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
  if (httpd_req_get_hdr_value_len(req, "Host") == 0)
  {
    ESP_LOGI(TAG, "Request headers lost");
  }
  return ESP_OK;
}

static const httpd_uri_t chart = {
    .uri = "/chart",
    .method = HTTP_GET,
    .handler = chart_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = HTML};
/* WEB APP SPECIFIC CODE ENDS HERE */


httpd_handle_t start_webserver(void)
{
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  // Start the httpd server
  ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK)
  {
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

void stop_webserver(httpd_handle_t server)
{
  // Stop the httpd server
  httpd_stop(server);
}

void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
  httpd_handle_t *server = (httpd_handle_t *)arg;
  if (*server)
  {
    ESP_LOGI(TAG, "Stopping webserver");
    stop_webserver(*server);
    *server = NULL;
  }
}

void connect_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
  httpd_handle_t *server = (httpd_handle_t *)arg;
  if (*server == NULL)
  {
    ESP_LOGI(TAG, "Starting webserver");
    *server = start_webserver();
  }
}