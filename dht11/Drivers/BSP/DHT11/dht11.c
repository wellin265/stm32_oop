#include "dht11.h"
#include "delay.h"
#include <string.h>

/*
 * DHT11 开漏输出驱动
 *
 * 引脚始终配置为 GPIO_MODE_OUTPUT_OD + GPIO_PULLUP：
 *   - 写 PIN_RESET → N-MOS 导通，拉低总线
 *   - 写 PIN_SET   → N-MOS 截止，总线由外部上拉电阻拉高（DHT11 也可拉低）
 *   - HAL_GPIO_ReadPin() 在 OUTPUT_OD 模式下读取的是 IDR，反映实际电平
 *
 * 这样不需要在输入/输出模式之间来回切换，和 DS18B20 的单总线用法一致。
 */

#define DHT11_TIMEOUT   1000

static uint8_t DHT11_ReadByte(DHT11_S* h)
{
	uint8_t i, data = 0;
	uint16_t timeout;

	for (i = 0; i < 8; i++) {
		data <<= 1;

		/* 等待 DHT11 拉低 50us 结束 → 上升沿 */
		timeout = 0;
		while (HAL_GPIO_ReadPin(h->config.port, h->config.pin) == GPIO_PIN_RESET) {
			if (++timeout > DHT11_TIMEOUT) return 0;
		}

		/* 上升沿后等 30us，此时:
		   bit 0 (26us HIGH) 已结束 → 读到 LOW
		   bit 1 (70us HIGH) 还在 → 读到 HIGH */
		delay_us(30);
		if (HAL_GPIO_ReadPin(h->config.port, h->config.pin) == GPIO_PIN_SET) data |= 1;

		/* 等待当前 bit 的高电平结束（bit 0 已结束则立即通过） */
		timeout = 0;
		while (HAL_GPIO_ReadPin(h->config.port, h->config.pin) == GPIO_PIN_SET) {
			if (++timeout > DHT11_TIMEOUT) return 0;
		}
	}
	return data;
}

static void DHT11_Init(DHT11_S* h)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = h->config.pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(h->config.port, &GPIO_InitStruct);

	/* 释放总线（高阻态，外部上拉拉高） */
	HAL_GPIO_WritePin(h->config.port, h->config.pin, GPIO_PIN_SET);

	h->temperature = 0;
	h->humidity = 0;
	h->status = 0;
}

static uint8_t DHT11_Read(DHT11_S* h)
{
	uint8_t buf[5] = {0};
	uint16_t timeout;

	/* ---- 发送起始信号 ---- */
	/* 拉低 >18ms */
	HAL_GPIO_WritePin(h->config.port, h->config.pin, GPIO_PIN_RESET);
	delay_ms(20);
	/* 释放总线 20-40us */
	HAL_GPIO_WritePin(h->config.port, h->config.pin, GPIO_PIN_SET);
	delay_us(30);

	/* ---- 等待 DHT11 响应 ---- */
	/* DHT11 检测到起始信号后，会在 20-40us 内拉低 */
	timeout = 0;
	while (HAL_GPIO_ReadPin(h->config.port, h->config.pin) == GPIO_PIN_SET) {
		if (++timeout > DHT11_TIMEOUT) { h->status = 0; return 0; }
	}

	/* DHT11 拉低 ~80us → 等待结束 */
	timeout = 0;
	while (HAL_GPIO_ReadPin(h->config.port, h->config.pin) == GPIO_PIN_RESET) {
		if (++timeout > DHT11_TIMEOUT) { h->status = 0; return 0; }
	}

	/* DHT11 拉高 ~80us → 等待结束 */
	timeout = 0;
	while (HAL_GPIO_ReadPin(h->config.port, h->config.pin) == GPIO_PIN_SET) {
		if (++timeout > DHT11_TIMEOUT) { h->status = 0; return 0; }
	}

	/* ---- 读取 40bit 数据 ---- */
	buf[0] = DHT11_ReadByte(h);
	buf[1] = DHT11_ReadByte(h);
	buf[2] = DHT11_ReadByte(h);
	buf[3] = DHT11_ReadByte(h);
	buf[4] = DHT11_ReadByte(h);

	/* 校验 */
	if (buf[4] == (uint8_t)(buf[0] + buf[1] + buf[2] + buf[3])) {
		h->humidity = buf[0];
		h->temperature = buf[2];
		h->status = 1;
		return 1;
	}

	h->status = 0;
	return 0;
}

static uint8_t DHT11_GetTemp(DHT11_S* h) { return h->temperature; }
static uint8_t DHT11_GetHumidity(DHT11_S* h) { return h->humidity; }

void dht11_create(DHT11_S* handle, DHT11_CONFIG_T* config)
{
	memset(handle, 0, sizeof(DHT11_S));
	handle->config.port = config->port;
	handle->config.pin = config->pin;

	handle->init = DHT11_Init;
	handle->read = DHT11_Read;
	handle->get_temp = DHT11_GetTemp;
	handle->get_humidity = DHT11_GetHumidity;

	handle->init(handle);
}
