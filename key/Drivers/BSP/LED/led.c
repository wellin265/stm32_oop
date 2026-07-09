//
// Created by a1807 on 2026/7/1.
//
#include "led.h"

/**
 * @brief  LED硬件初始化，使能GPIO时钟并配置引脚为开漏输出
 * @param  led_s: LED对象指针
 */
void led_init(LED_S *led_s)
{
    /* 使能对应的GPIO时钟 */
    if (led_s->led_cfg.GPIOx == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (led_s->led_cfg.GPIOx == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (led_s->led_cfg.GPIOx == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
    else if (led_s->led_cfg.GPIOx == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
    else if (led_s->led_cfg.GPIOx == GPIOE) __HAL_RCC_GPIOE_CLK_ENABLE();

    /* 配置GPIO为开漏输出 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pin = led_s->led_cfg.GPIO_Pin;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    if (led_s->led_cfg.polarity == LED_ACTIVE_HIGH)
    {
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    }
    else
    {
        GPIO_InitStruct.Pull = GPIO_PULLUP;
    }

    HAL_GPIO_Init(led_s->led_cfg.GPIOx, &GPIO_InitStruct);

    /* 默认关闭LED */
    led_s->off(led_s);
}

/**
 * @brief  打开LED（根据极性自动处理电平）
 * @param  led_s: LED对象指针
 */
void led_on(LED_S *led_s)
{
    if (led_s->led_cfg.polarity == LED_ACTIVE_HIGH)
    {
        HAL_GPIO_WritePin(led_s->led_cfg.GPIOx, led_s->led_cfg.GPIO_Pin, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(led_s->led_cfg.GPIOx, led_s->led_cfg.GPIO_Pin, GPIO_PIN_RESET);
    }
    led_s->state = LED_ON;
}

/**
 * @brief  关闭LED（根据极性自动处理电平）
 * @param  led_s: LED对象指针
 */
void led_off(LED_S *led_s)
{
    if (led_s->led_cfg.polarity == LED_ACTIVE_HIGH)
    {
        HAL_GPIO_WritePin(led_s->led_cfg.GPIOx, led_s->led_cfg.GPIO_Pin, GPIO_PIN_RESET);
    }
    else
    {
        HAL_GPIO_WritePin(led_s->led_cfg.GPIOx, led_s->led_cfg.GPIO_Pin, GPIO_PIN_SET);
    }
    led_s->state = LED_OFF;
}

/**
 * @brief  翻转LED状态（复用on/off，保持极性逻辑集中）
 * @param  led_s: LED对象指针
 */
void led_toggle(LED_S *led_s)
{
    if (led_s->state == LED_ON)
    {
        led_s->off(led_s);
    }
    else
    {
        led_s->on(led_s);
    }
}

/**
 * @brief  获取LED当前状态
 * @param  led_s: LED对象指针
 * @return LED_STATE_T: 当前状态（LED_ON / LED_OFF）
 */
LED_STATE_T led_get_state(LED_S *led_s)
{
    return led_s->state;
}

/**
 * @brief  LED对象构造函数
 * @param  led: LED对象指针
 * @param  config: LED配置结构体指针
 * @note   调用后即完成初始化，LED处于关闭状态
 *
 * @example 使用示例:
 *   LED_S led1;
 *   LED_CFG_S cfg = {
 *       .GPIOx = GPIOC,
 *       .GPIO_Pin = GPIO_PIN_13,
 *       .polarity = LED_ACTIVE_LOW
 *   };
 *   LED_Create(&led1, &cfg);
 *   led1.on(&led1);    // 点亮
 *   led1.off(&led1);   // 熄灭
 *   led1.toggle(&led1); // 翻转
 */
void LED_Create(LED_S *led, LED_CFG_S *config)
{
    /* 绑定配置 */
    led->led_cfg = *config;

    /* 绑定函数指针 */
    led->init      = led_init;
    led->on        = led_on;
    led->off       = led_off;
    led->toggle    = led_toggle;
    led->get_state = led_get_state;

    /* 初始状态 */
    led->state = LED_OFF;

    /* 执行硬件初始化 */
    led->init(led);
}
