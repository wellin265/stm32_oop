#include "ds18b20.h"
#include "delay.h"
#include <string.h>

#define DS18B20_SKIP_ROM    0xCC
#define DS18B20_CONVERT_T   0x44
#define DS18B20_READ_SCRATCHPAD 0xBE

static void DS18B20_ModeOut(DS18B20_S* h)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = h->config.pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(h->config.port, &GPIO_InitStruct);
}

static void DS18B20_ModeIn(DS18B20_S* h)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = h->config.pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(h->config.port, &GPIO_InitStruct);
}

static void DS18B20_WriteBit(DS18B20_S* h, uint8_t bit)
{
	DS18B20_ModeOut(h);
	HAL_GPIO_WritePin(h->config.port, h->config.pin, GPIO_PIN_RESET);
	if (bit) {
		delay_us(1);
		HAL_GPIO_WritePin(h->config.port, h->config.pin, GPIO_PIN_SET);
		delay_us(60);
	} else {
		delay_us(60);
		HAL_GPIO_WritePin(h->config.port, h->config.pin, GPIO_PIN_SET);
		delay_us(1);
	}
}

static void DS18B20_WriteByte(DS18B20_S* h, uint8_t data)
{
	uint8_t i;
	for (i = 0; i < 8; i++) {
		DS18B20_WriteBit(h, data & 0x01);
		data >>= 1;
	}
}

static uint8_t DS18B20_ReadBit(DS18B20_S* h)
{
	uint8_t bit = 0;
	DS18B20_ModeOut(h);
	HAL_GPIO_WritePin(h->config.port, h->config.pin, GPIO_PIN_RESET);
	delay_us(1);
	DS18B20_ModeIn(h);
	delay_us(5);
	if (HAL_GPIO_ReadPin(h->config.port, h->config.pin) == GPIO_PIN_SET) bit = 1;
	delay_us(60);
	return bit;
}

static uint8_t DS18B20_ReadByte(DS18B20_S* h)
{
	uint8_t i, data = 0;
	for (i = 0; i < 8; i++) {
		data |= (DS18B20_ReadBit(h) << i);
	}
	return data;
}

static uint8_t DS18B20_Reset(DS18B20_S* h)
{
	uint8_t presence;
	DS18B20_ModeOut(h);
	HAL_GPIO_WritePin(h->config.port, h->config.pin, GPIO_PIN_RESET);
	delay_us(480);
	HAL_GPIO_WritePin(h->config.port, h->config.pin, GPIO_PIN_SET);
	delay_us(60);
	DS18B20_ModeIn(h);
	presence = (HAL_GPIO_ReadPin(h->config.port, h->config.pin) == GPIO_PIN_RESET);
	delay_us(420);
	return presence;
}

static void DS18B20_Init(DS18B20_S* h)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = h->config.pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(h->config.port, &GPIO_InitStruct);
	HAL_GPIO_WritePin(h->config.port, h->config.pin, GPIO_PIN_SET);
	h->presence = 0;
	h->temperature = -999.0f;
}

static uint8_t DS18B20_Start(DS18B20_S* h)
{
	h->presence = DS18B20_Reset(h);
	if (!h->presence) return 0;
	DS18B20_WriteByte(h, DS18B20_SKIP_ROM);
	DS18B20_WriteByte(h, DS18B20_CONVERT_T);
	return 1;
}

static float DS18B20_ReadTemp(DS18B20_S* h)
{
	uint8_t tl, th;
	int16_t temp;

	if (!DS18B20_Reset(h)) return -999.0f;
	DS18B20_WriteByte(h, DS18B20_SKIP_ROM);
	DS18B20_WriteByte(h, DS18B20_READ_SCRATCHPAD);
	tl = DS18B20_ReadByte(h);
	th = DS18B20_ReadByte(h);
	DS18B20_Reset(h);

	temp = (int16_t)(th << 8) | tl;
	h->temperature = temp * 0.0625f;
	return h->temperature;
}

void ds18b20_create(DS18B20_S* handle, DS18B20_CONFIG_T* config)
{
	memset(handle, 0, sizeof(DS18B20_S));
	handle->config.port = config->port;
	handle->config.pin = config->pin;

	handle->init = DS18B20_Init;
	handle->start = DS18B20_Start;
	handle->read_temp = DS18B20_ReadTemp;

	handle->init(handle);
}
