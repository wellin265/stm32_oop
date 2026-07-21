#ifndef __DHT11_H_
#define __DHT11_H_

#include "stm32f1xx_hal.h"

typedef struct {
	GPIO_TypeDef* port;
	uint16_t pin;
} DHT11_CONFIG_T;

typedef struct DHT11_S DHT11_S;

struct DHT11_S {
	DHT11_CONFIG_T config;
	uint8_t temperature;
	uint8_t humidity;
	uint8_t status;

	void (*init)(DHT11_S* handle);
	uint8_t (*read)(DHT11_S* handle);
	uint8_t (*get_temp)(DHT11_S* handle);
	uint8_t (*get_humidity)(DHT11_S* handle);
};

void dht11_create(DHT11_S* handle, DHT11_CONFIG_T* config);

#endif
