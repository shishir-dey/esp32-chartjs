#ifndef APP_WEBSOCKET_H
#define APP_WEBSOCKET_H

#include "websocket_server.h"

void http_serve(struct netconn *conn);

/* FreeRTOS user task */
void server_task(void *pvParameters);
void server_handle_task(void *pvParameters);
void websocket_callback(uint8_t num, WEBSOCKET_TYPE_t type, char *msg, uint64_t len);

#endif