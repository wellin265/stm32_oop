# WS2812 驱动

WS2812/WS2812B RGB LED 灯带驱动，使用定时器 PWM + DMA 生成单线通信时序，面向对象 C 封装。

## 特性

- **PWM + DMA 硬件生成时序** — 无需 bit-bang，CPU 零干预
- **自动时序计算** — 从定时器 ARR 寄存器动态计算 T1H/T0H 占空比，适配任意定时器频率
- **零动态内存分配** — 颜色缓冲和 PWM DMA 缓冲均由用户提供静态内存
- **运行时可调亮度** — 0-255 线性缩放，在 DMA 发送时应用，不修改原始颜色数据
- **面向对象 API** — 与 USART_RING/LED/KEY 驱动风格统一

## 快速开始

```c
#include "ws2812.h"

/* 1. 分配静态缓冲区 */
#define LED_COUNT 25
static uint8_t  color_buf[LED_COUNT * 3];               /* 每 LED 3 字节 (GRB) */
static uint16_t pwm_buf[24 * LED_COUNT + 80];           /* 位流 + 复位脉冲 */

WS2812_S ws;

int main(void)
{
    /* 2. 系统初始化 (CubeMX 生成) */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_TIM2_Init();  /* TIM2 CH1 已配置 PWM+DMA */

    /* 3. 创建 WS2812 对象 */
    WS2812_CFG_S cfg = {
        .htim          = &htim2,
        .timer_channel = TIM_CHANNEL_1,
        .led_count     = LED_COUNT,
        .color_buf     = color_buf,
        .pwm_buf       = pwm_buf,
        .pwm_buf_size  = sizeof(pwm_buf) / sizeof(uint16_t),
    };
    WS2812_Create(&ws, &cfg);

    /* 4. 设置颜色并发送 */
    ws.fill(&ws, 0, 0, 0);                  /* 全部熄灭 */
    ws.set_led(&ws, 0, 255, 0, 0);          /* LED 0 = 红色 */
    ws.set_led_hex(&ws, 1, 0x00FF00);       /* LED 1 = 绿色 (0xRRGGBB) */
    ws.set_led(&ws, 2, 0, 0, 255);          /* LED 2 = 蓝色 */
    ws.send(&ws);                            /* 启动 DMA 发送 */
    ws.wait_done(&ws);                       /* 等待发送完成 */

    /* 5. 亮度控制 */
    ws.set_brightness(&ws, 128);             /* 50% 亮度 */
    ws.send(&ws);
    ws.wait_done(&ws);

    while (1)
    {
        /* 流水灯效果 */
        for (uint16_t i = 0; i < LED_COUNT; i++)
        {
            ws.clear(&ws);
            ws.set_led(&ws, i, 255, 0, 0);
            ws.send(&ws);
            ws.wait_done(&ws);
            HAL_Delay(50);
        }
    }
}
```

## API 参考

### 构造函数

```c
void WS2812_Create(WS2812_S *obj, WS2812_CFG_S *config);
```

参数校验失败（NULL 指针、缓冲区不足）时函数指针不被绑定，用户应确保参数合法。

### 配置结构体

```c
typedef struct {
    TIM_HandleTypeDef *htim;            /* 定时器句柄 (已配置 PWM+DMA) */
    uint32_t           timer_channel;   /* 定时器通道 (TIM_CHANNEL_1/2/3/4) */
    uint16_t           led_count;       /* LED 灯珠数量 */
    uint8_t           *color_buf;       /* 颜色缓冲区 (用户分配, led_count*3 字节) */
    uint16_t          *pwm_buf;         /* PWM DMA 缓冲区 (用户分配) */
    uint16_t           pwm_buf_size;    /* PWM 缓冲区大小 (uint16_t 字数) */
} WS2812_CFG_S;
```

**缓冲区大小计算：**

| 缓冲区 | 最小大小 | 说明 |
|--------|----------|------|
| `color_buf` | `led_count × 3` 字节 | 每 LED: [G, R, B] |
| `pwm_buf` | `24 × led_count + WS2812_RESET_PULSES` 字 (uint16_t) | 位流 + 锁存复位 |

`WS2812_RESET_PULSES` 默认为 80，对应 ≥50μs 低电平锁存信号。

### 颜色设置

| 成员函数 | 说明 |
|----------|------|
| `ws.set_led(&ws, index, r, g, b)` | 设置单个 LED (RGB 格式，0-255) |
| `ws.set_led_hex(&ws, index, 0xRRGGBB)` | 设置单个 LED (十六进制) |
| `ws.fill(&ws, r, g, b)` | 全部填充同一颜色 |
| `ws.clear(&ws)` | 全部熄灭 |

> **注意：** 索引从 0 开始，越界访问无操作。

### 发送控制

| 成员函数 | 返回值 | 说明 |
|----------|--------|------|
| `ws.send(&ws)` → `uint8_t` | 0=DMA忙, 1=启动成功 | 生成位流并启动 DMA |
| `ws.busy(&ws)` → `uint8_t` | 1=忙, 0=空闲 | 查询 DMA 状态 |
| `ws.stop(&ws)` | — | 停止 PWM DMA 输出 |
| `ws.wait_done(&ws)` | — | 阻塞等待 DMA 完成 + 自动停止 |

### 亮度控制

| 成员函数 | 说明 |
|----------|------|
| `ws.set_brightness(&ws, 0~255)` | 设置全局亮度 (默认 255) |

亮度在 `send()` 生成位流时通过整数运算应用（`输出 = 原始值 × 亮度 / 255`），不修改颜色缓冲区。这意味着修改亮度后只需重新 `send()` 即可生效，无需重新设置颜色。

### 其他

| 成员函数 | 说明 |
|----------|------|
| `ws.init(&ws)` | 重新初始化 (Create 已调用) |

## 时序原理

驱动使用定时器的 **PWM 占空比** 来模拟 WS2812 的单线归零码：

```
逻辑 1:  T1H ≈ 2/3 周期 (高电平), T1L = 1/3 周期 (低电平)
逻辑 0:  T0H ≈ 1/3 周期 (高电平), T0L = 2/3 周期 (低电平)
复位:    >= 50us 低电平 → 数据锁存到灯带
```

```
颜色缓冲区 (GRB)              PWM 位流 (DMA)               WS2812 输出
─────────────────────        ──────────────────────       ────────────
LED0: [G=255, R=0, B=0]  →   T1H,T1H,...,T1H (G)     →   绿=255
                              T0H,T0H,...,T0H (R)     →   红=0
                              T0H,T0H,...,T0H (B)     →   蓝=0
LED1: [G=0, R=255, B=0]  →   T0H,T0H,...,T0H (G)     →   绿=0
                              T1H,T1H,...,T1H (R)     →   红=255
                              T0H,T0H,...,T0H (B)     →   蓝=0
...                          ...                          ...
末尾:                         [0, 0, ..., 0] × 80        →   锁存!
```

DMA 将 PWM 占空比序列逐周期传输到定时器 CCR 寄存器，定时器自动生成对应波形，
全程无需 CPU 参与。传输完成后 PWM 输出保持低电平（最后 CCR=0），灯带自动锁存。

## 典型定时器配置

以下示例均为 WS2812B 兼容配置（1 bit ≈ 1.25μs, PWM 频率 ≈ 800kHz）：

| MCU 系列 | 定时器时钟 | PSC | ARR (Period) | T1H ≈ | T0H ≈ | 说明 |
|----------|-----------|-----|-------------|-------|-------|------|
| STM32F1 | 72 MHz | 0 | 89 | 60 / 800kHz = 0.75μs | 30 / 800kHz = 0.38μs | TIM2 APB1 |
| STM32F4 | 84 MHz | 0 | 104 | 70 / 800kHz = 0.88μs | 35 / 800kHz = 0.44μs | 调 PSC 使频率接近 800kHz |
| STM32F4 | 84 MHz | 1 | 52 | 35 / 793kHz ≈ 0.88μs | 18 / 793kHz ≈ 0.45μs | 更接近理想值 |

> **通用公式：** PWM 频率 = TIM_CLK / ((PSC+1) × (ARR+1)) ≈ 800kHz 即可满足 WS2812 时序要求。

驱动会自动根据 `htim->Init.Period` (ARR) 计算 T1H / T0H，**同一份驱动代码适配以上所有配置，无需修改**。

## 移植

### 切换到其他 STM32 系列 (F4/G0/H7/L4 等)

1. **修改头文件 include：**

   将 `#include "stm32f1xx_hal.h"` 改为对应系列的 HAL 头，例如：
   - STM32F4: `stm32f4xx_hal.h`
   - STM32G0: `stm32g0xx_hal.h`
   - STM32H7: `stm32h7xx_hal.h`

2. **CubeMX 定时器配置：**
   - 选择任意通用定时器（如 TIM1/TIM2/TIM3）
   - 设置 PWM 频率约 800kHz（调整 PSC 和 ARR）
   - **必须启用 DMA：** TIMx → DMA Settings → Add → Channel X (Memory to Peripheral, Half Word)
   - **必须将 DMA 模式设为 Normal**（非 Circular）

3. **CubeMX GPIO 配置：**
   - 对应定时器通道的 GPIO 设为 Alternate Function Push-Pull
   - 无需上拉/下拉（WS2812 DI 内部已有）

4. **PWM 缓冲区大小调整：**

   如 PWM 频率不是 800kHz，按实际频率调整 `WS2812_RESET_PULSES`：
   ```c
   // f_pwm = TIM_CLK / ((PSC+1) * (ARR+1))
   // WS2812_RESET_PULSES >= 50e-6 * f_pwm
   // 例: f_pwm = 1MHz → 至少 50 脉冲, 取 80 安全
   #define WS2812_RESET_PULSES  80
   ```

   或在 `ws2812.h` 中全局修改此宏。

### 移植到非 STM32 平台

1. 替换 HAL 依赖函数：
   - `HAL_TIM_PWM_Start_DMA()` → 平台等效的 PWM DMA 启动
   - `HAL_TIM_PWM_Stop_DMA()` → 平台等效的 PWM DMA 停止
   - `HAL_DMA_GetState()` → 平台等效的 DMA 状态查询

2. 移除 `#include "stm32f1xx_hal.h"`，引入平台 SDK 头文件。

3. 修改 `WS2812_CFG_S` 中的 `TIM_HandleTypeDef *htim` 为对应平台句柄类型。

4. `channel_to_dma_id()` 根据平台 DMA 通道映射重写。

## 文件结构

```
Drivers/BSP/WS2812/
├── ws2812.h      # 接口定义 (类型、函数指针、构造函数)
├── ws2812.c      # 驱动实现 (static 成员函数)
└── README.md     # 本文件
```

## 设计说明

### 为什么颜色缓冲和 PWM 缓冲分离？

- **颜色缓冲** 保存原始 RGB 值，亮度修改后无需重新设置
- **PWM 缓冲** 在 `send()` 时一次性生成，应用亮度后转换为占空比序列
- 用户可以预计算颜色表放入 color_buf，多次 `send()` 只改变亮度

### 为什么不用 DMA 双缓冲（Circular Mode）？

WS2812 灯带数据是单帧发送（非连续流），每次 `send()` 发送完整一帧后灯带自动锁存。
Circular Mode 会导致重复发送，不适合此应用场景。

### CPU 占用

- 颜色设置 (`set_led`/`fill`): O(1) ~ O(n) 简单内存写入
- 位流生成 (`send`): O(24×n) 整数位操作，25 LED 约 600 次循环
- DMA 传输: 零 CPU 占用 (25 LED × 24bit + 80reset = 680 次 DMA，约 0.85ms)
