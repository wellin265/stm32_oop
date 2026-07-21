# SORT_TIMER 软件定时器驱动

面向对象风格的软件定时器管理器，基于硬件定时器 1ms 时基，支持注册多个定时器、单次/自动两种工作模式。

## 架构

```
                    ┌──────────────────────┐
                    │   TIM2 1ms ISR       │
                    │   (硬件定时器)         │
                    └──────────┬───────────┘
                               │ 每 1ms
                               ▼
                    ┌──────────────────────┐
                    │  st.tick(&st)        │  ← ISR 上下文
                    │  遍历所有活跃定时器     │
                    │  count-- → flag=1    │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼───────────┐
                    │  while(1) 主循环       │  ← 主循环上下文
                    │  st.check(&st, id)    │
                    │  读取 flag → 清标志    │
                    │  ONCE 模式自动停用      │
                    └──────────────────────┘
```

## 快速开始

```c
#include "sort_timer.h"

/* 全局软件定时器对象 */
static SORT_TIMER_S g_st;

int main(void)
{
    /* 系统初始化 (由 STM32CubeMX 生成) */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM2_Init();
    MX_USART1_UART_Init();

    /* 创建软件定时器管理器 */
    SORT_TIMER_CFG_S cfg = { .max_timers = 8 };
    SORT_TIMER_Create(&g_st, &cfg);

    /* 注册定时器 */
    int led_timer = g_st.start(&g_st, 500, SORT_TIMER_MODE_AUTO);   // 每 500ms 自动重载
    int timeout   = g_st.start(&g_st, 3000, SORT_TIMER_MODE_ONCE);  // 3 秒后单次触发

    /* 启动硬件 1ms 定时器 */
    HAL_TIM_Base_Start_IT(&htim2);

    while (1) {
        if (g_st.check(&g_st, led_timer)) {
            /* 每 500ms 执行一次 */
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        }
        if (g_st.check(&g_st, timeout)) {
            /* 3 秒后执行一次 */
            printf("3 seconds elapsed!\r\n");
        }
    }
}

/* TIM2 1ms 中断回调 — 驱动软件定时器 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        g_st.tick(&g_st);
    }
}
```

## 工作模式

| 模式 | 枚举值 | 行为 |
|------|--------|------|
| **单次模式** | `SORT_TIMER_MODE_ONCE` | 超时后 `check()` 返回 1，**自动停用**该定时器。适合超时检测、延时执行。 |
| **自动模式** | `SORT_TIMER_MODE_AUTO` | 超时后 `check()` 返回 1，**ISR 自动重载**计数。适合周期性任务 (LED 闪烁、传感器轮询)。 |

```
ONCE 模式时间线:
  start(3000)     tick() × 2999     tick() 第 3000 次       check() 返回 1
  ├──────────────────────────────────┤                        ├─ active=0 (自动停用)
  count=3000                          count=1→0, flag=1

AUTO 模式时间线:
  start(500)      tick() × 499      tick() 第 500 次        check() 返回 1
  ├──────────────────────────────────┤                        ├─ count=500 (自动重载)
  count=500                           count=1→0, flag=1       └─ 下一次 tick: count=499...
```

## API 参考

### 构造函数

```c
void SORT_TIMER_Create(SORT_TIMER_S *self, SORT_TIMER_CFG_S *config);
```

构造软件定时器管理器对象。**由调用者分配内存**（推荐全局变量或 `static`），内部无动态分配。

**配置结构体：**

```c
typedef struct {
    uint8_t max_timers;   // 实际使用的定时器数量 (1 ~ 10)
} SORT_TIMER_CFG_S;
```

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `max_timers` | 4 (当设为 0) | 定时器池大小，上限 `SORT_TIMER_MAX_COUNT` (10)，超限自动截断 |

### 成员函数

| 成员函数 | 调用位置 | 说明 |
|----------|----------|------|
| `st.tick(&st)` | TIM2 1ms ISR 回调 | 驱动所有活跃计数器递减 |
| `st.start(&st, period_ms, mode)` → `int` | main / 初始化 | 注册定时器，成功返回 ID (≥ 0)，失败返回 -1 |
| `st.stop(&st, id)` | main | 停用并释放指定槽位 |
| `st.check(&st, id)` → `uint8_t` | main 循环 | 查询超时，1 = 已超时，0 = 未超时或 ID 无效 |
| `st.restart(&st, id)` | main | 重置计数为预装值，清除 flag |

### start() 返回值

| 返回值 | 含义 |
|--------|------|
| `>= 0` | 成功，返回定时器 ID |
| `-1` | 失败：`period_ms == 0` 或定时器池已满 |

### check() 行为详情

- **返回 1**：超时已发生，同时自动清除 flag
- **ONCE 模式**：第一次返回 1 后自动将定时器标记为不活跃 (`active = 0`)，后续调用返回 0
- **AUTO 模式**：每次超时都返回 1，定时器保持活跃
- **ID 无效或定时器未激活**：返回 0

## 线程安全 / 中断安全

| 函数 | 保护方式 | 说明 |
|------|----------|------|
| `tick()` | 仅 ISR 调用 | 仅递减 `count` 和置 `flag`，不做 HAL 调用 |
| `start()` | 写入顺序保证 | `active = 1` 在最后，ISR 不会看到半初始化状态 |
| `stop()` | `__disable_irq()` | 保护 `active`/`flag`/`count` 的联动清零 |
| `check()` | `__disable_irq()` | 保护 `flag` 的读-清原子操作 |
| `restart()` | `__disable_irq()` | 保护 `count`/`flag` 的联动重置 |

**注意：** `__disable_irq()` 临界区仅保护最小操作（单条赋值或读改写），不包含任何 HAL 调用或耗时操作。中断延迟可忽略。

## 配置参数

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `SORT_TIMER_MAX_COUNT` | 10 | 编译期定时器池最大容量。修改需重新编译。 |

内存占用估算（`SORT_TIMER_MAX_COUNT = 10`）：

```
sizeof(SORT_TIMER_ENTRY_S) = 12 字节 × 10 = 120 字节
sizeof(SORT_TIMER_CFG_S)   = 1 字节
sizeof(函数指针 × 5)        = 20 字节 (ARM 32-bit)
────────────────────────────────────────────
sizeof(SORT_TIMER_S)       ≈ 144 字节
```

## 移植到其他平台

### 1. 切换硬件定时器源

修改 TIM2 初始化，确保产生 **1ms 中断**。以 TIM2 为例 (STM32F103, 72MHz)：

```c
// CubeMX 配置: TIM2 → Prescaler=71, CounterPeriod=999 → 1ms
// 或在 tim.c 中:
htim2.Init.Prescaler = 72 - 1;        // 72MHz / 72 = 1MHz = 1µs/tick
htim2.Init.Period    = 1000 - 1;       // 1MHz / 1000 = 1kHz = 1ms
htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
```

使用其他 TIMx 时只需在 ISR 回调中修改 `Instance` 判断。

### 2. 更换 MCU 系列

| 步骤 | 操作 |
|------|------|
| 替换 HAL 头文件 | `#include "stm32f1xx_hal.h"` → 对应系列 (如 `stm32f4xx_hal.h`) |
| ISR 回调 | STM32 全系列通用 `HAL_TIM_PeriodElapsedCallback`，无需修改 |
| CMSIS 临界区 | `__disable_irq()` / `__enable_irq()` 全 Cortex-M 通用 |

### 3. 移植到非 STM32 平台

| 步骤 | 操作 |
|------|------|
| 替换 HAL 依赖 | 删除 `#include "stm32f1xx_hal.h"`，改用 `<stdint.h>` (提供 `uint8_t`/`uint32_t`) |
| 替换临界区 | `__disable_irq()` → 目标平台的关中断原语 |
| ISR 集成 | 在目标平台的 1ms 定时器 ISR 中调用 `st.tick(&st)` |

### 4. 调整 tick 时基

当前驱动假设 1 个 tick = 1ms。如需其他时基：

- **10ms tick**：`count` 单位变为 10ms，`start(&st, 50, ...)` 表示 500ms
- **非 1ms 精度场景**：在 `.c` 中 `count--` 逻辑无需改动，只需用户按 tick 单位计算 `period_ms`

### 5. 增加定时器容量

修改 `sort_timer.h` 中的 `SORT_TIMER_MAX_COUNT`：

```c
#define SORT_TIMER_MAX_COUNT  20U   // 从 10 增加到 20
```

## 限制与注意事项

- **tick() 调用频率**：必须与 `start()` 的 `period_ms` 单位一致（默认 1ms）
- **临界区时长**：`__disable_irq()` 保护区域仅数条指令，不包含函数调用
- **ONCE 模式消费时机**：超时后必须调用 `check()` 才会停用，否则仅 ISR 跳过 count=0 的条目（不影响其他定时器）
- **AUTO 模式消费时机**：若主循环耗时超过定时周期，`check()` 返回 1 后 flag 立即被下次 ISR 再次置 1，不会丢失事件
- **不支持回调**：设计为轮询模式，不提供超时回调函数指针

## 文件结构

```
Drivers/BSP/SORT_TIMER/
├── sort_timer.h    # 接口定义 (配置、函数指针类型、对象结构体、构造声明)
├── sort_timer.c    # 驱动实现 (静态函数 + 构造函数绑定)
└── README.md       # 本文件
```
