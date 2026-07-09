/**
 * @file    oled.c
 * @brief   SSD1306 OLED I2C 驱动实现
 * @note    面向对象风格：所有函数通过 OLED_S 对象调用
 *          支持脏页跟踪 → 局部刷新 → 大幅减少 I2C 通信量
 *          支持 DMA 批量传输 → 整页数据一次发送，释放 CPU
 */

#include "oled.h"
#include "oled_font.h"
#include <string.h>

/* ======================== 内部辅助 ======================== */

/** 标记指定页为脏 */
static inline void mark_dirty(OLED_S *oled, uint8_t page)
{
    oled->dirty_pages |= (1 << page);
}

/** 标记某纵坐标范围对应的页为脏 */
static void mark_dirty_y_range(OLED_S *oled, int16_t y0, int16_t y1)
{
    if (y0 < 0) y0 = 0;
    if (y1 >= OLED_HEIGHT) y1 = OLED_HEIGHT - 1;

    uint8_t start_page = (uint8_t)y0 / 8;
    uint8_t end_page   = (uint8_t)y1 / 8;

    for (uint8_t p = start_page; p <= end_page; p++)
    {
        mark_dirty(oled, p);
    }
}

/**
 * @brief  I2C 写一字节命令或数据 (轮询, 单字节)
 */
static void i2c_write_byte(OLED_S *oled, uint8_t type, uint8_t data)
{
    HAL_I2C_Mem_Write(oled->cfg.hi2c, oled->cfg.dev_addr,
                      type, I2C_MEMADD_SIZE_8BIT, &data, 1, 10);
}

static void write_cmd(OLED_S *oled, uint8_t cmd)
{
    i2c_write_byte(oled, 0x00, cmd);
}

/**
 * @brief  发送一整页数据 (128 字节) — 使用轮询 I2C 突发写入
 * @note   替代原来 128 次独立 write_dat()，1 次 START/STOP 替代 128 次
 */
static void send_page_blocking(OLED_S *oled, uint8_t page)
{
    /* 设置页地址和列地址 */
    write_cmd(oled, 0xB0 + page);
    write_cmd(oled, 0x00);
    write_cmd(oled, 0x10);

    /* 一次性发送整页 128 字节 (1 次 I2C 事务, 轮询标志位) */
    HAL_I2C_Mem_Write(oled->cfg.hi2c, oled->cfg.dev_addr,
                      0x40, I2C_MEMADD_SIZE_8BIT,
                      oled->buffer.page[page], OLED_WIDTH, HAL_MAX_DELAY);
}

/**
 * @brief  发送一整页数据 (128 字节) — 使用 DMA (需 I2C 中断)
 * @note   DMA 负责搬运 128 字节数据，CPU 不参与逐字节传输
 *         时序: [START]→[DEV_ADDR]→[0x40]→[DMA:128B]→[STOP]
 *         每次只发 1 次 I2C 事务，替代原来 128 次独立 write_dat()
 */
static void send_page_dma(OLED_S *oled, uint8_t page)
{
    /* 设置页地址和列地址 (阻塞) */
    write_cmd(oled, 0xB0 + page);
    write_cmd(oled, 0x00);
    write_cmd(oled, 0x10);

    /* DMA 发送整页 128 字节 (1 次 I2C 事务) */
    HAL_I2C_Mem_Write_DMA(oled->cfg.hi2c, oled->cfg.dev_addr,
                          0x40, I2C_MEMADD_SIZE_8BIT,
                          oled->buffer.page[page], OLED_WIDTH);
}

/* ======================== 核心功能 ======================== */

static void oled_init(OLED_S *oled)
{
    write_cmd(oled, 0xAE); /* 关闭显示 */
    write_cmd(oled, 0x20); write_cmd(oled, 0x10); /* 页寻址模式 */
    write_cmd(oled, 0xB0); /* 页地址 0 */
    write_cmd(oled, 0xC8); /* COM 扫描: 从下到上 */
    write_cmd(oled, 0x00); /* 列低地址 */
    write_cmd(oled, 0x10); /* 列高地址 */
    write_cmd(oled, 0x40); /* 起始行 */

    write_cmd(oled, 0x81); write_cmd(oled, 0xFF); /* 对比度最大 */
    write_cmd(oled, 0xA1); /* 段重映射 */
    write_cmd(oled, 0xA6); /* 正常显示 */
    write_cmd(oled, 0xA8); write_cmd(oled, 0x3F); /* 1/64 duty */
    write_cmd(oled, 0xA4); /* 输出跟随 RAM */
    write_cmd(oled, 0xD3); write_cmd(oled, 0x00); /* 不偏移 */
    write_cmd(oled, 0xD5); write_cmd(oled, 0xF0); /* 时钟分频 */
    write_cmd(oled, 0xD9); write_cmd(oled, 0x22); /* 预充电 */
    write_cmd(oled, 0xDA); write_cmd(oled, 0x12); /* COM 引脚 */
    write_cmd(oled, 0xDB); write_cmd(oled, 0x20); /* VCOMH */
    write_cmd(oled, 0x8D); write_cmd(oled, 0x14); /* 电荷泵开 */
    write_cmd(oled, 0xAF); /* 开启显示 */

    oled->clear(oled);
}

static void oled_on(OLED_S *oled)
{
    write_cmd(oled, 0x8D);
    write_cmd(oled, 0x14);
    write_cmd(oled, 0xAF);
}

static void oled_off(OLED_S *oled)
{
    write_cmd(oled, 0x8D);
    write_cmd(oled, 0x10);
    write_cmd(oled, 0xAE);
}

/**
 * @brief  清屏并刷新到硬件 (标记所有页为脏)
 */
static void oled_clear(OLED_S *oled)
{
    memset(oled->buffer.flat, 0, sizeof(oled->buffer.flat));
    oled->dirty_pages = 0xFF; /* 全部脏 */
}

/**
 * @brief  清除指定页并刷新到硬件
 * @param  page: 要清除的页号 (0 ~ OLED_PAGES-1)
 * @note   仅清零该页缓冲区, 标记为脏并立即发送到 OLED
 */
static void oled_clear_page(OLED_S *oled, uint8_t page)
{
    if (page >= OLED_PAGES)
        return;

    /* 清零该页缓冲区 */
    memset(&oled->buffer.flat[page * OLED_WIDTH], 0, OLED_WIDTH);

    /* 标记该页为脏并立即刷新到硬件 */
    oled->dirty_pages = (1 << page);
}

/**
 * @brief  局部刷新 — 仅发送脏页 (轮询 I2C 突发写入)
 * @note   每页只需 1 次 I2C 事务 (128 字节), 而非原来的 128 次
 */
static void oled_refresh(OLED_S *oled)
{
    uint8_t dirty = oled->dirty_pages;

    for (uint8_t page = 0; page < OLED_PAGES; page++)
    {
        if (dirty & (1 << page))
        {
            send_page_blocking(oled, page);
        }
    }

    oled->dirty_pages = 0; /* 清除脏标记 */
}

/**
 * @brief  DMA 刷新 — 仅发送脏页 (DMA + I2C 中断)
 * @note   每页启动 DMA 后等待 I2C 状态回到 READY 再处理下一页
 */
static void oled_refresh_dma(OLED_S *oled)
{
    uint8_t dirty = oled->dirty_pages;

    for (uint8_t page = 0; page < OLED_PAGES; page++)
    {
        if (dirty & (1 << page))
        {
            send_page_dma(oled, page);

            /* 等待当前页 DMA + I2C 传输完成, 否则下一页的 write_cmd 会冲突 */
            while (HAL_I2C_GetState(oled->cfg.hi2c) != HAL_I2C_STATE_READY);
        }
    }

    oled->dirty_pages = 0;
}

/**
 * @brief  设置单个像素点 (仅修改缓冲区, 标记脏页)
 */
static void oled_set_pixel(OLED_S *oled, int16_t x, int16_t y, uint8_t on)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT)
        return;

    uint8_t page = (uint8_t)y / 8;

    if (on)
        oled->buffer.page[page][x] |= (0x01 << (y % 8));
    else
        oled->buffer.page[page][x] &= ~(0x01 << (y % 8));

    mark_dirty(oled, page);
}

/* ======================== 显示功能 ======================== */

/**
 * @brief  显示字符串 (标记受影响页为脏)
 * @param  size: 1=6×8 字体, 2=8×16 字体
 */
static void oled_show_str(OLED_S *oled, int16_t x, int16_t y, char *str, uint8_t size)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT)
        return;

    uint8_t char_width, char_height;
    int16_t start_y = y;

    if (size == 1) { char_width = 6;  char_height = 8;  }
    else           { char_width = 8;  char_height = 16; }

    while (*str != '\0')
    {
        int32_t idx = *str - 32;
        if (idx < 0) { str++; continue; }

        /* 换行检查 */
        if (x + char_width > OLED_WIDTH)
        {
            x = 0;
            y += char_height;
            if (y + char_height > OLED_HEIGHT) break;
        }

        if (size == 1)
        {
            for (uint8_t col = 0; col < 6; col++)
            {
                uint8_t data = F6x8[idx][col];
                for (uint8_t row = 0; row < 8; row++)
                    oled_set_pixel(oled, x + col, y + row, (data >> row) & 0x01);
            }
            x += 6;
        }
        else
        {
            for (uint8_t half = 0; half < 2; half++)
            {
                for (uint8_t col = 0; col < 8; col++)
                {
                    uint8_t data = F8X16[idx][col + half * 8];
                    for (uint8_t bit = 0; bit < 8; bit++)
                        oled_set_pixel(oled, x + col, y + bit + half * 8,
                                       (data >> bit) & 0x01);
                }
            }
            x += 8;
        }
        str++;
    }

    /* 标记从起始 y 到最终 y 的所有页为脏 */
    mark_dirty_y_range(oled, start_y, y + char_height - 1);
}

static void oled_show_num(OLED_S *oled, int16_t x, int16_t y,
                          int32_t num, uint8_t size)
{
    char str[12];
    uint8_t i = 0;
    uint8_t is_neg = 0;

    if (num < 0) { is_neg = 1; num = -num; }

    if (num == 0) { str[i++] = '0'; }
    else
    {
        while (num > 0 && i < sizeof(str) - 2)
        {
            str[i++] = (num % 10) + '0';
            num /= 10;
        }
    }
    if (is_neg) str[i++] = '-';

    for (uint8_t j = 0; j < i / 2; j++)
    {
        char tmp = str[j];
        str[j] = str[i - 1 - j];
        str[i - 1 - j] = tmp;
    }
    str[i] = '\0';

    oled_show_str(oled, x, y, str, size);
}

/**
 * @brief  显示 BMP 位图 (标记受影响页为脏, 自动调用轮询刷新)
 */
static void oled_show_bmp(OLED_S *oled, int16_t x0, int16_t y0,
                          int16_t w, int16_t h, const uint8_t *bmp)
{
    if (x0 < 0 || x0 + w > OLED_WIDTH || y0 < 0 || y0 + h > OLED_HEIGHT)
        return;

    const uint8_t *p = bmp;
    for (int16_t y = y0; y < y0 + h; y += 8)
    {
        for (int16_t x = x0; x < x0 + w; x++)
        {
            for (int16_t bit = 0; bit < 8; bit++)
            {
                oled_set_pixel(oled, x, y + bit, ((*p) >> bit) & 0x01);
            }
            p++;
        }
    }

    mark_dirty_y_range(oled, y0, y0 + h - 1);
}

/**
 * @brief  填充矩形到缓冲区 (仅修改缓冲区 + 标记脏页, 不刷新硬件)
 * @param  on: 1=点亮, 0=熄灭 (清除)
 * @note   供 fill_rect 和 draw_rect(filled=1) 复用
 */
static void fill_rect_buffer(OLED_S *oled, int16_t x, int16_t y,
                             int16_t w, int16_t h, uint8_t on)
{
    /* 裁剪到屏幕范围 */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > OLED_WIDTH)  w = OLED_WIDTH - x;
    if (y + h > OLED_HEIGHT) h = OLED_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    int16_t y_end = y + h - 1;
    uint8_t start_page = (uint8_t)y / 8;
    uint8_t end_page   = (uint8_t)y_end / 8;

    for (uint8_t page = start_page; page <= end_page; page++)
    {
        /* 计算该页中被矩形覆盖的位范围 */
        uint8_t bit_first = (page == start_page) ? (uint8_t)(y % 8) : 0;
        uint8_t bit_last  = (page == end_page)   ? (uint8_t)(y_end % 8) : 7;

        /* 构造位掩码 */
        uint8_t mask = 0;
        for (uint8_t b = bit_first; b <= bit_last; b++)
            mask |= (uint8_t)(1 << b);

        /* 批量填充该页所有列 */
        if (mask == 0xFF)
        {
            /* 整页覆盖: 直接 memset 比逐列循环快 4~8 倍 */
            memset(&oled->buffer.flat[page * OLED_WIDTH + x],
                   on ? 0xFF : 0x00, (size_t)w);
        }
        else
        {
            for (int16_t col = x; col < x + w; col++)
            {
                if (on)
                    oled->buffer.page[page][col] |= mask;
                else
                    oled->buffer.page[page][col] &= ~mask;
            }
        }
    }

    mark_dirty_y_range(oled, y, y_end);
}

/**
 * @brief  清除/填充矩形区域并立即刷新到硬件
 * @param  x, y: 矩形左上角坐标
 * @param  w, h: 矩形宽高
 * @param  on: 1=点亮矩形, 0=清除矩形 (常用于局部清屏)
 * @note   clear 的补充 — 仅清除指定区域而非整屏
 */
static void oled_fill_rect(OLED_S *oled, int16_t x, int16_t y,
                           int16_t w, int16_t h, uint8_t on)
{
    fill_rect_buffer(oled, x, y, w, h, on);
}

/* ======================== 几何绘制 ======================== */

/**
 * @brief  绘制任意直线 (Bresenham 算法)
 * @param  x0, y0: 起点坐标
 * @param  x1, y1: 终点坐标
 * @note   纯整数运算, 无浮点, 适合嵌入式平台
 *         逐点调用 set_pixel, 脏页标记由 set_pixel 内部完成
 */
static void oled_draw_line(OLED_S *oled, int16_t x0, int16_t y0,
                           int16_t x1, int16_t y1)
{
    int16_t dx  = (x1 > x0) ? (int16_t)(x1 - x0) : (int16_t)(x0 - x1);
    int16_t dy  = (y1 > y0) ? (int16_t)(y1 - y0) : (int16_t)(y0 - y1);
    int16_t sx  = (x0 < x1) ? 1 : -1;
    int16_t sy  = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;

    while (1)
    {
        oled_set_pixel(oled, x0, y0, 1);

        if (x0 == x1 && y0 == y1)
            break;

        int16_t e2 = err << 1;  /* 2 * err, 避免乘法 */
        if (e2 > -dy)
        {
            err -= dy;
            x0  += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y0  += sy;
        }
    }
}

/**
 * @brief  绘制水平线 (直接操作缓冲区, 单页内高效完成)
 * @param  x0, x1: 起止列坐标 (无需保证 x0 <= x1, 内部自动排序)
 * @param  y: 水平线的纵坐标
 * @note   水平线所有像素在同一 bit 位, 跨列 OR 同一掩码即可
 */
static void oled_draw_hline(OLED_S *oled, int16_t x0, int16_t x1, int16_t y)
{
    /* 纵坐标越界 */
    if (y < 0 || y >= OLED_HEIGHT)
        return;

    /* 确保 x0 <= x1 */
    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }

    /* 裁剪到屏幕范围 */
    if (x0 < 0)            x0 = 0;
    if (x1 >= OLED_WIDTH)  x1 = OLED_WIDTH - 1;
    if (x0 > x1)           return;

    uint8_t page = (uint8_t)y / 8;
    uint8_t bit  = (uint8_t)(y % 8);
    uint8_t mask = (uint8_t)(1 << bit);

    for (int16_t col = x0; col <= x1; col++)
        oled->buffer.page[page][col] |= mask;

    mark_dirty(oled, page);
}

/**
 * @brief  绘制垂直线 (直接操作缓冲区)
 * @param  x: 垂直线所在列
 * @param  y0, y1: 起止纵坐标 (无需保证 y0 <= y1, 内部自动排序)
 * @note   垂直线可能跨多个页, 逐页设置掩码
 */
static void oled_draw_vline(OLED_S *oled, int16_t x, int16_t y0, int16_t y1)
{
    /* 横坐标越界 */
    if (x < 0 || x >= OLED_WIDTH)
        return;

    /* 确保 y0 <= y1 */
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }

    /* 裁剪到屏幕范围 */
    if (y0 < 0)             y0 = 0;
    if (y1 >= OLED_HEIGHT)  y1 = OLED_HEIGHT - 1;
    if (y0 > y1)            return;

    uint8_t start_page = (uint8_t)y0 / 8;
    uint8_t end_page   = (uint8_t)y1 / 8;

    for (uint8_t page = start_page; page <= end_page; page++)
    {
        /* 计算该页中被垂直线覆盖的位范围 */
        uint8_t bit_first = (page == start_page) ? (uint8_t)(y0 % 8) : 0;
        uint8_t bit_last  = (page == end_page)   ? (uint8_t)(y1 % 8) : 7;

        /* 构造该页内的位掩码 */
        uint8_t mask = 0;
        if (bit_first == 0 && bit_last == 7)
        {
            mask = 0xFF;  /* 整页覆盖: 跳过逐位循环 */
        }
        else
        {
            for (uint8_t b = bit_first; b <= bit_last; b++)
                mask |= (uint8_t)(1 << b);
        }

        oled->buffer.page[page][x] |= mask;
        mark_dirty(oled, page);
    }
}

/**
 * @brief  绘制矩形 (空心或实心)
 * @param  x, y: 矩形左上角坐标
 * @param  w, h: 矩形宽高
 * @param  filled: 0=空心矩形 (四条边), 1=实心矩形 (内部填充)
 * @note   空心模式复用 draw_hline + draw_vline
 *         实心模式复用 fill_rect_buffer (不自动刷新, 与其他 draw 方法一致)
 */
static void oled_draw_rect(OLED_S *oled, int16_t x, int16_t y,
                           int16_t w, int16_t h, uint8_t filled)
{
    if (w <= 0 || h <= 0)
        return;

    if (filled)
    {
        /* 实心: 直接填充缓冲区 */
        fill_rect_buffer(oled, x, y, w, h, 1);
        return;
    }

    /* 空心: 四条边 */
    int16_t x1 = x + w - 1;
    int16_t y1 = y + h - 1;

    /* 上边 + 下边 */
    oled_draw_hline(oled, x, x1, y);
    if (h > 1)
        oled_draw_hline(oled, x, x1, y1);

    /* 左边 + 右边 (去除与上下边重叠的端点) */
    if (h > 2)
    {
        oled_draw_vline(oled, x,  y + 1, y1 - 1);
        oled_draw_vline(oled, x1, y + 1, y1 - 1);
    }
}

/**
 * @brief  绘制圆 (空心或实心, Midpoint 算法)
 * @param  cx, cy: 圆心坐标
 * @param  r: 半径 (像素, >=1 才有可见圆)
 * @param  filled: 0=空心圆 (仅圆周), 1=实心圆 (内部填充)
 * @note   利用 8 分对称性, 只计算 1/8 圆弧, 纯整数运算
 *         实心模式在每个 y 高度画水平线连接左右对称点
 */
static void oled_draw_circle(OLED_S *oled, int16_t cx, int16_t cy,
                             int16_t r, uint8_t filled)
{
    if (r <= 0)
        return;

    int16_t x = 0;
    int16_t y = r;
    int16_t d = 1 - r;  /* 决策参数初始值 */

    while (x <= y)
    {
        if (filled)
        {
            /* 实心: 用水平线连接每对左右对称点, 一次性填充整行 */
            oled_draw_hline(oled, cx - x, cx + x, cy + y);
            oled_draw_hline(oled, cx - x, cx + x, cy - y);
            oled_draw_hline(oled, cx - y, cx + y, cy + x);
            oled_draw_hline(oled, cx - y, cx + y, cy - x);
        }
        else
        {
            /* 空心: 8 分对称绘制圆周 */
            oled_set_pixel(oled, cx + x, cy + y, 1);
            oled_set_pixel(oled, cx + y, cy + x, 1);
            oled_set_pixel(oled, cx - x, cy + y, 1);
            oled_set_pixel(oled, cx - y, cy + x, 1);
            oled_set_pixel(oled, cx + x, cy - y, 1);
            oled_set_pixel(oled, cx + y, cy - x, 1);
            oled_set_pixel(oled, cx - x, cy - y, 1);
            oled_set_pixel(oled, cx - y, cy - x, 1);
        }

        if (d < 0)
        {
            d += (x << 1) + 3;  /* d += 2*x + 3 */
        }
        else
        {
            d += ((x - y) << 1) + 5;  /* d += 2*(x - y) + 5 */
            y--;
        }
        x++;
    }
}

/* ======================== 中文显示 ======================== */

/**
 * @brief  显示 UTF-8 编码的中文字符串 (16×16 字体)
 * @param  x, y: 起始坐标 (像素)
 * @param  str: UTF-8 编码的中文字符串 (仅渲染 F16x16_CN 中已定义的汉字)
 * @note   每个汉字占 16×16 像素, 超出屏幕宽度自动换行
 *         仅识别以 0xE0~0xEF 开头的 3 字节 UTF-8 字符
 *         未定义的汉字静默跳过
 */
static void oled_show_cn(OLED_S *oled, int16_t x, int16_t y, char *str)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT)
        return;

    int16_t cur_x   = x;
    int16_t cur_y   = y;
    int16_t start_y = y;
    uint8_t cn_count = sizeof(F16x16_CN) / sizeof(F16x16_CN[0]);

    while (*str != '\0')
    {
        /* 检测 UTF-8 3 字节中文字符 (首字节 0xE0~0xEF) */
        if (((uint8_t)*str & 0xF0) == 0xE0
            && *(str + 1) != '\0'
            && *(str + 2) != '\0')
        {
            /* 查表匹配 */
            int32_t idx = -1;
            for (uint8_t i = 0; i < cn_count; i++)
            {
                if (F16x16_CN[i].index[0] == str[0] &&
                    F16x16_CN[i].index[1] == str[1] &&
                    F16x16_CN[i].index[2] == str[2])
                {
                    idx = (int32_t)i;
                    break;
                }
            }

            if (idx >= 0)
            {
                /* 换行检查 */
                if (cur_x + 16 > OLED_WIDTH)
                {
                    cur_x = 0;
                    cur_y += 16;
                    if (cur_y + 16 > OLED_HEIGHT) break;
                }

                /* 绘制 16×16 汉字 (列行式: 上半 16 字节 + 下半 16 字节) */
                for (uint8_t half = 0; half < 2; half++)
                {
                    for (uint8_t col = 0; col < 16; col++)
                    {
                        uint8_t data = F16x16_CN[idx].encoder[col + half * 16];
                        for (uint8_t bit = 0; bit < 8; bit++)
                            oled_set_pixel(oled, cur_x + col,
                                          cur_y + bit + half * 8,
                                          (data >> bit) & 0x01);
                    }
                }
                cur_x += 16;
                str += 3;  /* 跳过 3 字节 UTF-8 编码 */
            }
            else
            {
                str++;  /* 未在字库中找到, 跳过 */
            }
        }
        else
        {
            str++;  /* 非中文 UTF-8 字节, 跳过 */
        }
    }

    /* 标记从起始 y 到最终 y 的所有页为脏 */
    mark_dirty_y_range(oled, start_y, cur_y + 16 - 1);
}

void OLED_SetContrast(OLED_S *oled, uint8_t contrast)
{
    write_cmd(oled, 0x81);
    write_cmd(oled, contrast);
}

void OLED_SetDisplayMode(OLED_S *oled, uint8_t mode)
{
    write_cmd(oled, mode);
}

void OLED_ScrollHorizontal(OLED_S *oled, uint8_t start_page,
                           uint8_t end_page, uint8_t direction)
{
    write_cmd(oled, 0x2E);
    write_cmd(oled, direction);
    write_cmd(oled, 0x00);
    write_cmd(oled, start_page);
    write_cmd(oled, 0x05);
    write_cmd(oled, end_page);
    write_cmd(oled, 0x00);
    write_cmd(oled, 0xFF);
    write_cmd(oled, 0x2F);
}

void OLED_ShiftVertical(OLED_S *oled, uint8_t shift)
{
    write_cmd(oled, 0xD3);
    write_cmd(oled, shift);
}

/* ======================== 构造函数 ======================== */

/**
 * @brief  OLED 对象构造函数
 * @param  oled:   OLED 对象指针
 * @param  config: 硬件配置 (I2C 句柄 + 设备地址)
 * @note   调用后即完成初始化，OLED 处于开启状态
 *
 * @example 轮询刷新 (推荐用于简单场景):
 *   OLED_S oled;
 *   OLED_CFG_S cfg = { .hi2c = &hi2c1, .dev_addr = OLED_ADDR_78 };
 *   OLED_Create(&oled, &cfg);
 *
 *   oled.show_str(&oled, 0, 0, "Temp: 25C", 1);
 *   oled.refresh(&oled);       // 只刷新修改过的页
 *
 * @example DMA 刷新 (推荐用于大量数据传输):
 *   oled.show_bmp(&oled, 0, 0, 128, 64, bmp_data);
 *   // BMP 内部已调用 polling refresh
 *
 *   // 或者手动控制 DMA:
 *   oled.show_str(&oled, 0, 0, "Long text...", 1);
 *   oled.refresh_dma(&oled);   // DMA 传输, CPU 可做其他事
 *   // ... 做其他工作 ...
 *   while (HAL_I2C_GetState(oled.cfg.hi2c) != HAL_I2C_STATE_READY);
 */
void OLED_Create(OLED_S *oled, OLED_CFG_S *config)
{
    /* 绑定配置 */
    oled->cfg = *config;

    /* 绑定函数指针 */
    oled->init        = oled_init;
    oled->on          = oled_on;
    oled->off         = oled_off;
    oled->clear       = oled_clear;
    oled->clear_page  = oled_clear_page;
    oled->refresh     = oled_refresh;
    oled->refresh_dma = oled_refresh_dma;
    oled->set_pixel   = oled_set_pixel;
    oled->show_str    = oled_show_str;
    oled->show_num    = oled_show_num;
    oled->show_bmp    = oled_show_bmp;
    oled->fill_rect   = oled_fill_rect;
    oled->draw_line   = oled_draw_line;
    oled->draw_hline  = oled_draw_hline;
    oled->draw_vline  = oled_draw_vline;
    oled->draw_rect   = oled_draw_rect;
    oled->draw_circle = oled_draw_circle;
    oled->show_cn     = oled_show_cn;

    /* 初始化脏页标记 */
    oled->dirty_pages = 0;

    /* 执行硬件初始化 */
    oled->init(oled);
}
