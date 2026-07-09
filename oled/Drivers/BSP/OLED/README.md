# SSD1306 OLED I2C 驱动

面向对象风格的 SSD1306 OLED 驱动，支持脏页跟踪、局部刷新和 DMA 传输。

## 硬件连接

| OLED 引脚 | STM32 引脚 | 说明 |
|-----------|-----------|------|
| VCC | 3.3V | 电源 |
| GND | GND | 地 |
| SCL | PB8 | I2C1 时钟 |
| SDA | PB9 | I2C1 数据 |

## 快速开始

```c
#include "oled.h"

int main(void)
{
    // 系统初始化 (由 STM32CubeMX 生成)
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_I2C1_Init();

    // 创建 OLED 对象
    OLED_S oled;
    OLED_CFG_S cfg = {
        .hi2c      = &hi2c1,
        .dev_addr  = OLED_ADDR_78   // 默认地址, 可选 OLED_ADDR_7A
    };
    OLED_Create(&oled, &cfg);

    // 显示内容
    oled.show_str(&oled, 0, 0, "Hello World!", 1);  // 6×8 字体
    oled.show_num(&oled, 0, 16, 12345, 2);           // 8×16 字体

    // 几何绘制
    oled.draw_hline(&oled, 10, 50, 30);              // 水平线
    oled.draw_line(&oled, 10, 40, 60, 10);           // 任意直线
    oled.draw_rect(&oled, 20, 20, 30, 15, 0);        // 空心矩形
    oled.draw_circle(&oled, 90, 32, 12, 1);          // 实心圆

    // 中文显示 (UTF-8 编码)
    oled.show_cn(&oled, 0, 40, "嵌入式学习");         // 16×16 汉字
    oled.refresh(&oled);  // 刷新到屏幕

    while (1);
}
```

## API 参考

### 构造函数

```c
void OLED_Create(OLED_S *oled, OLED_CFG_S *config);
```

创建并初始化 OLED 对象。调用后屏幕已开启且清屏。

**配置结构体：**

```c
typedef struct {
    I2C_HandleTypeDef *hi2c;   // I2C 句柄指针 (如 &hi2c1)
    uint8_t dev_addr;          // 设备地址: OLED_ADDR_78 或 OLED_ADDR_7A
} OLED_CFG_S;
```

### 核心功能 (oled.xxx)

| 成员函数 | 说明 |
|----------|------|
| `oled.init(&oled)` | 硬件初始化 (Create 已调用) |
| `oled.on(&oled)` | 唤醒显示屏 |
| `oled.off(&oled)` | 休眠显示屏 |
| `oled.clear(&oled)` | 清屏并刷新 |
| `oled.refresh(&oled)` | 局部刷新 — 仅发送修改过的页 (轮询 I2C) |
| `oled.refresh_dma(&oled)` | DMA 刷新 — 仅发送修改过的页 (DMA 传输) |
| `oled.set_pixel(&oled, x, y, 1)` | 设置像素点 (仅改缓冲区, 不刷新) |

### 显示功能

| 成员函数 | 说明 |
|----------|------|
| `oled.show_str(&oled, x, y, "str", size)` | 显示字符串, size: 1=6×8, 2=8×16 |
| `oled.show_num(&oled, x, y, num, size)` | 显示数字 (int32_t) |
| `oled.show_bmp(&oled, x, y, w, h, bmp)` | 显示位图 (自动调用 refresh) |
| `oled.fill_rect(&oled, x, y, w, h, on)` | 清除/填充矩形 (自动刷新, on=0 局部清屏) |
| `oled.show_cn(&oled, x, y, "中文")` | 显示 UTF-8 中文字符串 (16×16 字体) |

### 中文显示 (oled.show_cn)

`show_cn` 用于显示 UTF-8 编码的 16×16 中文字符。字库定义在 `oled_font.h` 的 `F16x16_CN[]` 数组中。

**工作原理：**

```
输入 "嵌入式学习"
      │
      ▼
逐字节扫描, 识别 0xE0~0xEF 开头的 3 字节 UTF-8 序列
      │
      ▼
在 F16x16_CN[] 中匹配 index[3] → 找到则绘制 16×16 点阵
      │
      ▼
自动换行 (cur_x + 16 > 128 → 换行)
```

**添加新汉字：**

向 `oled_font.h` 的 `F16x16_CN[]` 数组中添加条目：

```c
{"字",  // UTF-8 编码的汉字本身 (自动转为 3 字节 index)
  0x00,0x00,...  // 上半 16 字节 (列行式取模)
  0x00,0x00,...  // 下半 16 字节
},
```

> 取模设置：共阴、列行式、逆向输出，字号 16×16。

### 几何绘制 (oled.draw_xxx)

所有 `draw_` 方法**仅修改缓冲区，不自动刷新**，由调用者统一控制刷新时机。

| 成员函数 | 说明 |
|----------|------|
| `oled.draw_hline(&oled, x0, x1, y)` | 水平线 (自动排序 x0/x1, 直接写缓冲区) |
| `oled.draw_vline(&oled, x, y0, y1)` | 垂直线 (自动排序 y0/y1, 直接写缓冲区) |
| `oled.draw_line(&oled, x0, y0, x1, y1)` | 任意直线 (Bresenham 算法, 纯整数运算) |
| `oled.draw_rect(&oled, x, y, w, h, filled)` | 矩形, filled=0 空心 / filled=1 实心 |
| `oled.draw_circle(&oled, cx, cy, r, filled)` | 圆, filled=0 空心 / filled=1 实心 (Midpoint 算法) |

**示例：**

```c
// 空心矩形 + 实心圆
oled.draw_rect(&oled, 10, 10, 50, 30, 0);   // filled=0 → 空心
oled.draw_circle(&oled, 64, 32, 20, 1);      // filled=1 → 实心
oled.refresh(&oled);  // 一次性刷新所有修改
```

### 高级功能 (直接调用)

```c
void OLED_SetContrast(&oled, 0x7F);       // 对比度 0x00~0xFF
void OLED_SetDisplayMode(&oled, OLED_DISPLAY_INVERSE);  // 反色显示
void OLED_ScrollHorizontal(&oled, 0, 7, OLED_SCROLL_LEFT);  // 水平滚动
void OLED_ShiftVertical(&oled, 10);       // 垂直偏移 0~63
```

## 脏页跟踪

驱动内部维护一个 `dirty_pages` 位掩码 (uint8_t)，每位对应屏幕的一页 (屏幕共 8 页, 每页 8 行)：

```
bit 7  bit 6  bit 5  ...  bit 0
  │      │      │            │
 page7  page6  page5       page0
```

**工作流程：**

```
show_str / show_num / show_cn / set_pixel / show_bmp
draw_line / draw_hline / draw_vline / draw_rect / draw_circle
        │
        ▼
  修改 buffer[][] + 标记对应 dirty_pages 位
        │
        ▼
  refresh() / refresh_dma()
        │
        ▼
  只发送 dirty 页 → 清除 dirty_pages
```

**效果：** 只修改了一行文字的场景下，最多传输 2 页 (256 字节)，而非全部 8 页 (1024 字节)，I2C 通信量减少 75%。

## refresh vs refresh_dma

| | refresh() | refresh_dma() |
|------|-----------|-------------|
| 传输方式 | 轮询 I2C 突发写入 | DMA 硬件搬运 |
| CPU 占用 | 传输期间阻塞 | 传输期间 CPU 可处理中断 |
| I2C 事务数 | 1 次/页 | 1 次/页 |
| 依赖 | 无 | 需 I2C 中断处理函数 |

**推荐用法：**
- **简单场景** → `refresh()` — 无需额外配置
- **大量数据 / 频繁刷新** → `refresh_dma()` — CPU 开销更低

## I2C 中断配置 (DMA 刷新必需)

使用 `refresh_dma()` 前，需在 `stm32f1xx_it.c` 中添加 I2C 中断处理函数：

```c
extern I2C_HandleTypeDef hi2c1;

void I2C1_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&hi2c1);
}

void I2C1_ER_IRQHandler(void)
{
    HAL_I2C_ER_IRQHandler(&hi2c1);
}
```

> CubeMX 在配置 I2C 时如果未启用中断，这些 handler 默认指向死循环的 `Default_Handler`，需要手动覆盖。

## 移植到其他平台

1. **更换 MCU**: 替换 `#include "main.h"` 为对应平台的 HAL 头文件
2. **更换 I2C 总线**: 修改 `cfg.hi2c = &hi2c2`
3. **更换设备地址**: 修改 `cfg.dev_addr = OLED_ADDR_7A`
4. **更换屏幕尺寸**: 修改 `OLED_WIDTH` / `OLED_HEIGHT` 宏
5. **更换字体**: 替换 `oled_font.h` 中的 `F6x8` / `F8X16` 数组
6. **添加汉字**: 向 `oled_font.h` 的 `F16x16_CN[]` 中添加新条目

## 文件结构

```
Drivers/BSP/OLED/
├── oled.h          # 接口定义 (类型、结构体、函数声明)
├── oled.c          # 驱动实现
├── oled_font.h     # 字体数据 (6×8, 8×16, 16×16 中文, BMP 素材)
└── README.md       # 本文件
```
