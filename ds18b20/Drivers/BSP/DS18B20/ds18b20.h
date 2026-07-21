#ifndef __DS18B20_H_
#define __DS18B20_H_

#include "stm32f1xx_hal.h"

typedef struct {
	GPIO_TypeDef* port;
	uint16_t pin;
} DS18B20_CONFIG_T;

typedef struct DS18B20_S DS18B20_S;

struct DS18B20_S {
	DS18B20_CONFIG_T config;
	uint8_t presence;
	float temperature;

	void (*init)(DS18B20_S* handle);
	uint8_t (*start)(DS18B20_S* handle);
	float (*read_temp)(DS18B20_S* handle);
};

void ds18b20_create(DS18B20_S* handle, DS18B20_CONFIG_T* config);

#endif
