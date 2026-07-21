/**
 * @file    at24c02.h
 * @brief   AT24C02 EEPROM I2C 驱动 — 面向对象封装 (HAL I2C + 自动跨页写入)
 * @note    硬件依赖: 需要 CubeMX 完成 I2C 初始化 (hi2c2)。
 *          AT24C02: 256 字节, 每页 8 字节, 设备地址 0xA0 (7-bit: 0x50 << 1)。
 *          写入后需等待 5ms 写周期完成，驱动自动处理页边界。
 *
 *          数据流:
 *          用户 → write(addr, buf, len) → 自动分页 → HAL_I2C_Mem_Write
 *          用户 → read(addr, buf, len)  → HAL_I2C_Mem_Read
 *
 * @example 使用示例:
 *   AT24C02_S eeprom;
 *   AT24C02_CFG_S cfg = {
 *       .hi2c      = &hi2c2,
 *       .dev_addr  = 0xA0,
 *       .page_size = 8,
 *       .capacity  = 256,
 *   };
 *   AT24C02_Create(&eeprom, &cfg);
 *
 *   uint8_t buf[16];
 *   eeprom.read(&eeprom, 0, buf, 16);     // 从地址0读16字节
 *   eeprom.write(&eeprom, 0, buf, 16);    // 从地址0写16字节 (自动跨页)
 */

#ifndef _AT24C02_H_
#define _AT24C02_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ======================== 常量定义 ======================== */

/** AT24C02 每页字节数 */
#define AT24C02_PAGE_SIZE           8

/** AT24C02 总容量 (字节) */
#define AT24C02_CAPACITY            256

/** 写周期等待时间 (ms) */
#define AT24C02_WRITE_DELAY_MS      5

/** I2C 通信超时 (ms) */
#define AT24C02_I2C_TIMEOUT_MS      10

/* ======================== 类型定义 ======================== */

/** AT24C02 硬件配置 */
typedef struct
{
    I2C_HandleTypeDef *hi2c;            /**< I2C 句柄指针 (CubeMX 已初始化) */
    uint16_t           dev_addr;        /**< 设备 I2C 地址 (8-bit, 如 0xA0) */
    uint8_t            page_size;       /**< 每页字节数 (AT24C02 为 8) */
    uint16_t           capacity;        /**< 总容量 (字节, 默认 256) */
} AT24C02_CFG_S;

/* 前向声明 */
typedef struct AT24C02_S AT24C02_S;

/** 函数指针类型定义 */
typedef uint8_t  (*AT24C02_READ_FUNC)(AT24C02_S *obj, uint16_t addr,
                                      uint8_t *buf, uint16_t len);
typedef uint8_t  (*AT24C02_WRITE_FUNC)(AT24C02_S *obj, uint16_t addr,
                                       const uint8_t *buf, uint16_t len);

/** AT24C02 EEPROM 对象 */
struct AT24C02_S
{
    AT24C02_CFG_S cfg;                      /**< 硬件配置 */

    /* ---- 内部状态 (用户不应直接访问) ---- */
    volatile uint8_t busy;                  /**< 忙标志 (ISR/多任务 保护) */

    /* ---- 成员函数 ---- */
    AT24C02_READ_FUNC   read;               /**< 从指定地址读取数据 */
    AT24C02_WRITE_FUNC  write;              /**< 向指定地址写入数据 (自动跨页) */
};

/* ======================== 构造函数 ======================== */
void AT24C02_Create(AT24C02_S *obj, AT24C02_CFG_S *config);

#ifdef __cplusplus
}
#endif

#endif /* _AT24C02_H_ */
