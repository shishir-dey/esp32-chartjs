#ifndef APP_ADC
#define APP_ADC

#define DEFAULT_VREF 1100 //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES 64  //Multisampling

void adcInit(void);
uint32_t getADCRawValue(void);
void print_char_val_type(esp_adc_cal_value_t val_type);

/* FreeRTOS user task */
void adc_task(void *pvParameters);

#endif