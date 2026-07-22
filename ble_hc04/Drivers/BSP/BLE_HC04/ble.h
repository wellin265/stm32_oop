//
// Created by a1807 on 2026/5/20.
//

#ifndef GCC_DEMO_BLE_H
#define GCC_DEMO_BLE_H

#include "stm32f1xx_hal.h"
#include "usart_ring.h"

#define TRUE  1
#define FALSE 0

typedef enum
{
	BLE_ROLE_SLAVE = 1,
	BLE_ROLE_SPPMASTER,
	BLE_ROLE_BLEMASTER,
}BLE_ROLE_MODE_T;

typedef struct BLE_S BLE_S;

struct BLE_S
{
	USART_RING_S* usart_ring;

	uint8_t (*test)(BLE_S* ble, uint16_t timeout_ms);
	uint32_t (*get_baud)(BLE_S* ble, uint16_t timeout_ms);
	uint8_t (*set_baud)(BLE_S* ble, uint32_t baud_rate, uint16_t timeout_ms);
	uint8_t (*get_version)(BLE_S* ble, uint16_t timeout_ms);
	uint8_t (*set_led)(BLE_S* ble, uint8_t on, uint16_t timeout_ms);
	uint8_t (*get_led_status)(BLE_S* ble, uint16_t timeout_ms);
	uint8_t (*set_default)(BLE_S* ble, uint16_t timeout_ms);
	uint8_t (*reset)(BLE_S* ble, uint16_t timeout_ms);
	uint8_t (*set_btmode)(BLE_S* ble, uint8_t on, uint16_t timeout_ms);
	uint8_t (*get_btmode)(BLE_S* ble, uint16_t timeout_ms);
	uint8_t (*set_role)(BLE_S* ble, BLE_ROLE_MODE_T ble_role_mode, uint16_t timeout_ms);
	uint8_t (*get_role)(BLE_S* ble, uint16_t timeout_ms);
	uint8_t (*clear)(BLE_S* ble, uint16_t timeout_ms);
};

uint8_t ble_create(BLE_S* ble, USART_RING_S* usart_ring);

#endif //GCC_DEMO_BLE_H