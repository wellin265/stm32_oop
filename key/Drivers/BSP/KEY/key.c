/**
 * @file    key.c
 * @brief   按键驱动实现 — 状态机 + 消抖 + FIFO 事件缓冲
 * @note    面向对象风格: 所有函数通过 KEY_S 对象调用
 *          scan() 需在 10ms 定时中断中调用以驱动状态机
 */

#include "key.h"

/* ======================== 内部辅助 ======================== */

/** FIFO 入队 (内部, 不加锁, 仅在 scan 中调用) */
static uint8_t fifo_push(KEY_S *key, KEY_EVENT_T event)
{
    if (key->fifo.count >= KEY_FIFO_SIZE)
        return 0;

    key->fifo.buf[key->fifo.tail] = event;
    key->fifo.tail = (key->fifo.tail + 1) % KEY_FIFO_SIZE;
    key->fifo.count++;
    return 1;
}

/** FIFO 出队 */
static uint8_t fifo_pop(KEY_S *key, KEY_EVENT_T *event)
{
    if (key->fifo.count == 0)
        return 0;

    *event = key->fifo.buf[key->fifo.head];
    key->fifo.head = (key->fifo.head + 1) % KEY_FIFO_SIZE;
    key->fifo.count--;
    return 1;
}

/** 读取当前引脚是否为按下状态 */
static uint8_t is_pressed(KEY_S *key)
{
    GPIO_PinState raw = HAL_GPIO_ReadPin(key->config.port, key->config.pin);
    if (key->config.active_level == KEY_ACTIVE_HIGH)
        return (raw == GPIO_PIN_SET);
    else
        return (raw == GPIO_PIN_RESET);
}

/** 检测释放条件 */
static uint8_t check_release(KEY_S *key)
{
    uint8_t threshold = key->config.filter_time_ms / 10;
    if (threshold == 0) threshold = 1;

    if (key->filter_count < threshold)
        key->filter_count++;

    if (key->filter_count >= threshold)
    {
        key->state = KEY_STATE_RELEASED;
        key->filter_count = 0;
        fifo_push(key, KEY_EVENT_UP);
        return 1;
    }
    return 0;
}

/* ======================== 核心功能 ======================== */

/**
 * @brief  初始化 GPIO + 重置状态
 */
static void key_init(KEY_S *key)
{
    /* 使能 GPIO 时钟 (HAL_GPIO_Init 内部不会自动开时钟) */
    if (key->config.port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (key->config.port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (key->config.port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
    else if (key->config.port == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
    else if (key->config.port == GPIOE) __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pin   = key->config.pin;
    gpio.Pull  = (key->config.active_level == KEY_ACTIVE_LOW)
                 ? GPIO_PULLUP : GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(key->config.port, &gpio);

    key->reset(key);
}

/**
 * @brief  清空状态和 FIFO
 */
static void key_reset(KEY_S *key)
{
    key->state        = KEY_STATE_RELEASED;
    key->filter_count = 0;
    key->press_ticks  = 0;
    key->repeat_count = 0;

    key->fifo.head  = 0;
    key->fifo.tail  = 0;
    key->fifo.count = 0;
}

/**
 * @brief  扫描按键状态机 (每 10ms 调用一次)
 * @note   必须在定时中断中周期性调用, 周期 = 10ms
 */
static void key_scan(KEY_S *key)
{
    uint8_t pressed = is_pressed(key);
    uint8_t filter_threshold = key->config.filter_time_ms / 10;
    if (filter_threshold == 0) filter_threshold = 1;

    switch (key->state)
    {
    case KEY_STATE_RELEASED:
        if (pressed)
        {
            if (key->filter_count < filter_threshold)
                key->filter_count++;

            if (key->filter_count >= filter_threshold)
            {
                key->state       = KEY_STATE_PRESSED;
                key->press_ticks = 0;
                key->repeat_count = 0;
                fifo_push(key, KEY_EVENT_DOWN);
            }
        }
        else
        {
            key->filter_count = 0;
        }
        break;

    case KEY_STATE_PRESSED:
        if (pressed)
        {
            key->filter_count = 0;
            key->press_ticks++;

            if (key->press_ticks >= key->config.long_press_time_ms / 10)
            {
                key->state = KEY_STATE_LONG_PRESSED;
                fifo_push(key, KEY_EVENT_LONG);
            }
        }
        else
        {
            check_release(key);
        }
        break;

    case KEY_STATE_LONG_PRESSED:
        if (pressed)
        {
            key->press_ticks++;

            /* 连发: 超过启动延时后, 每 repeat_speed_ms 发一次 */
            if (key->press_ticks >= key->config.repeat_delay_ms / 10)
            {
                uint16_t speed_ticks = key->config.repeat_speed_ms / 10;
                if (speed_ticks == 0) speed_ticks = 1;

                key->repeat_count++;
                if (key->repeat_count >= speed_ticks)
                {
                    key->repeat_count = 0;
                    key->press_ticks = key->config.repeat_delay_ms / 10;
                    fifo_push(key, KEY_EVENT_REPEAT);
                }
            }
        }
        else
        {
            check_release(key);
        }
        break;

    default:
        break;
    }
}

/* ======================== 事件获取 ======================== */

static uint8_t key_get_id(KEY_S *key)         { return key->config.id; }
static KEY_STATE_T key_get_state(KEY_S *key)   { return key->state; }
static uint8_t key_event_count(KEY_S *key)     { return key->fifo.count; }

/**
 * @brief  获取按键事件 (非阻塞)
 * @return KEY_EVENT_NONE 表示无事件
 */
static KEY_EVENT_T key_get_event(KEY_S *key)
{
    KEY_EVENT_T event = KEY_EVENT_NONE;
    __disable_irq();
    fifo_pop(key, &event);
    __enable_irq();
    return event;
}

/**
 * @brief  等待指定事件 (阻塞, 超时返回 NONE)
 * @param  want:       期望的事件, KEY_EVENT_NONE 表示匹配任意事件
 * @param  timeout_ms: 超时时间 (ms)
 */
static KEY_EVENT_T key_get_event_timeout(KEY_S *key, KEY_EVENT_T want,
                                          uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while (1)
    {
        KEY_EVENT_T e = key_get_event(key);

        if (e != KEY_EVENT_NONE)
        {
            if (want == KEY_EVENT_NONE || e == want)
                return e;
        }

        if (HAL_GetTick() - start >= timeout_ms)
            return KEY_EVENT_NONE;
    }
}

/* ======================== 构造函数 ======================== */

/**
 * @brief  按键对象构造函数
 * @param  key:    按键对象指针
 * @param  config: 硬件配置
 *
 * @example 使用示例:
 *   KEY_S key;
 *   KEY_CFG_S cfg = {
 *       .id = 0, .port = KEY0_GPIO_Port, .pin = KEY0_Pin,
 *       .active_level = KEY_ACTIVE_LOW
 *   };
 *   KEY_Create(&key, &cfg);
 *
 *   // TIM4 ISR (10ms):  key.scan(&key);
 *   // main loop:         KEY_EVENT_T e = key.get_event(&key);
 */
void KEY_Create(KEY_S *key, KEY_CFG_S *config)
{
    /* 绑定配置 */
    key->config = *config;

    /* 设置默认值 */
    if (key->config.filter_time_ms == 0)
        key->config.filter_time_ms = KEY_DEFAULT_FILTER_MS;
    if (key->config.long_press_time_ms == 0)
        key->config.long_press_time_ms = KEY_DEFAULT_LONG_PRESS_MS;
    if (key->config.repeat_delay_ms == 0)
        key->config.repeat_delay_ms = KEY_DEFAULT_REPEAT_DELAY_MS;
    if (key->config.repeat_speed_ms == 0)
        key->config.repeat_speed_ms = KEY_DEFAULT_REPEAT_SPEED_MS;

    /* 绑定函数指针 */
    key->init             = key_init;
    key->reset            = key_reset;
    key->scan             = key_scan;
    key->get_id           = key_get_id;
    key->get_event        = key_get_event;
    key->get_event_timeout = key_get_event_timeout;
    key->get_state        = key_get_state;
    key->event_count      = key_event_count;

    /* 硬件初始化 */
    key->init(key);
}
