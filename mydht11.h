#ifndef __MYDHT11__
#define __MYDHT11__
#include "stm32f4xx.h"                  // Device header


typedef struct {
    uint8_t humi_int;
    uint8_t humi_dec;
    uint8_t temp_int;
    uint8_t temp_dec;
} DHT11_Data_t;

void    DHT11_Init(void);
uint8_t DHT11_Read(DHT11_Data_t *out);

#endif

