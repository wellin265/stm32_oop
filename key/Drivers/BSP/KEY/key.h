/**
 * @file    key.h
 * @brief   按键驱动 — 面向对象封装, 支持按下/弹起/长按/连发 + FIFO 事件缓冲
 * @note    需在 10ms 定时中断中调用 key.scan(&key) 驱动状态机
 *
 * @example 使用示例:
 *   KEY_S key;
 *   KEY_CFG_S cfg = { .id = 0, .port = GPIOB, .pin = GPIO_PIN_12,
 *       .active_level = KEY_ACTIVE_LOW };
 *   KEY_Create(&key, &cfg);
 *
 *   // 定时器中断 (10ms):
 *   key.scan(&key);
 *
 *   // 主循环:
 *   KEY_EVENT_T e = key.get_event(&key);
 *   if (e == KEY_EVENT_DOWN) { ... }
 */

#ifndef _KEY_H_
#define _KEY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* ======================== 常量定义 ======================== */

/** 按键事件 FIFO 深度 */
#define KEY_FIFO_SIZE       10

/** 默认参数 (单位: ms) */
#define KEY_DEFAULT_FILTER_MS       20
#define KEY_DEFAULT_LONG_PRESS_MS   1000
#define KEY_DEFAULT_REPEAT_DELAY_MS 500
#define KEY_DEFAULT_REPEAT_SPEED_MS 100

/* ======================== 类型定义 ======================== */

/** 按键事件类型 */
typedef enum
{
    KEY_EVENT_NONE = 0,     /**< 无事件 */
    KEY_EVENT_DOWN,         /**< 按下 */
    KEY_EVENT_UP,           /**< 弹起 */
    KEY_EVENT_LONG,         /**< 长按 */
    KEY_EVENT_REPEAT,       /**< 连发 */
} KEY_EVENT_T;

/** 按键状态 */
typedef enum
{
    KEY_STATE_RELEASED = 0,     /**< 释放 */
    KEY_STATE_PRESSED,          /**< 按下 */
    KEY_STATE_LONG_PRESSED,     /**< 长按中 */
    KEY_STATE_REPEAT,           /**< 连发中 */
} KEY_STATE_T;

/** 按键激活电平 */
typedef enum
{
    KEY_ACTIVE_HIGH = 0,    /**< 高电平激活 */
    KEY_ACTIVE_LOW,         /**< 低电平激活 (内部上拉, 按下接地) */
} KEY_ACTIVE_LEVEL_T;

/** 按键硬件配置 */
typedef struct
{
    uint8_t id;                     /**< 按键 ID (用于多按键区分) */
    GPIO_TypeDef *port;             /**< GPIO 端口 */
    uint16_t pin;                   /**< GPIO 引脚 */
    KEY_ACTIVE_LEVEL_T active_level; /**< 激活电平 */
    uint16_t filter_time_ms;        /**< 消抖时间 (ms), 默认 50 */
    uint16_t long_press_time_ms;    /**< 长按判定时间 (ms), 默认 1000 */
    uint16_t repeat_delay_ms;       /**< 连发启动延时 (ms), 默认 500 */
    uint16_t repeat_speed_ms;       /**< 连发间隔 (ms), 默认 100 */
} KEY_CFG_S;

/* 前向声明 */
typedef struct KEY_S KEY_S;

/** 函数指针类型定义 */
typedef void        (*KEY_INIT_FUNC)(KEY_S *key);
typedef void        (*KEY_RESET_FUNC)(KEY_S *key);
typedef void        (*KEY_SCAN_FUNC)(KEY_S *key);
typedef uint8_t     (*KEY_GET_ID_FUNC)(KEY_S *key);
typedef KEY_EVENT_T (*KEY_GET_EVENT_FUNC)(KEY_S *key);
typedef KEY_EVENT_T (*KEY_GET_EVENT_TIMEOUT_FUNC)(KEY_S *key, KEY_EVENT_T want, uint32_t timeout_ms);
typedef KEY_STATE_T (*KEY_GET_STATE_FUNC)(KEY_S *key);
typedef uint8_t     (*KEY_EVENT_COUNT_FUNC)(KEY_S *key);

/** 按键对象 */
struct KEY_S
{
    KEY_CFG_S config;                   /**< 硬件配置 */

    /* ---- 内部状态 (用户不应直接访问) ---- */
    KEY_STATE_T state;                  /**< 当前状态机状态 */
    uint8_t     filter_count;           /**< 消抖计数器 */
    uint16_t    press_ticks;            /**< 按下持续 tick 数 (每 tick=10ms) */
    uint16_t    repeat_count;           /**< 连发计数器 */

    /* ---- FIFO 事件缓冲 ---- */
    struct
    {
        KEY_EVENT_T buf[KEY_FIFO_SIZE];
        uint8_t head;
        uint8_t tail;
        uint8_t count;
    } fifo;

    /* ---- 成员函数 ---- */
    KEY_INIT_FUNC               init;           /**< 初始化 GPIO + 重置状态 */
    KEY_RESET_FUNC              reset;          /**< 清空状态和 FIFO */
    KEY_SCAN_FUNC               scan;           /**< 扫描 (每 10ms 调用一次) */
    KEY_GET_ID_FUNC             get_id;         /**< 获取按键 ID */
    KEY_GET_EVENT_FUNC          get_event;      /**< 取事件 (非阻塞, 无事件返回 NONE) */
    KEY_GET_EVENT_TIMEOUT_FUNC  get_event_timeout; /**< 等事件 (阻塞, 超时返回 NONE) */
    KEY_GET_STATE_FUNC          get_state;      /**< 获取当前按键状态 */
    KEY_EVENT_COUNT_FUNC        event_count;    /**< FIFO 中待处理事件数 */
};

/* ======================== 构造函数 ======================== */
void KEY_Create(KEY_S *key, KEY_CFG_S *config);

#ifdef __cplusplus
}
#endif

#endif /* _KEY_H_ */
