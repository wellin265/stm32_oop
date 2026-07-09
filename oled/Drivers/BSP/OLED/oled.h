/**
 * @file    oled.h
 * @brief   SSD1306 OLED I2C 驱动 — 面向对象封装
 * @note    使用方式参考 LED 驱动风格，通过 OLED_Create() 构造对象后调用成员函数
 *
 * @example 使用示例:
 *   OLED_S oled;
 *   OLED_CFG_S cfg = { .hi2c = &hi2c1, .dev_addr = OLED_ADDR_78 };
 *   OLED_Create(&oled, &cfg);
 *   oled.show_str(&oled, 0, 0, "Hello", 1);
 *   oled.refresh(&oled);
 */

#ifndef _OLED_H_
#define _OLED_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* ======================== 常量定义 ======================== */

/* OLED I2C 设备地址 (7位地址左移1位后的值) */
#define OLED_ADDR_78    0x78
#define OLED_ADDR_7A    0x7A

/* 屏幕硬件参数 */
#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define OLED_PAGES      (OLED_HEIGHT / 8)

/* 滚动方向 */
#define OLED_SCROLL_LEFT    0x27
#define OLED_SCROLL_RIGHT   0x26

/* 显示模式 */
#define OLED_DISPLAY_NORMAL    0xA6
#define OLED_DISPLAY_INVERSE   0xA7

/* ======================== 类型定义 ======================== */

/** OLED 配置结构体 */
typedef struct
{
    I2C_HandleTypeDef *hi2c;    /**< I2C 句柄指针，解耦具体 I2C 外设 */
    uint8_t dev_addr;           /**< OLED 设备地址 (0x78 或 0x7A) */
} OLED_CFG_S;

/* 前向声明 */
typedef struct OLED_S OLED_S;

/** 函数指针类型定义 */
typedef void (*OLED_INIT_FUNC)(OLED_S *oled);
typedef void (*OLED_ON_FUNC)(OLED_S *oled);
typedef void (*OLED_OFF_FUNC)(OLED_S *oled);
typedef void (*OLED_CLEAR_FUNC)(OLED_S *oled);
typedef void (*OLED_CLEAR_PAGE_FUNC)(OLED_S *oled, uint8_t page);
typedef void (*OLED_REFRESH_FUNC)(OLED_S *oled);
typedef void (*OLED_REFRESH_DMA_FUNC)(OLED_S *oled);
typedef void (*OLED_SET_PIXEL_FUNC)(OLED_S *oled, int16_t x, int16_t y, uint8_t on);
typedef void (*OLED_SHOW_STR_FUNC)(OLED_S *oled, int16_t x, int16_t y, char *str, uint8_t size);
typedef void (*OLED_SHOW_NUM_FUNC)(OLED_S *oled, int16_t x, int16_t y, int32_t num, uint8_t size);
typedef void (*OLED_SHOW_BMP_FUNC)(OLED_S *oled, int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t *bmp);
typedef void (*OLED_FILL_RECT_FUNC)(OLED_S *oled, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t on);
typedef void (*OLED_DRAW_LINE_FUNC)(OLED_S *oled, int16_t x0, int16_t y0, int16_t x1, int16_t y1);
typedef void (*OLED_DRAW_HLINE_FUNC)(OLED_S *oled, int16_t x0, int16_t x1, int16_t y);
typedef void (*OLED_DRAW_VLINE_FUNC)(OLED_S *oled, int16_t x, int16_t y0, int16_t y1);
typedef void (*OLED_DRAW_RECT_FUNC)(OLED_S *oled, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t filled);
typedef void (*OLED_DRAW_CIRCLE_FUNC)(OLED_S *oled, int16_t cx, int16_t cy, int16_t r, uint8_t filled);
typedef void (*OLED_SHOW_CN_FUNC)(OLED_S *oled, int16_t x, int16_t y, char *str);

/** OLED 对象结构体 — 面向对象风格，通过成员函数操作 */
struct OLED_S
{
    OLED_CFG_S cfg;                             /**< 硬件配置 */
    union {
        uint8_t page[OLED_PAGES][OLED_WIDTH];           /**< 按页访问: buffer.page[p][c] */
        uint8_t flat[OLED_PAGES * OLED_WIDTH];          /**< 线性访问: buffer.flat[i] */
    } buffer;                                           /**< 显存缓冲区 (8页 × 128列) */
    uint8_t dirty_pages;                        /**< 脏页位掩码: bit n = 页 n 已修改 */

    /* ---- 核心功能 ---- */
    OLED_INIT_FUNC          init;        /**< 硬件初始化 */
    OLED_ON_FUNC            on;          /**< 唤醒显示 */
    OLED_OFF_FUNC           off;         /**< 休眠显示 */
    OLED_CLEAR_FUNC         clear;       /**< 清屏 (刷新到硬件) */
    OLED_CLEAR_PAGE_FUNC    clear_page;  /**< 清除指定页 (刷新到硬件) */
    OLED_REFRESH_FUNC       refresh;     /**< 局部刷新 (仅脏页, 轮询I2C) */
    OLED_REFRESH_DMA_FUNC   refresh_dma; /**< DMA 刷新 (仅脏页, 非阻塞DMA) */
    OLED_SET_PIXEL_FUNC     set_pixel;   /**< 设置单个像素点 */

    /* ---- 显示功能 ---- */
    OLED_SHOW_STR_FUNC  show_str;   /**< 显示字符串 (6x8 / 8x16) */
    OLED_SHOW_NUM_FUNC  show_num;   /**< 显示数字 */
    OLED_SHOW_BMP_FUNC  show_bmp;   /**< 显示 BMP 位图 */
    OLED_FILL_RECT_FUNC fill_rect;  /**< 清除/填充矩形 (clear 的补充, on=0 清除区域) */

    /* ---- 几何绘制 ---- */
    OLED_DRAW_LINE_FUNC   draw_line;   /**< 绘制任意直线 (Bresenham) */
    OLED_DRAW_HLINE_FUNC  draw_hline;  /**< 绘制水平线 (快速) */
    OLED_DRAW_VLINE_FUNC  draw_vline;  /**< 绘制垂直线 (快速) */
    OLED_DRAW_RECT_FUNC   draw_rect;   /**< 绘制矩形 (filled=0 空心, filled=1 实心) */
    OLED_DRAW_CIRCLE_FUNC draw_circle; /**< 绘制圆   (filled=0 空心, filled=1 实心) */

    /* ---- 中文字符 ---- */
    OLED_SHOW_CN_FUNC show_cn;     /**< 显示 UTF-8 中文 (16×16 字体) */
};

/* ======================== 构造函数 ======================== */
void OLED_Create(OLED_S *oled, OLED_CFG_S *config);

/* ======================== 高级功能 (直接调用) ======================== */
void OLED_SetContrast(OLED_S *oled, uint8_t contrast);
void OLED_SetDisplayMode(OLED_S *oled, uint8_t mode);
void OLED_ScrollHorizontal(OLED_S *oled, uint8_t start_page,
                           uint8_t end_page, uint8_t direction);
void OLED_ShiftVertical(OLED_S *oled, uint8_t shift);

#ifdef __cplusplus
}
#endif

#endif /* _OLED_H_ */
