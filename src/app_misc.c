#include "app_config.h"
#include "esp_adc_cal.h"
#include "app_misc.h"

void check_efuse(void)
{
#if CONFIG_IDF_TARGET_ESP32
  //Check if TP is burned into eFuse
  if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK)
  {
    printf("eFuse Two Point: Supported\n");
  }
  else
  {
    printf("eFuse Two Point: NOT supported\n");
  }
  //Check Vref is burned into eFuse
  if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK)
  {
    printf("eFuse Vref: Supported\n");
  }
  else
  {
    printf("eFuse Vref: NOT supported\n");
  }
#elif CONFIG_IDF_TARGET_ESP32S2
  if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK)
  {
    printf("eFuse Two Point: Supported\n");
  }
  else
  {
    printf("Cannot retrieve eFuse Two Point calibration values. Default calibration values will be used.\n");
  }
#else
#error "This example is configured for ESP32/ESP32S2."
#endif
}