#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "driver/adc.h"

/* User configs */
#define ESP_WIFI_SSID "TEST_SSID"
#define ESP_WIFI_PASS "12345678"
#define ESP_MAXIMUM_RETRY 5

#if CONFIG_IDF_TARGET_ESP32
static const adc_channel_t channel = ADC_CHANNEL_6; //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
#elif CONFIG_IDF_TARGET_ESP32S2
static const adc_channel_t channel = ADC_CHANNEL_6; // GPIO7 if ADC1, GPIO17 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_13;
#endif

#endif