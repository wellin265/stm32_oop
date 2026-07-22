/**
 * @file    ws2812.c
 * @brief   WS2812/WS2812B RGB LED 灯带驱动实现 (PWM + DMA)
 * @note    面向对象风格: 所有函数通过 WS2812_S 对象调用。
 *          零动态内存分配: 颜色缓冲和 PWM DMA 缓冲均由用户提供。
 *          时序自动计算: T1H/T0H 占空比从定时器 ARR 寄存器动态计算，
 *          无需硬编码，适配任意定时器频率配置。
 */

#include "ws2812.h"
#include <string.h>

/* ======================== 内部辅助 ======================== */

/**
 * @brief  将 TIM_CHANNEL_x 映射为 TIM_DMA_ID_CCx
 * @param  channel: TIM_CHANNEL_1/2/3/4
 * @return 对应的 DMA ID 索引 (0=错误)
 * @note   用于访问 htim->hdma[TIM_DMA_ID_CCx]
 */
static inline uint32_t channel_to_dma_id(uint32_t channel)
{
    switch (channel)
    {
    case TIM_CHANNEL_1: return TIM_DMA_ID_CC1;
    case TIM_CHANNEL_2: return TIM_DMA_ID_CC2;
    case TIM_CHANNEL_3: return TIM_DMA_ID_CC3;
    case TIM_CHANNEL_4: return TIM_DMA_ID_CC4;
    default:            return 0;
    }
}

/**
 * @brief  根据定时器 ARR 计算 T1H 和 T0H 占空比值
 * @param  obj: WS2812 对象指针
 * @note   T1H = (ARR + 1) * 2 / 3  (逻辑 1 高电平)
 *         T0H = (ARR + 1) * 1 / 3  (逻辑 0 高电平)
 *         WS2812 协议要求: T0H=0.4us, T0L=0.85us, T1H=0.8us, T1L=0.45us
 *         2/3 : 1/3 比例在 800kHz 时基下满足上述时序要求
 */
static void ws2812_calc_timing(WS2812_S *obj)
{
    uint32_t period = obj->cfg.htim->Init.Period;  /* ARR 值 */
    uint32_t total  = period + 1U;

    obj->t0h_pulse = (total * WS2812_T0H_NUMERATOR) / WS2812_BIT_DIVISOR;
    obj->t1h_pulse = (total * WS2812_T1H_NUMERATOR) / WS2812_BIT_DIVISOR;
}

/* ======================== 核心功能 ======================== */

/**
 * @brief  初始化 WS2812 硬件和内部状态
 * @param  obj: WS2812 对象指针
 * @note   计算 PWM 时序参数，清空颜色缓冲区，设置默认亮度。
 *          硬件定时器和 DMA 由 CubeMX 完成初始化，本函数不重复配置。
 */
static void ws2812_init(WS2812_S *obj)
{
    /* 从定时器 ARR 自动计算 T1H / T0H 占空比 */
    ws2812_calc_timing(obj);

    /* 初始化状态 */
    obj->brightness = WS2812_DEFAULT_BRIGHTNESS;

    /* 清空颜色缓冲区 (全部熄灭) */
    memset(obj->cfg.color_buf, 0, obj->cfg.led_count * 3U);
}

/**
 * @brief  设置单个 LED 颜色 (RGB 格式输入)
 * @param  obj:   WS2812 对象指针
 * @param  index: LED 索引 (0 ~ led_count-1)
 * @param  red:   红色分量 (0-255)
 * @param  green: 绿色分量 (0-255)
 * @param  blue:  蓝色分量 (0-255)
 * @note   内部存储为 WS2812 原生 GRB 顺序，RGB→GRB 转换在此完成。
 *         索引越界时无操作。
 */
static void ws2812_set_led(WS2812_S *obj, uint16_t index,
                           uint8_t red, uint8_t green, uint8_t blue)
{
    if (index >= obj->cfg.led_count)
        return;

    uint8_t *p = &obj->cfg.color_buf[index * 3U];
    p[0] = green;  /* G */
    p[1] = red;    /* R */
    p[2] = blue;   /* B */
}

/**
 * @brief  设置单个 LED 颜色 (十六进制 0xRRGGBB 格式输入)
 * @param  obj:   WS2812 对象指针
 * @param  index: LED 索引 (0 ~ led_count-1)
 * @param  color: RGB 颜色值 (0xRRGGBB, 如 0xFF0000 = 红色)
 * @note   提取 R/G/B 后调用 set_led()，自动转换为 GRB 存储。
 *         索引越界时无操作。
 */
static void ws2812_set_led_hex(WS2812_S *obj, uint16_t index, uint32_t color)
{
    if (index >= obj->cfg.led_count)
        return;

    uint8_t red   = (uint8_t)((color >> 16U) & 0xFFU);
    uint8_t green = (uint8_t)((color >> 8U)  & 0xFFU);
    uint8_t blue  = (uint8_t)( color        & 0xFFU);

    ws2812_set_led(obj, index, red, green, blue);
}

/**
 * @brief  填充所有 LED 为同一颜色 (RGB 格式输入)
 * @param  obj:   WS2812 对象指针
 * @param  red:   红色分量 (0-255)
 * @param  green: 绿色分量 (0-255)
 * @param  blue:  蓝色分量 (0-255)
 */
static void ws2812_fill(WS2812_S *obj, uint8_t red, uint8_t green, uint8_t blue)
{
    uint8_t *p = obj->cfg.color_buf;
    for (uint16_t i = 0; i < obj->cfg.led_count; i++)
    {
        p[0] = green;
        p[1] = red;
        p[2] = blue;
        p += 3;
    }
}

/**
 * @brief  熄灭所有 LED
 * @param  obj: WS2812 对象指针
 */
static void ws2812_clear(WS2812_S *obj)
{
    memset(obj->cfg.color_buf, 0, obj->cfg.led_count * 3U);
}

/**
 * @brief  设置全局亮度
 * @param  obj:        WS2812 对象指针
 * @param  brightness: 亮度值 (0 = 全灭, 255 = 全亮)
 * @note   亮度在 send() 生成位流时应用，不会修改原始颜色缓冲区。
 *         缩放公式: 输出 = 原始值 * brightness / 255 (整数运算)
 */
static void ws2812_set_brightness(WS2812_S *obj, uint8_t brightness)
{
    obj->brightness = brightness;
}

/* ======================== 发送操作 ======================== */

/**
 * @brief  生成 PWM 位流并启动 DMA 发送
 * @param  obj: WS2812 对象指针
 * @return 0 = DMA 忙 (发送失败), 1 = 启动成功
 * @note   将颜色缓冲区中每个 LED 的 24 位 GRB 数据展开为 PWM 占空比序列:
 *           逻辑 1 → T1H 高电平 (2/3 周期)
 *           逻辑 0 → T0H 高电平 (1/3 周期)
 *         末尾追加 WS2812_RESET_PULSES 个零脉冲 (>50us 低电平锁存信号)。
 *         亮度缩放在此阶段应用。
 *
 * @example 发送完成后等待:
 *   if (ws.send(&ws)) {
 *       ws.wait_done(&ws);  // 阻塞等待完成
 *   }
 */
static uint8_t ws2812_send(WS2812_S *obj)
{
    uint32_t dma_id = channel_to_dma_id(obj->cfg.timer_channel);
    if (dma_id == 0U)
        return 0;

    /* 检查 DMA 是否空闲 */
    if (HAL_DMA_GetState(obj->cfg.htim->hdma[dma_id]) != HAL_DMA_STATE_READY)
        return 0;

    uint16_t  *pwm    = obj->cfg.pwm_buf;
    uint8_t   *color  = obj->cfg.color_buf;
    uint16_t   count  = obj->cfg.led_count;
    uint32_t   t1h    = obj->t1h_pulse;
    uint32_t   t0h    = obj->t0h_pulse;
    uint8_t    bright = obj->brightness;

    uint32_t idx = 0;

    for (uint16_t led = 0; led < count; led++)
    {
        uint8_t g = color[0];
        uint8_t r = color[1];
        uint8_t b = color[2];
        color += 3;

        /* 应用亮度缩放 (无浮点运算) */
        if (bright != 255U)
        {
            g = (uint8_t)(((uint16_t)g * bright) / 255U);
            r = (uint8_t)(((uint16_t)r * bright) / 255U);
            b = (uint8_t)(((uint16_t)b * bright) / 255U);
        }

        /* 将 24 位 GRB 数据展开为 PWM 脉冲序列 (MSB 优先) */
        uint32_t grb = ((uint32_t)g << 16U) | ((uint32_t)r << 8U) | b;

        for (int8_t bit = 23; bit >= 0; bit--)
        {
            if (grb & (1UL << (uint32_t)bit))
                pwm[idx] = (uint16_t)t1h;
            else
                pwm[idx] = (uint16_t)t0h;

            idx++;
        }
    }

    /* 复位脉冲: 全部填零 (低电平 >= 50us 锁存) */
    for (uint16_t i = 0; i < WS2812_RESET_PULSES; i++)
    {
        pwm[idx] = 0;
        idx++;
    }

    /* 启动 PWM DMA 传输
     * 注: uint16_t* → uint32_t* 的强制转换是 HAL API 历史遗留问题，
     * 实际传输位宽由 DMA 初始化时配置的 PDATAALIGN 决定 (HALFWORD = 16-bit)
     */
    HAL_TIM_PWM_Start_DMA(obj->cfg.htim, obj->cfg.timer_channel,
                          (uint32_t *)pwm, idx);

    return 1;
}

/**
 * @brief  查询 DMA 是否正在发送
 * @param  obj: WS2812 对象指针
 * @return 1 = DMA 忙 (不可发送新数据), 0 = DMA 空闲
 */
static uint8_t ws2812_busy(WS2812_S *obj)
{
    uint32_t dma_id = channel_to_dma_id(obj->cfg.timer_channel);
    if (dma_id == 0U)
        return 0;

    return (HAL_DMA_GetState(obj->cfg.htim->hdma[dma_id]) != HAL_DMA_STATE_READY);
}

/**
 * @brief  停止 PWM DMA 输出
 * @param  obj: WS2812 对象指针
 * @note   调用后定时器 PWM 停止，输出引脚恢复默认电平。
 *         可在 send() 后的任意时刻调用以节省功耗。
 */
static void ws2812_stop(WS2812_S *obj)
{
    HAL_TIM_PWM_Stop_DMA(obj->cfg.htim, obj->cfg.timer_channel);
}

/**
 * @brief  阻塞等待 DMA 发送完成并自动停止 PWM
 * @param  obj: WS2812 对象指针
 * @note   内部轮询 busy() → stop()，调用后灯带数据已锁存。
 *         阻塞时间取决于 LED 数量和定时器频率:
 *           25 LED × 24 bit × 1.25us + 100us (reset) ≈ 0.85ms
 */
static void ws2812_wait_done(WS2812_S *obj)
{
    while (ws2812_busy(obj))
    {
        /* 等待 DMA 传输完成 */
    }

    /* 传输完成后停止 PWM 输出以节省功耗 */
    ws2812_stop(obj);
}

/* ======================== 构造函数 ======================== */

/**
 * @brief  WS2812 驱动对象构造函数
 * @param  obj:    对象指针 (由调用者分配内存)
 * @param  config: 硬件配置指针 (由调用者分配内存)
 * @note   调用后对象即初始化完毕，所有成员函数可直接使用。
 *          配置中的缓冲区必须在对象生命周期内保持有效。
 *          参数校验失败时不会初始化函数指针，用户应检查返回值或
 *          确保参数合法。
 *
 * @example 完整使用示例:
 *   #define LED_NUM 25
 *   static uint8_t  led_color[LED_NUM * 3];
 *   static uint16_t led_pwm[24 * LED_NUM + 80];
 *
 *   WS2812_S ws;
 *   WS2812_CFG_S cfg = {
 *       .htim          = &htim2,
 *       .timer_channel = TIM_CHANNEL_1,
 *       .led_count     = LED_NUM,
 *       .color_buf     = led_color,
 *       .pwm_buf       = led_pwm,
 *       .pwm_buf_size  = sizeof(led_pwm) / sizeof(uint16_t),
 *   };
 *   WS2812_Create(&ws, &cfg);
 *
 *   // 设置颜色
 *   ws.fill(&ws, 0, 0, 0);                    // 全部熄灭
 *   ws.set_led(&ws, 0, 255, 0, 0);            // LED0 红色
 *   ws.set_led_hex(&ws, 1, 0x00FF00);         // LED1 绿色
 *
 *   // 发送数据
 *   ws.send(&ws);
 *   ws.wait_done(&ws);
 *
 *   // 亮度控制
 *   ws.set_brightness(&ws, 128);              // 50% 亮度
 *   ws.send(&ws);
 *   ws.wait_done(&ws);
 */
void WS2812_Create(WS2812_S *obj, WS2812_CFG_S *config)
{
    /* 参数校验 */
    if (config->htim == NULL)
        return;
    if (config->color_buf == NULL || config->pwm_buf == NULL)
        return;
    if (config->led_count == 0U)
        return;

    /* 检查 PWM 缓冲区大小是否足够容纳位流 */
    uint16_t min_size = config->led_count * 24U + WS2812_RESET_PULSES;
    if (config->pwm_buf_size < min_size)
        return;

    /* 绑定配置 (值拷贝) */
    obj->cfg = *config;

    /* 绑定函数指针 */
    obj->init           = ws2812_init;
    obj->set_led        = ws2812_set_led;
    obj->set_led_hex    = ws2812_set_led_hex;
    obj->fill           = ws2812_fill;
    obj->clear          = ws2812_clear;
    obj->set_brightness = ws2812_set_brightness;
    obj->send           = ws2812_send;
    obj->busy           = ws2812_busy;
    obj->stop           = ws2812_stop;
    obj->wait_done      = ws2812_wait_done;

    /* 执行硬件初始化 */
    obj->init(obj);
}
