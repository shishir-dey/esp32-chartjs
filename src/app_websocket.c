#include <string.h>

#include "esp_log.h"
#include "app_websocket.h"

static QueueHandle_t client_queue;
const static int client_queue_size = 10;

void http_serve(struct netconn *conn)
{
  const static char *TAG = "http_server";
  const static char HTML_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
  // const static char ERROR_HEADER[] = "HTTP/1.1 404 Not Found\nContent-type: text/html\n\n";
  // const static char JS_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/javascript\n\n";
  // const static char CSS_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/css\n\n";
  // const static char PNG_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/png\n\n";
  // const static char ICO_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/x-icon\n\n";
  // const static char PDF_HEADER[] = "HTTP/1.1 200 OK\nContent-type: application/pdf\n\n";
  // const static char EVENT_HEADER[] = "HTTP/1.1 200 OK\nContent-Type: text/event-stream\nCache-Control: no-cache\nretry: 3000\n\n";
  struct netbuf *inbuf;
  static char *buf;
  static uint16_t buflen;
  static err_t err;

  // default page
  // extern const uint8_t root_html_start[] asm("_binary_root_html_start");
  // extern const uint8_t root_html_end[] asm("_binary_root_html_end");
  // const uint32_t root_html_len = root_html_end - root_html_start;
  // const uint8_t root_html_end[] asm("_binary_root_html_end");
  const uint8_t root_html_start[] = "Hello";
  const uint32_t root_html_len = 5;

  netconn_set_recvtimeout(conn, 1000); // allow a connection timeout of 1 second
  ESP_LOGI(TAG, "reading from client...");
  err = netconn_recv(conn, &inbuf);
  ESP_LOGI(TAG, "read from client");
  if (err == ERR_OK)
  {
    netbuf_data(inbuf, (void **)&buf, &buflen);
    if (buf)
    {
      printf("Received = %s\n", buf);

      // default page
      if (strstr(buf, "GET / ") && !strstr(buf, "Upgrade: websocket"))
      {
        ESP_LOGI(TAG, "Sending /");
        netconn_write(conn, HTML_HEADER, sizeof(HTML_HEADER) - 1, NETCONN_NOCOPY);
        netconn_write(conn, root_html_start, root_html_len, NETCONN_NOCOPY);
        netconn_close(conn);
        netconn_delete(conn);
        netbuf_delete(inbuf);
      }

      //   default page websocket
      else if (strstr(buf, "GET / ") && strstr(buf, "Upgrade: websocket"))
      {
        ESP_LOGI(TAG, "Requesting websocket on /");
        ws_server_add_client(conn, buf, buflen, "/", websocket_callback);
        netbuf_delete(inbuf);
      }
    }
    else
    {
      ESP_LOGI(TAG, "Unknown request (empty?...)");
      netconn_close(conn);
      netconn_delete(conn);
      netbuf_delete(inbuf);
    }
  }
  else
  { // if err==ERR_OK
    ESP_LOGI(TAG, "error on read, closing connection");
    netconn_close(conn);
    netconn_delete(conn);
    netbuf_delete(inbuf);
  }
}

/* FreeRTOS user task */
void server_task(void *pvParameters)
{
  // handles clients when they first connect. passes to a queue
  const static char *TAG = "server_task";
  struct netconn *conn, *newconn;
  static err_t err;
  client_queue = xQueueCreate(client_queue_size, sizeof(struct netconn *));

  conn = netconn_new(NETCONN_TCP);
  netconn_bind(conn, NULL, 8080);
  netconn_listen(conn);
  ESP_LOGI(TAG, "server listening");
  do
  {
    err = netconn_accept(conn, &newconn);
    ESP_LOGI(TAG, "new client");
    if (err == ERR_OK)
    {
      xQueueSendToBack(client_queue, &newconn, portMAX_DELAY);
      //http_serve(newconn);
    }
  } while (err == ERR_OK);
  netconn_close(conn);
  netconn_delete(conn);
  ESP_LOGE(TAG, "task ending, rebooting board");
  esp_restart();
}

/* FreeRTOS user task */
void server_handle_task(void *pvParameters)
{
  // receives clients from queue, handles them
  const static char *TAG = "server_handle_task";
  struct netconn *conn;
  ESP_LOGI(TAG, "task starting");
  for (;;)
  {
    xQueueReceive(client_queue, &conn, portMAX_DELAY);
    if (!conn)
      continue;
    http_serve(conn);
  }
  vTaskDelete(NULL);
}

void websocket_callback(uint8_t num, WEBSOCKET_TYPE_t type, char *msg, uint64_t len)
{
  const static char *TAG = "websocket_callback";
  int value;

  switch (type)
  {
  case WEBSOCKET_CONNECT:
    ESP_LOGI(TAG, "client %i connected!", num);
    break;
  case WEBSOCKET_DISCONNECT_EXTERNAL:
    ESP_LOGI(TAG, "client %i sent a disconnect message", num);
    break;
  case WEBSOCKET_DISCONNECT_INTERNAL:
    ESP_LOGI(TAG, "client %i was disconnected", num);
    break;
  case WEBSOCKET_DISCONNECT_ERROR:
    ESP_LOGI(TAG, "client %i was disconnected due to an error", num);
    break;
  case WEBSOCKET_TEXT:
    if (len)
    {
      switch (msg[0])
      {
      case 'L':
        if (sscanf(msg, "L%i", &value))
        {
          ESP_LOGI(TAG, "LED value: %i", value);
          ws_server_send_text_all_from_callback(msg, len); // broadcast it!
        }
      }
    }
    break;
  case WEBSOCKET_BIN:
    ESP_LOGI(TAG, "client %i sent binary message of size %i:\n%s", num, (uint32_t)len, msg);
    break;
  case WEBSOCKET_PING:
    ESP_LOGI(TAG, "client %i pinged us with message of size %i:\n%s", num, (uint32_t)len, msg);
    break;
  case WEBSOCKET_PONG:
    ESP_LOGI(TAG, "client %i responded to the ping", num);
    break;
  }
}