#ifndef APP_WIFI
#define APP_WIFI


void wifi_init_sta(void);
void wifi_event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data);

#endif