# STM32 驱动代码风格规范

> **目标读者:** 需要编写新驱动的 AI Agent 或开发者。
> **适用范围:** STM32F4 HAL 库外设驱动，位于 `Drivers/BSP/` 目录下。
> **核心原则:** 面向对象 C 封装、Doxygen 中文注释、4 空格缩进、静态绑定函数指针。

---

## 目录

1. [文件组织](#1-文件组织)
2. [头文件完整结构](#2-头文件完整结构)
3. [源文件完整结构](#3-源文件完整结构)
4. [面向对象 C 模式 (OOP) — 最核心章节](#4-面向对象-c-模式-oop--最核心章节)
5. [命名规范 (精确到每个字符)](#5-命名规范-精确到每个字符)
6. [注释规范 (Doxygen 全中文)](#6-注释规范-doxygen-全中文)
7. [格式化规范 (精确规则)](#7-格式化规范-精确规则)
8. [代码分段与分隔符](#8-代码分段与分隔符)
9. [变量与类型使用惯例](#9-变量与类型使用惯例)
10. [中断安全与临界区](#10-中断安全与临界区)
11. [GPIO / I2C / SPI / UART 初始化惯用写法](#11-gpio--i2c--spi--uart-初始化惯用写法)
12. [常见设计模式速查](#12-常见设计模式速查)
13. [完整模板 (可直接复制)](#13-完整模板-可直接复制)
14. [禁止事项](#14-禁止事项)

---

## 1. 文件组织

```
BSP/
├── MODULE_NAME/              # 目录名为模块大写缩写
│   ├── module_name.h         # 头文件: 小写+下划线
│   └── module_name.c         # 源文件: 小写+下划线
```

**硬性规则:**
- 目录名用**全大写**，如 `KEY/`、`INMP441/`、`USART_RING/`、`OLED/`、`LED/`
- `.h` 和 `.c` 文件名用**全小写+下划线**: `usart_ring.c`、`inmp441_fft.c`、`stmflash.c`
- 一个模块可以拆分子模块: `inmp441.c` + `inmp441_fft.c`，放在同一目录下
- 头文件包含 `"main.h"` 或 `"stm32f4xx_hal.h"`，不包含无关头文件

---

## 2. 头文件完整结构

### 2.1 必须遵循的骨架 (从上到下顺序固定)

```c
/**
 * @file    module_name.h
 * @brief   <一句话中文描述> — 面向对象封装
 * @note    <关键设计说明、数据流、硬件依赖、调用限制>
 *
 * @example 使用示例:
 *   MODULE_S obj;
 *   MODULE_CFG_S cfg = {
 *       .param1 = val1,
 *       .param2 = val2,
 *   };
 *   MODULE_Create(&obj, &cfg);
 *   obj.do_something(&obj, args);
 */

#ifndef _MODULE_NAME_H_              /* 格式: _大写_H_ */
#define _MODULE_NAME_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"                      /* 或 stm32f4xx_hal.h、arm_math.h */

/* ======================== 常量定义 ======================== */

/** <常量中文说明> */
#define MODULE_CONSTANT     数值

/* ======================== 类型定义 ======================== */

/** <枚举中文说明> */
typedef enum
{
    MODULE_ENUM_VAL1 = 0,       /**< 中文说明 */
    MODULE_ENUM_VAL2,           /**< 中文说明 */
} MODULE_ENUM_T;

/** <配置结构体中文说明> */
typedef struct
{
    类型     成员名;              /**< 中文说明 */
    类型     成员名;              /**< 中文说明 */
} MODULE_CFG_S;

/* 前向声明 */
typedef struct MODULE_S MODULE_S;

/** 函数指针类型定义 */
typedef 返回值  (*MODULE_功能_FUNC)(MODULE_S *obj, ...);

/** <对象结构体中文说明> */
struct MODULE_S
{
    MODULE_CFG_S cfg;                   /**< 硬件配置 */

    /* ---- 内部状态 (用户不应直接访问) ---- */
    volatile 类型 成员名;               /**< 中文说明 */
    类型          成员名;               /**< 中文说明 */

    /* ---- 成员函数 ---- */
    MODULE_功能_FUNC  成员名;           /**< 中文说明 */
};

/* ======================== 构造函数 ======================== */
void MODULE_Create(MODULE_S *obj, MODULE_CFG_S *config);

#ifdef __cplusplus
}
#endif

#endif /* _MODULE_NAME_H_ */
```

### 2.2 Include Guard 精确格式

```c
#ifndef _MODULE_NAME_H_
#define _MODULE_NAME_H_
...
#endif /* _MODULE_NAME_H_ */
```

- 宏名 = `_` + 文件名大写 + `_H_`，点号换成下划线
- 例如 `usart_ring.h` → `_USART_RING_H_`，`inmp441_fft.h` → `_INMP441_FFT_H_`
- `#endif` 后**必须**加 `/* _MODULE_NAME_H_ */` 注释

### 2.3 C++ 兼容 (每个头文件必修)

```c
#ifdef __cplusplus
extern "C" {
#endif

/* 所有声明放在这里 */

#ifdef __cplusplus
}
#endif
```

### 2.4 Include 顺序

```c
#include "main.h"           /* 首选，包含所有 HAL 和 BSP 基础定义 */
// 如果模块不依赖具体项目:
#include "stm32f4xx_hal.h"  /* 仅包含 HAL */
// 特殊依赖:
#include "arm_math.h"       /* CMSIS-DSP */
#include <stdarg.h>         /* vsnprintf 等 */
#include <string.h>         /* memset, memcpy */
#include <stdio.h>          /* printf (仅调试用) */
```

---

## 3. 源文件完整结构

```c
/**
 * @file    module_name.c
 * @brief   <中文实现描述>
 * @note    面向对象风格: 所有函数通过 MODULE_S 对象调用。
 *          <其它关键实现细节>
 */

#include "module_name.h"
/* 需要的其它 include，如 <string.h>、<stdio.h> */

/* ======================== 内部数据 ======================== */

/** <静态变量说明> */
static MODULE_S *g_module = NULL;         /* 全局单例指针，DMA 回调用 */
static float32_t s_buffer[512] = {0};    /* 预计算缓存 */

/* ======================== 内部辅助 ======================== */

/**
 * @brief  <函数功能中文描述>
 * @param  obj: 对象指针
 * @return <返回值说明>
 */
static void helper_func(MODULE_S *obj)
{
    /* ... */
}

/* ======================== 核心功能 ======================== */

/**
 * @brief  初始化对象状态 (硬件已在 CubeMX 完成)
 */
static void module_init(MODULE_S *obj)
{
    /* 重置所有状态变量 */
    /* 清零缓冲 */
    /* 注册全局指针 */
}

/**
 * @brief  <其他核心功能>
 * @param  obj: 对象指针
 * @param  arg: 参数说明
 * @return 0=失败, 1=成功
 */
static uint8_t module_do_something(MODULE_S *obj, int arg)
{
    /* 参数校验 */
    /* 实际操作 */
    /* 返回结果 */
}

/* ======================== 构造函数 ======================== */

/**
 * @brief  <模块名> 对象构造函数
 * @param  obj:    对象指针 (由调用者分配)
 * @param  config: 硬件配置指针 (由调用者分配)
 * @note   调用后外设即启动，无需再调 init() 或 start()
 *
 * @example 使用示例:
 *   MODULE_S dev;
 *   MODULE_CFG_S cfg = {
 *       .param1 = val1,
 *       .param2 = val2,
 *   };
 *   MODULE_Create(&dev, &cfg);
 *   dev.do_something(&dev, 42);
 */
void MODULE_Create(MODULE_S *obj, MODULE_CFG_S *config)
{
    /* 1. 绑定配置 (值拷贝) */
    obj->cfg = *config;

    /* 2. 填充未设置的默认值 (以 0 为"使用默认"的信号) */
    if (obj->cfg.timeout_ms == 0)
        obj->cfg.timeout_ms = MODULE_DEFAULT_TIMEOUT_MS;

    /* 3. 绑定函数指针 */
    obj->init         = module_init;
    obj->do_something = module_do_something;

    /* 4. 调 init() 完成硬件初始化和状态复位 */
    obj->init(obj);
}
```

**关键规则:**
- 所有绑定到对象函数指针的实现函数必须是 `static`
- 不需要绑为对象成员的公共 API（如 `OLED_SetContrast`）声明为普通非 static 函数，写在构造函数分段之前
- 构造函数必须是最后一个分段，放在文件末尾

---

## 4. 面向对象 C 模式 (OOP) — 最核心章节

所有外设驱动**必须**遵从此 OOP 模式。整个模式由四部分构成。

### 4.1 配置结构体 `_CFG_S`

包含该外设所有**不经常改变**的硬件参数和配置项:

```c
/** INMP441 MEMS 麦克风硬件配置 */
typedef struct
{
    I2S_HandleTypeDef *hi2s;            /**< 已初始化的 I2S 句柄 (Master RX, 24-bit) */
    INMP441_CHANNEL_T  channel;         /**< 声道选择 */
} INMP441_CFG_S;

/** UART 环形缓冲区配置 */
typedef struct
{
    UART_HandleTypeDef *huart;          /**< UART 句柄指针 */
    uint8_t *rx_buf;                    /**< 接收环形缓冲区 (用户分配) */
    uint16_t rx_buf_size;               /**< 接收缓冲区大小 */
    uint8_t *tx_buf;                    /**< 发送缓冲区 (用户分配) */
    uint16_t tx_buf_size;               /**< 发送缓冲区大小 */
    uint32_t baud_rate;                 /**< 初始波特率 */
} USART_RING_CFG_S;

/** LED 硬件配置 */
typedef struct
{
    GPIO_TypeDef    *GPIOx;             /**< LED 连接的 GPIO 端口 */
    uint16_t         GPIO_Pin;          /**< LED 连接的 GPIO 引脚 */
    LED_POLARITY_T   polarity;          /**< LED 的极性 (高/低电平点亮) */
} LED_CFG_S;
```

**规则:**
- 类型名: `模块大写_CFG_S`
- 成员顺序: HAL 句柄 → GPIO 配置 → 功能参数 → 可选参数
- 可选参数使用 `0` 作为"使用默认值"的标记，构造函数中会替换为默认宏

### 4.2 枚举类型 `_T`

```c
/** 按键事件类型 */
typedef enum
{
    KEY_EVENT_NONE = 0,     /**< 无事件 */
    KEY_EVENT_DOWN,         /**< 按下 */
    KEY_EVENT_UP,           /**< 弹起 */
    KEY_EVENT_LONG,         /**< 长按 */
    KEY_EVENT_REPEAT,       /**< 连发 */
} KEY_EVENT_T;

/** 按键激活电平 */
typedef enum
{
    KEY_ACTIVE_HIGH = 0,    /**< 高电平激活 */
    KEY_ACTIVE_LOW,         /**< 低电平激活 (内部上拉, 按下接地) */
} KEY_ACTIVE_LEVEL_T;

/** LED 极性 */
typedef enum
{
    LED_ACTIVE_HIGH = 0,    /**< LED 正极接 IO, 负极接 GND */
    LED_ACTIVE_LOW,         /**< LED 负极接 IO, 正极接 VCC */
} LED_POLARITY_T;
```

**规则:**
- 类型名: `模块大写_含义_T`
- 枚举值: `模块大写_含义大驼峰`，如 `KEY_EVENT_DOWN`、`LED_ACTIVE_HIGH`
- 第一个值显式赋值 `= 0`，后续值不写 `=`（靠自动递增）
- 关于两个选项（如高低电平），`= 0` 的那个表示默认/常见情况

### 4.3 函数指针类型定义 `_FUNC`

```c
/* 在头文件中，typedef struct MODULE_S MODULE_S; 之后 */
typedef void        (*KEY_INIT_FUNC)(KEY_S *key);
typedef void        (*KEY_RESET_FUNC)(KEY_S *key);
typedef void        (*KEY_SCAN_FUNC)(KEY_S *key);
typedef uint8_t     (*KEY_GET_ID_FUNC)(KEY_S *key);
typedef KEY_EVENT_T (*KEY_GET_EVENT_FUNC)(KEY_S *key);
typedef KEY_STATE_T (*KEY_GET_STATE_FUNC)(KEY_S *key);

typedef void     (*LED_INIT_FUNC)(LED_S *led);
typedef void     (*LED_ON_FUNC)(LED_S *led);
typedef void     (*LED_OFF_FUNC)(LED_S *led);
typedef void     (*LED_TOGGLE_FUNC)(LED_S *led);
typedef LED_STATE_T (*LED_GETSTATE_FUNC)(LED_S *led);

typedef int      (*USART_RING_SEND_FUNC)(USART_RING_S *handle, const char *format, ...);
typedef int      (*USART_RING_SEND_RAW_FUNC)(USART_RING_S *handle, const uint8_t *data, uint16_t len);
```

**规则:**
- 类型名: `模块大写_功能大写_FUNC`
- 第一个参数**始终**是 `MODULE_S *obj`（指向自身对象的指针）
- 返回值类型以及函数指针的 `()` 垂直对齐
- typedef 语句末尾不加多余空格

### 4.4 对象结构体 `_S`

```c
struct KEY_S
{
    /* ---- 配置 (不可变) ---- */
    KEY_CFG_S config;

    /* ---- 内部状态 (用户不应直接访问) ---- */
    KEY_STATE_T state;
    uint8_t     filter_count;
    uint16_t    press_ticks;
    uint16_t    repeat_count;

    /* ---- FIFO 事件缓冲 ---- */
    struct
    {
        KEY_EVENT_T buf[10];
        uint8_t head;
        uint8_t tail;
        uint8_t count;
    } fifo;

    /* ---- 成员函数 (函数指针) ---- */
    KEY_INIT_FUNC               init;
    KEY_RESET_FUNC              reset;
    KEY_SCAN_FUNC               scan;
    KEY_GET_ID_FUNC             get_id;
    KEY_GET_EVENT_FUNC          get_event;
    KEY_GET_EVENT_TIMEOUT_FUNC  get_event_timeout;
    KEY_GET_STATE_FUNC          get_state;
    KEY_EVENT_COUNT_FUNC        event_count;
};
```

**规则:**
- 类型名: `模块大写_S`（如 `KEY_S`、`LED_S`、`INMP441_S`、`OLED_S`、`USART_RING_S`）
- **必须**先做前向声明 `typedef struct MODULE_S MODULE_S;`，否则函数指针类型定义中无法引用自身
- 成员排列顺序: `cfg` → 内部状态变量 → 子结构体 → 函数指针
- 内部状态区域以 `/* ---- 内部状态 (用户不应直接访问) ---- */` 分隔
- 函数指针区域以 `/* ---- 成员函数 ---- */` 分隔
- 内部状态中的 `volatile` 标识: 仅 ISR 和主循环**同时**读写的变量加 volatile，仅主循环访问的不加

### 4.5 构造函数 `_Create()` 的精确结构

构造函数总是做四件事，顺序固定:

```c
void MODULE_Create(MODULE_S *obj, MODULE_CFG_S *config)
{
    /* 绑定配置 (值拷贝) */
    obj->cfg = *config;

    /* 设置默认值 — 如果配置中某字段为 0，替换为默认宏 */
    if (obj->cfg.filter_time_ms == 0)
        obj->cfg.filter_time_ms = MODULE_DEFAULT_FILTER_MS;
    if (obj->cfg.long_press_time_ms == 0)
        obj->cfg.long_press_time_ms = MODULE_DEFAULT_LONG_PRESS_MS;

    /* 绑定函数指针 */
    obj->init      = module_init;
    obj->start     = module_start;
    obj->do_thing  = module_do_thing;

    /* 执行 init() — 完成硬件初始化，设备就绪 */
    obj->init(obj);
}
```

**注意:**
- 如果模块启动后就持续运行（如 UART DMA 接收），构造函数里可以再调一次 `obj->start(obj)` 或直接在 `init()` 内启动
- 构造函数注释必须包含 `@example` 展示完整调用链

---

## 5. 命名规范 (精确到每个字符)

### 5.1 类型后缀

| 后缀 | 含义 | 仅用于 |
|------|------|--------|
| `_T` | 枚举类型 | `typedef enum { ... } NAME_T;` |
| `_S` | 结构体类型 | `typedef struct { ... } NAME_S;` |
| `_FUNC` | 函数指针类型 | `typedef 返回值 (*NAME_FUNC)(...);` |

### 5.2 变量命名

| 作用域 | 前缀 | 示例 |
|--------|------|------|
| 全局变量 (非 static) | `g_` | `volatile HARD_FAULT_FRAME_T g_hardfault_frame;` |
| 文件级静态全局 | `s_` | `static float32_t s_hann_window[512];` |
| 全局单例指针 (用于 DMA 回调) | `g_` | `static INMP441_S *g_mic = NULL;` |
| 局部变量 | 无前缀, snake_case | `filter_threshold`, `read_len`, `peak_mag` |
| 循环变量 | `i`, `j`, `col`, `row`, `page` | 可用简短名 |

### 5.3 函数命名

| 类型 | 风格 | 示例 |
|------|------|------|
| 构造函数 | `模块大写_Create` (PascalCase_动词) | `KEY_Create`, `INMP441_Create`, `OLED_Create`, `LED_Create`, `USART_RING_Create` |
| 非成员的公共 API | `模块大写_动词名词` (PascalCase) | `OLED_SetContrast`, `OLED_ScrollHorizontal`, `INMP441_FFT_Create`, `INMP441_FFT_Process` |
| 对象成员实现 (static) | `模块小写_动词名词` (全 snake_case) | `key_init`, `key_scan`, `led_on`, `led_toggle`, `oled_refresh`, `inmp441_start`, `usart_ring_read` |
| HAL/DMA 回调 (static) | `模块小写_dma_事件` | `inmp441_dma_ht_cplt`, `inmp441_dma_tc_cplt` |
| 内部辅助 (static) | snake_case | `fifo_push`, `fifo_pop`, `is_pressed`, `check_release`, `ring_used_unlocked`, `mark_dirty`, `send_page_blocking` |
| HAL 库重写 | 与 HAL 同名 | `void HAL_Delay(uint32_t Delay)` |

**关键区分:** 
- 头文件中声明给用户看的函数（构造函数、公共 API） → **PascalCase 前缀**: `LED_Create`、`OLED_SetContrast`
- .c 文件中绑定到对象函数指针的内部实现 → **全小写 snake_case**: `led_init`、`oled_set_pixel`

### 5.4 宏常量

```c
#define INMP441_DMA_BUF_TOTAL      4096    /* 全大写 + 下划线，值与注释之间用空格填充对齐 */
#define INMP441_FRAMES_PER_HALF    512
#define KEY_FIFO_SIZE              10
#define KEY_DEFAULT_FILTER_MS      20
#define OLED_WIDTH                 128
#define OLED_HEIGHT                64
#define USART_RING_OK              0
#define USART_RING_ERR             (-1)
#define USART_RING_BUSY            (-2)
```

- 全大写 + 下划线
- 数值和行尾注释至少间隔 4 个空格，同类宏的数字部分左对齐

---

## 6. 注释规范 (Doxygen 全中文)

### 6.1 文件头注释 (每个 .h 和 .c 必修)

```c
/**
 * @file    inmp441.h
 * @brief   INMP441 MEMS 麦克风 I2S 驱动 — 面向对象封装 (DMA 双缓冲 + 零拷贝)
 * @note    使用 DMA Double Buffer Mode 实现 ping-pong 零拷贝交付。
 *          驱动不负责 I2S/DMA 硬件初始化（由 CubeMX 生成的 i2s.c / dma.c 完成），
 *          仅依赖已配置好的 I2S_HandleTypeDef 句柄指针。
 *
 *          数据流:
 *          I2S@192kHz → DMA DBM (NDTR=4096)
 *                       ├─ M0AR: dma_buf[0..2047]    (2.67ms 填满)
 *                       └─ M1AR: dma_buf[2048..4095] (2.67ms 填满)
 *          HT/TC 中断 → 提取 512 采样到 out_buf[ping/pong] → 切换指针
 *          主循环 → tryGetBlock() 零拷贝获取指针
 *
 * @example 左声道 (INMP441 典型用法):
 *   INMP441_S mic;
 *   INMP441_CFG_S cfg = { .hi2s = &hi2s1, .channel = INMP441_CHANNEL_LEFT };
 *   INMP441_Create(&mic, &cfg);
 *
 *   int32_t *data_l, *data_r;
 *   if (mic.tryGetBlock(&mic, &data_l, &data_r)) {
 *       for (int i = 0; i < 512; i++) process(data_l[i]);
 *   }
 */
```

**要求:**
- `@file` 必须给文件名
- `@brief` 一句话描述，格式: `<外设名> <中文名> <通信接口> 驱动 — 面向对象封装 (<关键特性>)`
- `@note` 必须写: 硬件依赖、数据流、关键设计决策、调用限制
- `@example` 必须给出从声明到调用的完整使用示例

### 6.2 函数注释 (每个函数必修)

```c
/**
 * @brief  启动 I2S DMA 双缓冲接收
 * @param  mic: 麦克风对象指针
 * @return 0 = 无新数据, 1 = 有新数据块
 * @note   绕过 HAL_I2S_Receive_DMA，直接操作 DMA 寄存器以启用 DBM。
 *         CubeMX 生成的 DMA 初始化已配置好:
 *           - CHSEL=3, DIR=P2M, PSIZE=WORD, MSIZE=WORD
 *           - PINC=DISABLE, MINC=ENABLE, CIRC=ENABLE
 *         本函数在此基础上:
 *           - 写 M0AR / M1AR / NDTR
 *           - 置位 DBM (要求 CIRC 已置位)
 *           - 注册 HT/TC 回调到 HAL DMA handle
 *           - 使能 HT/TC/ERR 中断
 *
 * @example 单声道:
 *   int32_t *data_l, *data_r;
 *   if (mic.tryGetBlock(&mic, &data_l, &data_r)) {
 *       for (int i = 0; i < 512; i++) printf("%ld\r\n", data_l[i]);
 *   }
 */
static uint8_t inmp441_try_get_block(INMP441_S *mic,
                                     int32_t **out_l, int32_t **out_r)
```

**格式规则:**
- `@brief` 后跟一个空格，中文句号结束
- `@param` 的 `:` 后面跟一个空格，参数说明可以横向对齐美观
- `@return` 和 `@retval` 不同: `@return` 描述返回值，`@retval` 描述每个值的含义，无返回值的函数写 `@retval 无`
- `@note` 可以有多行，每行缩进 2 空格或对齐到 `@note` 文字起始位置
- `@example` 下的代码缩进 4 空格，代码块顶格空一行
- static 内部函数可以省略 `@example`

### 6.3 枚举和结构体成员注释

```c
/** 按键事件类型 */
typedef enum
{
    KEY_EVENT_NONE = 0,     /**< 无事件 */
    KEY_EVENT_DOWN,         /**< 按下 */
    KEY_EVENT_UP,           /**< 弹起 */
    KEY_EVENT_LONG,         /**< 长按 (持续按下超过阈值) */
    KEY_EVENT_REPEAT,       /**< 连发 (长按后周期性触发) */
} KEY_EVENT_T;

/** 按键硬件配置 */
typedef struct
{
    uint8_t          id;                 /**< 按键 ID (用于多按键区分) */
    GPIO_TypeDef    *port;               /**< GPIO 端口 */
    uint16_t         pin;                /**< GPIO 引脚 */
    KEY_ACTIVE_LEVEL_T active_level;     /**< 激活电平 */
    uint16_t         filter_time_ms;     /**< 消抖时间 (ms), 默认 50 */
    uint16_t         long_press_time_ms; /**< 长按判定时间 (ms), 默认 1000 */
    uint16_t         repeat_delay_ms;    /**< 连发启动延时 (ms), 默认 500 */
    uint16_t         repeat_speed_ms;    /**< 连发间隔 (ms), 默认 100 */
} KEY_CFG_S;
```

**规则:**
- 类型定义**上一行**用 `/** <说明> */`
- 每个成员**右侧**用 `/**< <说明> */` 
- 成员名用空格对齐（不对齐也可以，但对齐更美观）
- `/**<` 与代码末尾之间用空格填充到至少间隔 4 个空格

### 6.4 行内注释

```c
/* 行内注释用 C 风格，前后各空一格与代码分隔 */
SysTick->CTRL = 0;                              /* 清 Systick 状态，以便下一步重设 */
SysTick->VAL = 0x00;                            /* 清空计数器 */

/* 注释掉的废弃代码用 C++ 双斜杠 */
//#define BSP_Printf   printf

/* 子段标记 (全小写) */
/* ---- I2S 溢出恢复 ---- */
/* ---- DMA 中断回调 ---- */

/* 关键步骤注释 (在代码上方) */
/* 使能对应的 GPIO 时钟 */
if (led_s->led_cfg.GPIOx == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
```

---

## 7. 格式化规范 (精确规则)

### 7.1 缩进
- **4 个空格**，不用 Tab
- 续行缩进: 对齐到参数起始位置

```c
/* 续行对齐示例 */
static KEY_EVENT_T key_get_event_timeout(KEY_S *key, KEY_EVENT_T want,
                                          uint32_t timeout_ms);

void OLED_ScrollHorizontal(OLED_S *oled, uint8_t start_page,
                           uint8_t end_page, uint8_t direction);
```

### 7.2 大括号 — 精确规则

| 场景 | 规则 | 示例 |
|------|------|------|
| 函数定义 | `{` **另起一行** | `void func(void)\n{` |
| if / else / for / while | `{` **紧跟在同一行** | `if (x) {` |
| do-while | `do` 另起一行，`while` 紧跟 `}` | 见下方 |
| 单语句 (短) | 可省大括号，同行或下一行 | `if (x < 0) return;` |

```c
void func_name(void)
{
    if (condition)
    {
        /* 多行代码 */
    }
    else if (other)
    {
        /* ... */
    }
    else
    {
        /* ... */
    }

    for (int i = 0; i < n; i++)
    {
        /* ... */
    }

    do
    {
        temp = SysTick->CTRL;
    } while ((temp & 0x01) && !(temp & (1 << 16)));

    /* 简单语句不加大括号 */
    if (threshold == 0) threshold = 1;
    if (key->fifo.count >= KEY_FIFO_SIZE)
        return 0;
}
```

### 7.3 switch-case

```c
switch (key->state)
{
case KEY_STATE_RELEASED:
    if (pressed)
    {
        /* ... */
    }
    break;

case KEY_STATE_PRESSED:
    /* ... */
    break;

case KEY_STATE_LONG_PRESSED:
    /* ... */
    break;

default:
    break;
}
```

- `case` 不缩进，与 `switch` 对齐
- `case` 内容缩进 4 空格
- 每个 case 以 `break;` 结尾
- case 之间空一行

### 7.4 结构体初始化

```c
/* 零初始化: = {0}，一个空格 */
GPIO_InitTypeDef gpio = {0};

/* C99 指定初始化器: .member = value, 逗号后换行 */
LED_CFG_S cfg = {
    .GPIOx    = GPIOC,
    .GPIO_Pin = GPIO_PIN_13,
    .polarity = LED_ACTIVE_LOW,
};

KEY_CFG_S cfg = {
    .id            = 0,
    .port          = GPIOB,
    .pin           = GPIO_PIN_12,
    .active_level  = KEY_ACTIVE_LOW,
};
```

- **必须**使用 `.member = value` 的 C99 指定初始化器语法
- `=` 左右各一个空格
- 成员名和 `=` 可以不对齐，但对齐更美观

### 7.5 空格规则

```c
/* 二元运算符: 前后各一个空格 */
a = b + c;
x = (y << 8) | (z >> 8);

/* 一元运算符: 不空格 */
!flag;
i++;
*p++;

/* 逗号后: 一个空格 */
func(a, b, c);

/* 指针声明: 星号靠变量名 */
GPIO_TypeDef *port;
int32_t *out_l, *out_r;

/* sizeof: 当作函数用 */
sizeof(buf)
sizeof(oled->buffer.flat)

/* 括号内: 不空格 */
if (x > 0)

/* 分号后: 跟注释时空两格以上 */
key->state = KEY_STATE_PRESSED;  /* 注释 */
```

### 7.6 三元运算符

```c
/* 简短: 同行 */
return (raw == GPIO_PIN_SET);

/* 中等长度: 同行，括号明确优先级 */
dst_l[i] = (val & 0x800000U) ? (int32_t)(val | 0xFF000000U) : (int32_t)val;

/* 长/有赋值: 改写为 if-else，不要硬写三元 */
uint8_t threshold = key->config.filter_time_ms / 10;
if (threshold == 0) threshold = 1;
```

### 7.7 整数后缀

```c
/* 与 uint8_t/uint16_t/uint32_t 比较时，常量加 U */
if (mic->ready_count >= 2U) { ... }
if (mic->ready_count == 0U) return 0;
mic->write_idx ^= 1U;

/* 移位运算、地址运算的常量加 U */
uint32_t idx = i << 2U;
(1UL << 25)   /* unsigned long 的位操作 */
0x800000U     /* 24-bit 符号位检测 */
```

---

## 8. 代码分段与分隔符

### 8.1 分隔符格式

```c
/* ======================== 段名 ======================== */
```

- `=` 号固定为 24 个（两端各一个空格）
- 段名使用**中文**
- 段与段之间空两行

### 8.2 .c 文件的推荐分段 (从上到下)

```c
/* ======================== 内部数据 ======================== */
/* ======================== 内部辅助 ======================== */
/* ======================== 核心功能 ======================== */
/* ======================== 数据获取 (零拷贝) ======================== */
/* ======================== 接收操作 ======================== */
/* ======================== 发送操作 ======================== */
/* ======================== 事件获取 ======================== */
/* ======================== 显示功能 ======================== */
/* ======================== 高级功能 ======================== */
/* ======================== 公共 API ======================== */
/* ======================== 构造函数 ======================== */
```

- 小型模块只需要 2~3 个分段（如 `内部辅助` + `核心功能` + `构造函数`）
- 复杂模块按功能拆分子分段
- 构造函数**必须是最后一个分段**

---

## 9. 变量与类型使用惯例

### 9.1 整数类型选择

```c
/* 外设寄存器、缓冲区索引: 用 stdint.h 定宽类型 */
uint32_t addr;
uint16_t len;
uint8_t  flag;

/* 循环变量/临时变量: int 就够了 */
int i, j;

/* HAL 库类型: 照用 */
GPIO_PinState raw;
HAL_StatusTypeDef status;
```

### 9.2 volatile 的使用原则

```c
/* volatile — ISR 会修改，主循环会读取 */
volatile uint8_t  write_idx;
volatile uint8_t  ready_count;
volatile uint16_t head;

/* 不加 volatile — 仅 ISR 访问或仅主循环访问 */
uint8_t  active_channels;   /* 构造函数设一次，之后只读 */
uint16_t filter_time_ms;    /* 配置后不变 */
```

### 9.3 数组定义

```c
/* 定长缓冲 — 用宏定义尺寸 */
uint32_t dma_buf[INMP441_DMA_BUF_TOTAL];

/* 多维数组 — 用注释标注重含义 */
int32_t out_buf[2][2][512];  /* [ping/pong][L/R][samples] */

/* union 提供多种访问方式 */
union {
    uint8_t page[8][128];    /* 按页访问: page[y/8][x] */
    uint8_t flat[8 * 128];   /* 线性访问: flat[y/8 * 128 + x] */
} buffer;
```

### 9.4 指针空值检查

```c
/* 公开入口: 必须检查 */
if (buf == NULL || len == 0) return 0;
if (mic->cfg.hi2s == NULL) return;

/* 内部 static 函数: 可省略，假设构造时已保证 */
static void internal_func(MODULE_S *obj)
{
    /* 直接用 obj->xxx，不检查 obj */
}
```

### 9.5 memset / memcpy 惯用

```c
/* 清零缓冲 */
memset(handle->cfg.rx_buf, 0, handle->cfg.rx_buf_size);
memset(oled->buffer.flat, 0, sizeof(oled->buffer.flat));

/* 拷贝数据 */
memcpy(buf, &handle->cfg.rx_buf[tail], chunk1);
memcpy(handle->cfg.tx_buf, data, len);
```

---

## 10. 中断安全与临界区

### 10.1 保护共享数据的固定模式

```c
/* 读 head/tail — 关中断保护 */
static uint16_t ring_available(USART_RING_S *handle)
{
    uint16_t avail;
    __disable_irq();
    avail = ring_used_unlocked(handle);
    __enable_irq();
    return avail;
}

/* 读写环形缓冲 — 整个复制过程在临界区内 */
static uint16_t ring_read(USART_RING_S *handle, uint8_t *buf, uint16_t len)
{
    uint16_t read_len;
    __disable_irq();
    /* 计算可用量 */
    /* 复制数据 */
    /* 更新 tail */
    __enable_irq();
    return read_len;
}

/* FIFO 出队 — 关中断保护 */
static KEY_EVENT_T key_get_event(KEY_S *key)
{
    KEY_EVENT_T event = KEY_EVENT_NONE;
    __disable_irq();
    fifo_pop(key, &event);
    __enable_irq();
    return event;
}
```

- 用 `__disable_irq()` / `__enable_irq()`，不用 `__set_PRIMASK()`
- 临界区越短越好，不放耗时操作
- 不要在临界区内调 HAL 函数

### 10.2 ISR 回调的写法

```c
/* DMA 回调 — 参数必须是 DMA_HandleTypeDef * (HAL 约定) */
static void module_dma_ht_cplt(DMA_HandleTypeDef *hdma)
{
    if (g_module != NULL) module_process_half(g_module, g_module->dma_buf);
}

static void module_dma_tc_cplt(DMA_HandleTypeDef *hdma)
{
    if (g_module != NULL) module_process_half(g_module, &g_module->dma_buf[HALF_SIZE]);
}
```

- 回调通过全局单例指针 `g_module` 访问对象
- 必须做 NULL 检查
- ISR 内只做数据搬运，最快的路径

### 10.3 无锁 ping-pong 模式

```c
/* ISR (生产者): 只写 write_idx 和 ready_count */
mic->ready_count++;
mic->write_idx ^= 1U;

/* 主循环 (消费者): 只写 read_idx 和 ready_count */
if (mic->ready_count == 0U) return 0;
/* ... 使用 out_buf[read_idx] ... */
mic->ready_count--;
mic->read_idx ^= 1U;
```

- 生产和消费操作不同的变量（write_idx vs read_idx），无竞态
- ready_count 用于溢出检测（>=2 表示主循环落后了至少 2 个半区）
- `^= 1U` 在 0/1 间切换

---

## 11. GPIO / I2C / SPI / UART 初始化惯用写法

### 11.1 GPIO 时钟使能 (if-else 链)

```c
/* 标准模式: 根据 GPIO 基地址判断并开启对应时钟 */
if (obj->cfg.port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
else if (obj->cfg.port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
else if (obj->cfg.port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
else if (obj->cfg.port == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
else if (obj->cfg.port == GPIOE) __HAL_RCC_GPIOE_CLK_ENABLE();
```

### 11.2 GPIO 初始化 (HAL 方式)

```c
/* 输入模式 (按键) */
GPIO_InitTypeDef gpio = {0};
gpio.Mode  = GPIO_MODE_INPUT;
gpio.Pin   = obj->config.pin;
gpio.Pull  = (obj->config.active_level == KEY_ACTIVE_LOW)
             ? GPIO_PULLUP : GPIO_PULLDOWN;
gpio.Speed = GPIO_SPEED_FREQ_LOW;
HAL_GPIO_Init(obj->config.port, &gpio);

/* 开漏输出模式 (LED) */
GPIO_InitTypeDef GPIO_InitStruct = {0};
GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
GPIO_InitStruct.Pin   = led_s->led_cfg.GPIO_Pin;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
GPIO_InitStruct.Pull  = (led_s->led_cfg.polarity == LED_ACTIVE_HIGH)
                        ? GPIO_PULLDOWN : GPIO_PULLUP;
HAL_GPIO_Init(led_s->led_cfg.GPIOx, &GPIO_InitStruct);
```

### 11.3 I2C 写命令/数据 (OLED 风格)

```c
/* 写一个字节命令或数据 */
static void i2c_write_byte(OLED_S *oled, uint8_t type, uint8_t data)
{
    HAL_I2C_Mem_Write(oled->cfg.hi2c, oled->cfg.dev_addr,
                      type, I2C_MEMADD_SIZE_8BIT, &data, 1, 10);
}

/* 0x00 = 命令, 0x40 = 数据 */
static void write_cmd(OLED_S *oled, uint8_t cmd)
{
    i2c_write_byte(oled, 0x00, cmd);
}

/* 发送一整页 128 字节 (轮询 I2C) */
HAL_I2C_Mem_Write(oled->cfg.hi2c, oled->cfg.dev_addr,
                  0x40, I2C_MEMADD_SIZE_8BIT,
                  oled->buffer.page[page], OLED_WIDTH, HAL_MAX_DELAY);

/* 发送一整页 128 字节 (DMA I2C) */
HAL_I2C_Mem_Write_DMA(oled->cfg.hi2c, oled->cfg.dev_addr,
                      0x40, I2C_MEMADD_SIZE_8BIT,
                      oled->buffer.page[page], OLED_WIDTH);
```

### 11.4 UART DMA 接收 + IDLE 中断

```c
/* 启动接收 — 标准两步 */
__HAL_UART_ENABLE_IT(handle->cfg.huart, UART_IT_IDLE);
HAL_UART_Receive_DMA(handle->cfg.huart, handle->cfg.rx_buf,
                     handle->cfg.rx_buf_size);

/* 停止接收 */
HAL_UART_DMAStop(handle->cfg.huart);
__HAL_UART_DISABLE_IT(handle->cfg.huart, UART_IT_IDLE);

/* IDLE 中断处理 (在 USARTx_IRQHandler 中调用) */
static void irq_handler(USART_RING_S *handle)
{
    if (__HAL_UART_GET_FLAG(handle->cfg.huart, UART_FLAG_IDLE) != RESET)
    {
        __HAL_UART_CLEAR_IDLEFLAG(handle->cfg.huart);
        uint16_t received = handle->cfg.rx_buf_size
                          - __HAL_DMA_GET_COUNTER(handle->cfg.huart->hdmarx);
        handle->head = received % handle->cfg.rx_buf_size;
    }
}
```

### 11.5 DMA 发送检查 (发送前必做)

```c
/* 发送前必须检查 DMA 是否空闲 */
if (HAL_DMA_GetState(handle->cfg.huart->hdmatx) != HAL_DMA_STATE_READY)
    return USART_RING_BUSY;

HAL_UART_Transmit_DMA(handle->cfg.huart, handle->cfg.tx_buf, len);
```

---

## 12. 常见设计模式速查

### 12.1 状态机模式 (按键消抖)

```c
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
                key->state = KEY_STATE_PRESSED;
                key->press_ticks = 0;
                fifo_push(key, KEY_EVENT_DOWN);
            }
        }
        else { key->filter_count = 0; }
        break;

    case KEY_STATE_PRESSED:
        /* 按下的处理... */
        break;

    /* ... 更多状态 ... */

    default:
        break;
    }
}
```

### 12.2 FIFO 事件缓冲 (无锁)

```c
/* 入队 (仅 ISR/scan 中调用，单生产者无需锁) */
static uint8_t fifo_push(KEY_S *key, KEY_EVENT_T event)
{
    if (key->fifo.count >= KEY_FIFO_SIZE)
        return 0;

    key->fifo.buf[key->fifo.tail] = event;
    key->fifo.tail = (key->fifo.tail + 1) % KEY_FIFO_SIZE;
    key->fifo.count++;
    return 1;
}

/* 出队 (主循环调用，需关中断保护) */
static uint8_t fifo_pop(KEY_S *key, KEY_EVENT_T *event)
{
    if (key->fifo.count == 0)
        return 0;

    *event = key->fifo.buf[key->fifo.head];
    key->fifo.head = (key->fifo.head + 1) % KEY_FIFO_SIZE;
    key->fifo.count--;
    return 1;
}
```

### 12.3 环形缓冲区 (关中断保护)

```c
/* 内部计算 — 假设已关中断 */
static inline uint16_t ring_used_unlocked(USART_RING_S *handle)
{
    if (handle->head >= handle->tail)
        return handle->head - handle->tail;
    else
        return handle->cfg.rx_buf_size - handle->tail + handle->head;
}

/* 公开接口 — 进临界区调用 */
static uint16_t ring_available(USART_RING_S *handle)
{
    uint16_t avail;
    __disable_irq();
    avail = ring_used_unlocked(handle);
    __enable_irq();
    return avail;
}
```

### 12.4 DMA 双缓冲 + 零拷贝 (Ping-Pong)

```c
/* 消费端 */
static uint8_t try_get_block(MODULE_S *dev, int32_t **out)
{
    if (dev->ready_count == 0U) return 0;  /* 无新数据 */

    *out = dev->out_buf[dev->read_idx];     /* 指针传递，不拷贝 */
    dev->ready_count--;
    dev->read_idx ^= 1U;
    return 1;
}

/* 生产端 (ISR 中) */
static void process_half(MODULE_S *dev, const uint32_t *src)
{
    if (dev->ready_count >= 2U)             /* 溢出保护 */
    {
        dev->overflow++;
        return;
    }

    int32_t *dst = dev->out_buf[dev->write_idx];
    /* 解析数据写入 dst... */

    dev->ready_count++;
    dev->write_idx ^= 1U;
}
```

### 12.5 脏页跟踪 + 局部刷新 (OLED)

```c
/* 标记 — 每个修改像素/字符/图形的操作都要调用 */
static inline void mark_dirty(OLED_S *oled, uint8_t page)
{
    oled->dirty_pages |= (uint8_t)(1 << page);
}

/* 刷新 — 只发送修改过的页 */
static void refresh(OLED_S *oled)
{
    uint8_t dirty = oled->dirty_pages;
    for (uint8_t page = 0; page < OLED_PAGES; page++)
    {
        if (dirty & (1 << page))
        {
            send_page_blocking(oled, page);  /* 仅 1 次 I2C 事务 */
        }
    }
    oled->dirty_pages = 0;
}
```

### 12.6 可变参数格式化发送

```c
static int ring_send(USART_RING_S *handle, const char *format, ...)
{
    if (HAL_DMA_GetState(handle->cfg.huart->hdmatx) != HAL_DMA_STATE_READY)
        return USART_RING_BUSY;

    uint16_t len;
    va_list args;
    va_start(args, format);
    len = vsnprintf((char *)handle->cfg.tx_buf, handle->cfg.tx_buf_size,
                    format, args);
    va_end(args);

    if (len >= handle->cfg.tx_buf_size)
        len = handle->cfg.tx_buf_size - 1;

    HAL_UART_Transmit_DMA(handle->cfg.huart, handle->cfg.tx_buf, len);
    return USART_RING_OK;
}
```

### 12.7 CMSIS-DSP 科学计算 (非设备驱动可简化)

不驱动外设的纯算法模块可以不用 OOP 模式，使用传统函数 API：

```c
/* 头文件: 声明公开 API，不用对象包装 */
void INMP441_FFT_Create(INMP441_FFT_S *fft, uint16_t sample_rate);
void INMP441_FFT_Process(INMP441_FFT_S *fft, const int32_t *samples);
uint16_t INMP441_FFT_FindPeak(const INMP441_FFT_S *fft,
                               uint16_t min_bin, uint16_t max_bin);
float INMP441_FFT_BinToFreq(const INMP441_FFT_S *fft, uint16_t bin);
```

即使不用 OOP 模式，命名仍然遵循: `模块大写_动词名词` (PascalCase 前缀)，注释仍然遵循 Doxygen 格式。

---

## 13. 完整模板 (可直接复制)

### 13.1 头文件模板

```c
/**
 * @file    template.h
 * @brief   模板外设驱动 — 面向对象封装
 * @note    硬件依赖: 需要 CubeMX 完成 SPI/I2C 初始化。
 *          数据流: <简要描述>
 *
 * @example 使用示例:
 *   TEMPLATE_S dev;
 *   TEMPLATE_CFG_S cfg = {
 *       .hspi = &hspi1,
 *       .cs_port = GPIOA,
 *       .cs_pin = GPIO_PIN_4,
 *   };
 *   TEMPLATE_Create(&dev, &cfg);
 *   dev.do_something(&dev, 42);
 */

#ifndef _TEMPLATE_H_
#define _TEMPLATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ======================== 常量定义 ======================== */

/** 内部缓冲区大小 */
#define TEMPLATE_BUFFER_SIZE        256

/** 默认超时 (ms) */
#define TEMPLATE_DEFAULT_TIMEOUT_MS 1000

/* ======================== 类型定义 ======================== */

/** 工作模式 */
typedef enum
{
    TEMPLATE_MODE_SLOW = 0,     /**< 慢速模式 */
    TEMPLATE_MODE_FAST,         /**< 快速模式 */
} TEMPLATE_MODE_T;

/** 硬件配置 */
typedef struct
{
    SPI_HandleTypeDef *hspi;            /**< SPI 句柄 (已初始化) */
    GPIO_TypeDef      *cs_port;         /**< 片选 GPIO 端口 */
    uint16_t           cs_pin;          /**< 片选 GPIO 引脚 */
    TEMPLATE_MODE_T    mode;            /**< 工作模式 */
    uint16_t           timeout_ms;      /**< 超时 (ms), 0 = 使用默认值 */
} TEMPLATE_CFG_S;

/* 前向声明 */
typedef struct TEMPLATE_S TEMPLATE_S;

/** 函数指针类型定义 */
typedef void     (*TEMPLATE_INIT_FUNC)(TEMPLATE_S *dev);
typedef uint8_t  (*TEMPLATE_DO_SOMETHING_FUNC)(TEMPLATE_S *dev, int arg);

/** 外设对象 */
struct TEMPLATE_S
{
    TEMPLATE_CFG_S cfg;                         /**< 硬件配置 */

    /* ---- 内部状态 (用户不应直接访问) ---- */
    volatile uint8_t busy;                      /**< 忙标志 (ISR 更新) */
    uint8_t           buffer[TEMPLATE_BUFFER_SIZE]; /**< 内部缓冲区 */

    /* ---- 成员函数 ---- */
    TEMPLATE_INIT_FUNC            init;         /**< 初始化 */
    TEMPLATE_DO_SOMETHING_FUNC    do_something; /**< 执行操作 */
};

/* ======================== 构造函数 ======================== */
void TEMPLATE_Create(TEMPLATE_S *dev, TEMPLATE_CFG_S *config);

#ifdef __cplusplus
}
#endif

#endif /* _TEMPLATE_H_ */
```

### 13.2 源文件模板

```c
/**
 * @file    template.c
 * @brief   模板外设驱动实现
 * @note    面向对象风格: 所有函数通过 TEMPLATE_S 对象调用
 */

#include "template.h"
#include <string.h>

/* ======================== 内部数据 ======================== */

/** 全局实例指针 (DMA/中断回调需要访问对象) */
static TEMPLATE_S *g_dev = NULL;

/* ======================== 内部辅助 ======================== */

/**
 * @brief  内部辅助函数
 * @param  dev: 设备对象指针
 */
static void internal_helper(TEMPLATE_S *dev)
{
    /* ... */
}

/* ======================== 核心功能 ======================== */

/**
 * @brief  初始化设备硬件和内部状态
 * @param  dev: 设备对象指针
 */
static void template_init(TEMPLATE_S *dev)
{
    g_dev = dev;
    dev->busy = 0;
    memset(dev->buffer, 0, sizeof(dev->buffer));

    /* GPIO 时钟使能 */
    if (dev->cfg.cs_port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (dev->cfg.cs_port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (dev->cfg.cs_port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
    else if (dev->cfg.cs_port == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
    else if (dev->cfg.cs_port == GPIOE) __HAL_RCC_GPIOE_CLK_ENABLE();

    /* 配置 CS 引脚为推挽输出 */
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pin   = dev->cfg.cs_pin;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(dev->cfg.cs_port, &gpio);

    /* CS 初始为高 (不选中) */
    HAL_GPIO_WritePin(dev->cfg.cs_port, dev->cfg.cs_pin, GPIO_PIN_SET);
}

/**
 * @brief  执行操作
 * @param  dev: 设备对象指针
 * @param  arg: 操作参数
 * @return 0 = 失败, 1 = 成功
 */
static uint8_t template_do_something(TEMPLATE_S *dev, int arg)
{
    if (dev->busy) return 0;
    if (dev->cfg.hspi == NULL) return 0;

    /* 选中设备 */
    HAL_GPIO_WritePin(dev->cfg.cs_port, dev->cfg.cs_pin, GPIO_PIN_RESET);

    /* 通过 SPI 收发数据... */
    /* ... */

    /* 释放设备 */
    HAL_GPIO_WritePin(dev->cfg.cs_port, dev->cfg.cs_pin, GPIO_PIN_SET);

    return 1;
}

/* ======================== 构造函数 ======================== */

/**
 * @brief  外设对象构造函数
 * @param  dev:    设备对象指针 (调用者分配内存)
 * @param  config: 硬件配置指针 (调用者分配内存)
 * @note   调用后外设即初始化完毕，可直接使用成员函数。
 *
 * @example 使用示例:
 *   TEMPLATE_S dev;
 *   TEMPLATE_CFG_S cfg = {
 *       .hspi   = &hspi1,
 *       .cs_port = GPIOA,
 *       .cs_pin  = GPIO_PIN_4,
 *       .mode    = TEMPLATE_MODE_FAST,
 *   };
 *   TEMPLATE_Create(&dev, &cfg);
 *
 *   if (dev.do_something(&dev, 42)) {
 *       // 操作成功
 *   }
 */
void TEMPLATE_Create(TEMPLATE_S *dev, TEMPLATE_CFG_S *config)
{
    /* 绑定配置 */
    dev->cfg = *config;

    /* 设置默认值 */
    if (dev->cfg.timeout_ms == 0)
        dev->cfg.timeout_ms = TEMPLATE_DEFAULT_TIMEOUT_MS;

    /* 绑定函数指针 */
    dev->init         = template_init;
    dev->do_something = template_do_something;

    /* 执行硬件初始化 */
    dev->init(dev);
}
```

---

## 14. 禁止事项

以下做法在现有驱动中**从未出现**，新驱动也不应使用:

- ❌ 使用 `malloc` / `free` 动态分配 — 所有缓冲区由调用者静态分配或嵌入对象结构体
- ❌ 使用 Tab 缩进 — 永远是 4 个空格
- ❌ 函数体不写注释 — 每个函数都应有 Doxygen 注释
- ❌ 在头文件中实现函数（`static inline` 除外）— 实现一律在 .c
- ❌ 使用匈牙利命名法 (`iVar`, `pData`) — 用 snake_case
- ❌ 单字母全局变量 — 全局变量必须有 `g_` 前缀和有意义的名字
- ❌ 在 ISR 中调用 `printf` / HAL 阻塞函数
- ❌ 在临界区内调用 HAL 函数或做耗时计算
- ❌ 头文件包含不需要的 .h — 如 bsp.h 中不要包含 `<stdlib.h>` 如果只用到了 uint32_t
- ❌ 中文出现在代码逻辑中（变量名、函数名）— 中文仅用于注释
- ❌ 使用 `#pragma once` 代替 include guard — 统一用 `#ifndef _NAME_H_`
