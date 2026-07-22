# MPU6050 六轴传感器 I2C 驱动

> **面向对象 C 封装** | STM32 HAL 硬件 I2C | 突发读取 | 双模式 (轮询 + INT) | 零动态内存

---

## 概述

本驱动将 MPU6050（三轴加速度计 + 三轴陀螺仪 + 温度传感器）封装为面向对象的 C 模块，遵循项目 [CODE_STYLE.md](../CODE_STYLE.md) 规范。

### 核心特性

- **双模式支持** — `use_int=0` 轮询模式（无额外引脚），`use_int=1` INT 引脚中断模式（CPU 高效）
- **突发读取** — `read_all_raw()` 单次 I2C 事务读取全部 14 字节传感器数据，效率最优、时刻一致
- **两层 API** — 原始 ADC 值 (`int16_t`) + 物理量转换 (`float` g / °/s / °C)
- **高可移植** — I2C 句柄通过配置结构体注入，不硬编码；所有 H/W 相关宏集中在文件头部
- **零依赖** — 不依赖 `printf`、不使用 `malloc`/`free`、纯 HAL 库调用

---

## 硬件连接

```
         STM32F103                MPU6050
        ┌─────────┐             ┌──────────┐
        │     SCL │─────────────│ SCL      │
        │  (PB10) │             │          │
        │     SDA │─────────────│ SDA      │
        │  (PB11) │             │          │
        │         │             │ AD0→GND  │  (地址 = 0x68)
        │    3.3V │─────────────│ VCC      │
        │     GND │─────────────│ GND      │
        │     INT │─────────────│ INT      │  (INT 模式必需, 轮询可省略)
        │  (PA12) │             │          │
        └─────────┘             └──────────┘
```

- `AD0 = GND` → 7 位 I2C 地址为 **0x68**（默认）
- `AD0 = VCC` → 7 位 I2C 地址为 **0x69**（备用）
- 本示例使用 **I2C2**（PB10 = SCL, PB11 = SDA），由 CubeMX 配置
- INT 引脚可选：仅 **INT 模式** 需要（配为 EXTI 下降沿中断）

---

## 文件结构

```
Drivers/BSP/MPU6050/
├── mpu6050.h          # 头文件: 寄存器宏、类型定义、对象结构体、构造函数声明
├── mpu6050.c          # 源文件: 所有功能实现 (static 内部函数 + 构造函数)
└── README.md          # 本文档
```

---

## 快速入门

### 1. CubeMX 配置

- 启用 I2C2（或 I2C1），速度建议 **Fast Mode (400kHz)**
- 启用 USART1（用于 printf 输出，可选）
- 生成代码

### 2. 引入驱动

将 `mpu6050.h` 和 `mpu6050.c` 复制到项目的 `Drivers/BSP/MPU6050/` 目录，并在 CMakeLists.txt 或 IDE 中添加源文件路径。

在 `main.c` 中包含头文件：

```c
#include "mpu6050.h"
```

### 3. 初始化并使用

```c
/* USER CODE BEGIN 0 */
#include "mpu6050.h"

int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}
/* USER CODE END 0 */

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C2_Init();
    MX_USART1_UART_Init();

    /* ---- 创建 MPU6050 对象 ---- */
    MPU6050_S imu;
    MPU6050_CFG_S cfg = {
        .hi2c        = &hi2c2,               /* I2C 句柄 */
        .dev_addr    = MPU6050_DEFAULT_ADDR,  /* 7 位地址 */
        .gyro_range  = MPU6050_GYRO_2000DPS,  /* 陀螺仪 ±2000°/s */
        .accel_range = MPU6050_ACCEL_2G,      /* 加速度 ±2g */
        .dlpf        = MPU6050_DLPF_42HZ,     /* 低通滤波 42Hz */
        .sample_rate = 100,                   /* 采样率 100Hz */
    };
    MPU6050_Create(&imu, &cfg);

    /* ---- 验证芯片是否存在 ---- */
    if (!imu.who_am_i(&imu))
    {
        printf("MPU6050 未检测到, 请检查硬件连接\r\n");
        while (1);
    }
    printf("MPU6050 初始化成功!\r\n");

    /* ---- 主循环 ---- */
    MPU6050_RAW_DATA_S raw;
    while (1)
    {
        if (imu.read_all_raw(&imu, &raw))
        {
            float ax = imu.accel_to_g(&imu, raw.accel_x);
            float ay = imu.accel_to_g(&imu, raw.accel_y);
            float az = imu.accel_to_g(&imu, raw.accel_z);
            float gx = imu.gyro_to_dps(&imu, raw.gyro_x);
            float gy = imu.gyro_to_dps(&imu, raw.gyro_y);
            float gz = imu.gyro_to_dps(&imu, raw.gyro_z);
            float t  = imu.temp_to_c(&imu, raw.temp);

            printf("A: %6.2f %6.2f %6.2f | "
                   "G: %7.2f %7.2f %7.2f | "
                   "T: %5.1f°C\r\n",
                   ax, ay, az, gx, gy, gz, t);
        }
        HAL_Delay(10);
    }
}
```

---

## INT 模式使用

### 原理

MPU6050 每次采样完成后会在 `INT` 引脚输出一个脉冲（默认低电平有效）。INT 模式的流程：

```
MPU6050 采样完成 → INT 引脚拉低 → STM32 EXTI 下降沿中断
  → HAL_GPIO_EXTI_Callback → imu.int_handler(&imu)  [仅置 data_ready=1]
  → 主循环 imu.is_data_ready(&imu) 检测到标志 → 读取数据
```

> **关键设计：不在 ISR 中做 I2C 通信。** `int_handler` 仅置一个 `volatile` 标志位，读取仍在主循环完成。这是嵌入式中断安全的最佳实践。

### CubeMX 配置

1. 在 Pinout 视图中找到 MPU6050 的 INT 引脚（如 PA12），设为 **GPIO_EXTIx**
2. 在 System Core → GPIO 中，将 INT 引脚配置为：
   - GPIO mode: **External Interrupt Mode with Falling edge trigger**
   - GPIO Pull-up/Pull-down: **Pull-up**（MPU6050 INT 为推挽低电平输出）
3. 在 NVIC 中勾选对应的 EXTI 中断使能

### 代码示例

```c
#include "mpu6050.h"

/* ---- 全局对象指针 (EXTI 回调中访问) ---- */
static MPU6050_S g_imu;

/* ---- EXTI 中断回调 ---- */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    /* 判断是否是 MPU6050 的 INT 引脚 */
    if (GPIO_Pin == MPU6050_INT_PIN)
    {
        g_imu.int_handler(&g_imu);  /* 仅置标志, 快速返回 */
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C2_Init();
    MX_USART1_UART_Init();

    /* ---- INT 模式初始化 (use_int=1) ---- */
    MPU6050_CFG_S cfg = {
        .hi2c        = &hi2c2,
        .dev_addr    = MPU6050_DEFAULT_ADDR,
        .gyro_range  = MPU6050_GYRO_2000DPS,
        .accel_range = MPU6050_ACCEL_2G,
        .dlpf        = MPU6050_DLPF_42HZ,
        .sample_rate = 100,
        .use_int     = 1,                   /* 开启 INT 模式 */
    };
    MPU6050_Create(&g_imu, &cfg);

    if (!g_imu.who_am_i(&g_imu))
    {
        printf("MPU6050 未检测到!\r\n");
        while (1);
    }

    /* ---- 主循环 (统一写法, 两种模式通用) ---- */
    MPU6050_RAW_DATA_S raw;
    while (1)
    {
        if (g_imu.is_data_ready(&g_imu))    /* INT: 有数据才进入; 轮询: 永真 */
        {
            if (g_imu.read_all_raw(&g_imu, &raw))
            {
                float ax = g_imu.accel_to_g(&g_imu, raw.accel_x);
                float gz = g_imu.gyro_to_dps(&g_imu, raw.gyro_z);
                printf("A: %.2fg  G: %.2f°/s\r\n", ax, gz);
            }
        }
        /* INT 模式下 CPU 可在此处理其他任务, 不必忙等 */
    }
}
```

### 双模式对比

| | 轮询模式 (`use_int=0`) | INT 模式 (`use_int=1`) |
|---|---|---|
| 额外引脚 | 不需要 | 需要 INT → EXTI |
| CPU 占用 | 忙等或定时器触发 | 中断通知, CPU 空闲时可休眠 |
| `is_data_ready()` | 永远返回 `1` | 有数据返回 `1`, 无数据返回 `0` |
| `int_handler()` | 空操作 | 置 `data_ready = 1` |
| 主循环写法 | **完全相同** `if (imu.is_data_ready(...))` | **完全相同** |
| 适用场景 | 教学、简单采集 | 省电、多任务 RTOS、配合低功耗 |

---

## API 参考

### 类型一览

| 类型 | 说明 |
|------|------|
| `MPU6050_GYRO_RANGE_T` | 陀螺仪量程枚举: `_250DPS`, `_500DPS`, `_1000DPS`, `_2000DPS` |
| `MPU6050_ACCEL_RANGE_T` | 加速度计量程枚举: `_2G`, `_4G`, `_8G`, `_16G` |
| `MPU6050_DLPF_T` | DLPF 截止频率枚举: `_256HZ` ~ `_5HZ` |
| `MPU6050_CFG_S` | 硬件配置结构体 |
| `MPU6050_RAW_DATA_S` | 单次原始采样数据 (7 个 int16_t 字段) |
| `MPU6050_S` | 外设对象 (配置 + 内部状态 + 函数指针) |

### 构造函数

```c
void MPU6050_Create(MPU6050_S *imu, MPU6050_CFG_S *config);
```

创建并初始化 MPU6050 对象。`imu` 和 `config` 由调用者分配内存（栈上或全局区均可）。

### 初始化与状态

| 函数指针 | 签名 | 说明 |
|----------|------|------|
| `init` | `void init(MPU6050_S *imu)` | 重新初始化芯片寄存器 |
| `reset` | `uint8_t reset(MPU6050_S *imu)` | 软件复位, 返回 0=失败 1=成功 |
| `who_am_i` | `uint8_t who_am_i(MPU6050_S *imu)` | 验证芯片存在, 返回 1=检测到 |

### 数据读取（原始值）

| 函数指针 | 说明 | I2C 事务 |
|----------|------|----------|
| `read_all_raw` | **推荐** — 突发读取加速度+温度+陀螺仪, 填充全部字段 | 1 次, 14 字节 |
| `read_accel_raw` | 仅读取加速度 (X/Y/Z), 填充 `accel_*` 字段 | 1 次, 6 字节 |
| `read_gyro_raw` | 仅读取陀螺仪 (X/Y/Z), 填充 `gyro_*` 字段 | 1 次, 6 字节 |
| `read_temp_raw` | 仅读取温度, 填充 `temp` 字段 | 1 次, 2 字节 |

全部返回 `1` = 成功, `0` = I2C 通信失败。

### 运行中修改配置

| 函数指针 | 说明 | 自动更新转换系数 |
|----------|------|:---:|
| `set_gyro_range(range)` | 修改陀螺仪量程 | ✓ |
| `set_accel_range(range)` | 修改加速度计量程 | ✓ |
| `set_dlpf(dlpf)` | 修改 DLPF 截止频率 | — |
| `set_sample_rate(rate_hz)` | 修改采样率 (4~1000Hz) | — |

### INT 模式控制

| 函数指针 | 签名 | 说明 |
|----------|------|------|
| `int_handler` | `void int_handler(MPU6050_S *imu)` | EXTI 回调中调用, 仅置 `data_ready=1`。轮询模式为空操作 |
| `is_data_ready` | `uint8_t is_data_ready(MPU6050_S *imu)` | 轮询: 永返 1; INT: 返回并清除 `data_ready` |

### 数据转换（原始值 → 物理量）

| 函数指针 | 公式 |
|----------|------|
| `accel_to_g(imu, raw)` | `raw / accel_scale` (g) |
| `gyro_to_dps(imu, raw)` | `raw / gyro_scale` (°/s) |
| `temp_to_c(imu, raw)` | `raw / 340.0 + 36.53` (°C) |

### 配置结构体

```c
typedef struct {
    I2C_HandleTypeDef    *hi2c;         // I2C 句柄 (如 &hi2c2)
    uint8_t               dev_addr;     // 7 位地址 (0 = 使用默认 0x68)
    MPU6050_GYRO_RANGE_T  gyro_range;   // 陀螺仪量程
    MPU6050_ACCEL_RANGE_T accel_range;  // 加速度计量程
    MPU6050_DLPF_T        dlpf;         // DLPF 截止频率
    uint16_t              sample_rate;  // 采样率 Hz (0 = 使用默认 100)
    uint8_t               use_int;      // 0=轮询模式, 1=INT 引脚中断模式 (默认 0)
} MPU6050_CFG_S;
```

### 原始数据结构体

```c
typedef struct {
    int16_t accel_x;    // 加速度 X 轴原始值
    int16_t accel_y;    // 加速度 Y 轴原始值
    int16_t accel_z;    // 加速度 Z 轴原始值
    int16_t temp;       // 温度原始值
    int16_t gyro_x;     // 陀螺仪 X 轴原始值
    int16_t gyro_y;     // 陀螺仪 Y 轴原始值
    int16_t gyro_z;     // 陀螺仪 Z 轴原始值
} MPU6050_RAW_DATA_S;
```

---

## 移植指南

### 移植到其他 STM32 系列 (F4 / G0 / H7 / L4 ...)

**步骤 1: 修改 HAL 头文件**

在 `mpu6050.h` 中:

```c
// 将
#include "stm32f1xx_hal.h"
// 改为对应系列, 例如:
#include "stm32f4xx_hal.h"
// #include "stm32g0xx_hal.h"
// #include "stm32h7xx_hal.h"
```

**步骤 2: 调整 I2C 地址宏 (关键!)**

STM32F0/F1/F3/L0/L1 HAL 要求 I2C 7 位地址**左移 1 位**; F4/F7/G0/H7/L4 等新 HAL **不左移**。

在 `mpu6050.h` 中找到 `MPU6050_HAL_DEVADDR` 宏:

```c
// STM32F1 (当前)
#define MPU6050_HAL_DEVADDR(addr)  ((addr) << 1)

// STM32F4 / G0 / H7 / L4 等新 HAL — 不左移:
#define MPU6050_HAL_DEVADDR(addr)  (addr)
```

**步骤 3: 修改 I2C 句柄**

在配置结构体中替换为实际 I2C 句柄:

```c
MPU6050_CFG_S cfg = {
    .hi2c = &hi2c1,   // 改为你的 I2C 外设
    // ...
};
```

**步骤 4: 调整超时时间 (可选)**

默认 100ms, 在 `mpu6050.h` 中修改:

```c
#define MPU6050_I2C_TIMEOUT_MS  100
```

### 移植到非 STM32 平台

本驱动核心逻辑与平台无关。移植步骤:

1. **I2C 读写适配** — 修改 `i2c_write_reg()` 和 `i2c_read_reg()` 两个 static 函数，替换为你的平台 I2C API
2. **延时适配** — 将 `HAL_Delay(100)` 替换为你的平台延时函数
3. **类型适配** — 调整 `#include` 和 `I2C_HandleTypeDef` 类型
4. 其他代码无需修改

### 修改默认 I2C 地址

如果 AD0 接 VCC:

```c
MPU6050_CFG_S cfg = {
    .dev_addr = MPU6050_ALT_ADDR,  // 0x69
    // ...
};
```

---

## 常见问题

### Q: 读出的数据全是 0 或不变？

1. 用 `imu.who_am_i()` 确认 I2C 通信正常
2. 检查 `MPU6050_HAL_DEVADDR` 宏是否正确（F1 需左移，F4 不需）
3. 检查 I2C 时钟是否使能，GPIO 是否配置为复用开漏

### Q: 数据波动很大？

1. 检查电源稳定性（MPU6050 对电源噪声敏感）
2. 降低 DLPF 截止频率（如 `MPU6050_DLPF_20HZ`）
3. 在应用层增加滑动平均滤波

### Q: 不使用 `read_all_raw` 而分别读取会怎样？

分别读取会产生 3 次 I2C 事务，加速度和陀螺仪数据可能来自不同采样时刻。对于姿态解算等需要数据一致性的应用，务必使用 `read_all_raw`。

### Q: 为什么 run_all_raw 的转换系数是这样？

参考 MPU6050 数据手册:

| 量程 | 加速度 LSB/g | 陀螺仪 LSB/(°/s) |
|------|:-----------:|:----------------:|
| ±2g / ±250°/s   | 16384 | 131.0 |
| ±4g / ±500°/s   |  8192 |  65.5 |
| ±8g / ±1000°/s  |  4096 |  32.8 |
| ±16g / ±2000°/s |  2048 |  16.4 |

### Q: INT 模式下收不到中断怎么办？

1. 确认 CubeMX 中 INT 引脚配置为 **下降沿触发** (Falling edge)，因为 MPU6050 INT 默认低电平有效
2. 确认 NVIC 中对应 EXTI 中断已使能
3. 用示波器或逻辑分析仪抓 INT 引脚波形，正常应有周期性负脉冲
4. 检查 `use_int = 1` 是否正确设置（打印 `imu.cfg.use_int` 确认）
5. 检查主循环是否调用了 `is_data_ready()`（它负责清除 `data_ready` 标志）

### Q: 能否同时使用轮询和 INT 模式？

不能。`use_int` 只能在创建对象时选择一种，之后两种模式的 `is_data_ready()` 行为不同。但主循环的写法是**完全一样的**：

```c
// 这段代码在两种模式下都能正确工作
if (imu.is_data_ready(&imu))     // 轮询: 永真; INT: 仅数据就绪时真
{
    imu.read_all_raw(&imu, &raw);
    // 处理数据...
}
```

轮询模式下你需要在循环中加 `HAL_Delay` 控制速率；INT 模式下由硬件中断驱动，不需要额外延时。

---

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.1 | 2026-07-22 | 增加 INT 模式: `use_int` 配置、`int_handler`、`is_data_ready` |
| v1.0 | 2026-07-22 | 初版: OOP 封装, 硬件 I2C, 突发读取, 两层 API |
