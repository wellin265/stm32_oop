/**
 * @file    ws2812.h
 * @brief   WS2812/WS2812B RGB LED 灯带驱动 — 面向对象封装 (PWM + DMA)
 * @note    使用定时器 PWM + DMA 模式生成 WS2812 单线通信时序，无需 bit-bang。
 *          定时器和 DMA 由 CubeMX 完成初始化，本驱动仅依赖已配置好的
 *          TIM_HandleTypeDef / DMA_HandleTypeDef 句柄指针。
 *
 *          数据流:
 *          用户设置颜色 → set_led() / fill() 写入颜色缓冲区 (GRB)
 *          send() → 将颜色缓冲转换为 PWM 占空比位流 → DMA 发送到定时器 CCR
 *          定时器 PWM 输出 → WS2812 灯带锁存数据 (RESET >= 50us 低电平)
 *
 *          时序自动计算: T1H = 2/3 * (ARR+1), T0H = 1/3 * (ARR+1)
 *          适用于定时器 Period=89 (90 个时钟周期 / bit) 的典型配置:
 *            - TIM Clock = 72MHz, PSC=0, ARR=89 → 800kHz → T_bit = 1.25us
 *            - T1H ≈ 0.83us, T0H ≈ 0.42us
 *
 * @example 使用示例:
 *   #define LED_COUNT 25
 *   static uint8_t  color_buf[LED_COUNT * 3];
 *   static uint16_t pwm_buf[24 * LED_COUNT + 80];
 *
 *   WS2812_S ws;
 *   WS2812_CFG_S cfg = {
 *       .htim          = &htim2,
 *       .timer_channel = TIM_CHANNEL_1,
 *       .led_count     = LED_COUNT,
 *       .color_buf     = color_buf,
 *       .pwm_buf       = pwm_buf,
 *       .pwm_buf_size  = sizeof(pwm_buf) / sizeof(uint16_t),
 *   };
 *   WS2812_Create(&ws, &cfg);
 *
 *   ws.fill(&ws, 0, 0, 0);
 *   ws.set_led(&ws, 0, 255, 0, 0);
 *   ws.send(&ws);
 *   while (ws.busy(&ws));
 */

#ifndef _WS2812_H_
#define _WS2812_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* ======================== 常量定义 ======================== */

/** PWM 位时基分母 (T1H 占 2/3, T0H 占 1/3) */
#define WS2812_BIT_DIVISOR           3

/** PWM 位时基分子: 逻辑 1 高电平 */
#define WS2812_T1H_NUMERATOR         2

/** PWM 位时基分子: 逻辑 0 高电平 */
#define WS2812_T0H_NUMERATOR         1

/** 复位脉冲数量 (低电平 >= 50us 用于锁存)
 *  典型值: 800kHz PWM 频率下, 80 脉冲 = 100us >= 50us
 *  如需适配不同定时器频率请调整此值
 */
#define WS2812_RESET_PULSES          80

/** 默认全局亮度 (0=全灭, 255=全亮) */
#define WS2812_DEFAULT_BRIGHTNESS    255

/* ======================== 类型定义 ======================== */

/** WS2812 硬件配置 */
typedef struct
{
    TIM_HandleTypeDef *htim;                /**< 定时器句柄 (CubeMX 已配置为 PWM+DMA) */
    uint32_t           timer_channel;       /**< 定时器通道 (TIM_CHANNEL_1/2/3/4) */
    uint16_t           led_count;           /**< LED 灯珠数量 */
    uint8_t           *color_buf;           /**< 颜色缓冲区 (用户分配, led_count*3 字节, GRB 顺序) */
    uint16_t          *pwm_buf;             /**< PWM DMA 缓冲区 (用户分配, uint16_t 数组) */
    uint16_t           pwm_buf_size;        /**< PWM 缓冲区大小 (uint16_t 字数, >= 24*led_count+reset) */
} WS2812_CFG_S;

/* 前向声明 */
typedef struct WS2812_S WS2812_S;

/** 函数指针类型定义 */
typedef void     (*WS2812_INIT_FUNC)(WS2812_S *obj);
typedef void     (*WS2812_SET_LED_FUNC)(WS2812_S *obj, uint16_t index,
                                        uint8_t red, uint8_t green, uint8_t blue);
typedef void     (*WS2812_SET_LED_HEX_FUNC)(WS2812_S *obj, uint16_t index,
                                            uint32_t color);
typedef void     (*WS2812_FILL_FUNC)(WS2812_S *obj, uint8_t red,
                                     uint8_t green, uint8_t blue);
typedef void     (*WS2812_CLEAR_FUNC)(WS2812_S *obj);
typedef void     (*WS2812_SET_BRIGHTNESS_FUNC)(WS2812_S *obj, uint8_t brightness);
typedef uint8_t  (*WS2812_SEND_FUNC)(WS2812_S *obj);
typedef uint8_t  (*WS2812_BUSY_FUNC)(WS2812_S *obj);
typedef void     (*WS2812_STOP_FUNC)(WS2812_S *obj);
typedef void     (*WS2812_WAIT_DONE_FUNC)(WS2812_S *obj);

/** WS2812 驱动对象 */
struct WS2812_S
{
    WS2812_CFG_S cfg;                               /**< 硬件配置 */

    /* ---- 内部状态 (用户不应直接访问) ---- */
    uint8_t  brightness;                            /**< 全局亮度 (0-255, 默认 255) */
    uint32_t t1h_pulse;                             /**< 逻辑 1 高电平 CCR 值 (从 ARR 自动计算) */
    uint32_t t0h_pulse;                             /**< 逻辑 0 高电平 CCR 值 (从 ARR 自动计算) */

    /* ---- 成员函数 ---- */
    WS2812_INIT_FUNC            init;               /**< 初始化硬件和内部状态 */
    WS2812_SET_LED_FUNC         set_led;            /**< 设置单个 LED 颜色 (RGB 输入) */
    WS2812_SET_LED_HEX_FUNC     set_led_hex;        /**< 设置单个 LED 颜色 (0xRRGGBB 输入) */
    WS2812_FILL_FUNC            fill;               /**< 填充所有 LED 为同一颜色 */
    WS2812_CLEAR_FUNC           clear;              /**< 熄灭所有 LED */
    WS2812_SET_BRIGHTNESS_FUNC  set_brightness;     /**< 设置全局亮度 (0-255) */
    WS2812_SEND_FUNC            send;               /**< 生成位流并启动 DMA 传输 */
    WS2812_BUSY_FUNC            busy;               /**< 查询 DMA 是否忙 (1=忙, 0=空闲) */
    WS2812_STOP_FUNC            stop;               /**< 停止 PWM DMA 输出 */
    WS2812_WAIT_DONE_FUNC       wait_done;          /**< 阻塞等待 DMA 发送完成并停止 */
};

/* ======================== 构造函数 ======================== */
void WS2812_Create(WS2812_S *obj, WS2812_CFG_S *config);

#ifdef __cplusplus
}
#endif

#endif /* _WS2812_H_ */
