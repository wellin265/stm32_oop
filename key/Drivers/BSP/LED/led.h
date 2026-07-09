//
// Created by a1807 on 2026/7/1.
//

#ifndef CLION_EXAMPLE_LED_H
#define CLION_EXAMPLE_LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

typedef enum
{
	LED_OFF = 0,
	LED_ON
}LED_STATE_T;

typedef enum
{
	LED_ACTIVE_HIGH = 0,    /* LED的正极连接到IO口，负极连接到GND */
	LED_ACTIVE_LOW          /* LED的负极连接到IO口，正极连接到VCC */
}LED_POLARITY_T;

typedef struct
{
	GPIO_TypeDef *GPIOx;      /* LED连接的GPIO端口 */
	uint16_t GPIO_Pin;          /* LED连接的GPIO引脚 */
	LED_POLARITY_T polarity;    /* LED的极性 */
}LED_CFG_S;

// 前向声明
typedef struct LED_S LED_S;

typedef void (*LED_INIT_FUNC)(LED_S* led);
typedef void (*LED_ON_FUNC)(LED_S* led);
typedef void (*LED_OFF_FUNC)(LED_S* led);
typedef void (*LED_TOGGLE_FUNC)(LED_S* led);
typedef LED_STATE_T (*LED_GETSTATE_FUNC)(LED_S* led);

struct LED_S
{
	LED_CFG_S led_cfg;

	LED_STATE_T state;
	LED_INIT_FUNC init;
	LED_ON_FUNC on;
	LED_OFF_FUNC off;
	LED_TOGGLE_FUNC toggle;
	LED_GETSTATE_FUNC get_state;
};

void LED_Create(LED_S* led, LED_CFG_S* config);

#ifdef __cplusplus
}
#endif

#endif //CLION_EXAMPLE_LED_H