/**
 * @file    at24c02.c
 * @brief   AT24C02 EEPROM I2C 驱动实现
 * @note    面向对象风格: 所有函数通过 AT24C02_S 对象调用。
 *          使用 HAL I2C Memory 函数实现读写，写入自动处理页边界并等待写周期。
 */

#include "at24c02.h"

/* ======================== 内部辅助 ======================== */

/**
 * @brief  校验设备地址范围
 * @param  obj: 对象指针
 * @param  addr: 起始地址
 * @param  len:  数据长度
 * @return 0 = 地址越界, 1 = 合法
 */
static uint8_t at24c02_check_addr(AT24C02_S *obj, uint16_t addr, uint16_t len)
{
    if ((uint32_t)addr + len > (uint32_t)obj->cfg.capacity)
        return 0;
    return 1;
}

/* ======================== 核心功能 ======================== */

/**
 * @brief  初始化对象内部状态
 * @param  obj: 对象指针
 */
static void at24c02_init(AT24C02_S *obj)
{
    obj->busy = 0;
}

/**
 * @brief  从 EEPROM 指定地址读取数据
 * @param  obj:  对象指针
 * @param  addr: 起始地址 (0 ~ capacity-1)
 * @param  buf:  接收缓冲区指针
 * @param  len:  读取字节数
 * @return 0 = 失败, 1 = 成功
 * @note   使用 HAL_I2C_Mem_Read 实现，单次 I2C 事务完成。
 *         AT24C02 内部地址计数器在 256 字节边界自动回卷。
 */
static uint8_t at24c02_read(AT24C02_S *obj, uint16_t addr,
                             uint8_t *buf, uint16_t len)
{
    HAL_StatusTypeDef status;

    /* 参数校验 */
    if (buf == NULL || len == 0) return 0;
    if (obj->cfg.hi2c == NULL) return 0;
    if (!at24c02_check_addr(obj, addr, len)) return 0;

    status = HAL_I2C_Mem_Read(obj->cfg.hi2c, obj->cfg.dev_addr,
                               addr, I2C_MEMADD_SIZE_8BIT,
                               buf, len, AT24C02_I2C_TIMEOUT_MS);

    return (status == HAL_OK) ? 1 : 0;
}

/**
 * @brief  向 EEPROM 指定地址写入数据 (自动处理页边界)
 * @param  obj:  对象指针
 * @param  addr: 起始地址 (0 ~ capacity-1)
 * @param  buf:  发送缓冲区指针
 * @param  len:  写入字节数
 * @return 0 = 失败, 1 = 成功
 * @note   AT24C02 每页 8 字节，跨页写入需分多次 I2C 事务。
 *         每次页写入后需等待 5ms 内部写周期完成。
 */
static uint8_t at24c02_write(AT24C02_S *obj, uint16_t addr,
                              const uint8_t *buf, uint16_t len)
{
    HAL_StatusTypeDef status;
    uint16_t remaining = len;
    uint16_t offset = 0;

    /* 参数校验 */
    if (buf == NULL || len == 0) return 0;
    if (obj->cfg.hi2c == NULL) return 0;
    if (!at24c02_check_addr(obj, addr, len)) return 0;

    obj->busy = 1;

    while (remaining > 0U)
    {
        /* 计算当前页剩余空间 */
        uint8_t page_offset = (uint8_t)(addr & (obj->cfg.page_size - 1U));
        uint16_t chunk = (uint16_t)(obj->cfg.page_size - page_offset);

        if (chunk > remaining) chunk = remaining;

        status = HAL_I2C_Mem_Write(obj->cfg.hi2c, obj->cfg.dev_addr,
                                    addr, I2C_MEMADD_SIZE_8BIT,
                                    (uint8_t *)&buf[offset], chunk,
                                    AT24C02_I2C_TIMEOUT_MS);

        if (status != HAL_OK)
        {
            obj->busy = 0;
            return 0;
        }

        /* 等待内部写周期完成 (最大 5ms) */
        HAL_Delay(AT24C02_WRITE_DELAY_MS);

        addr      += chunk;
        offset    += chunk;
        remaining -= chunk;
    }

    obj->busy = 0;
    return 1;
}

/* ======================== 构造函数 ======================== */

/**
 * @brief  AT24C02 EEPROM 对象构造函数
 * @param  obj:    对象指针 (由调用者分配)
 * @param  config: 硬件配置指针 (由调用者分配)
 * @note   调用后设备即就绪，可直接使用 read/write 成员函数。
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
 *   eeprom.read(&eeprom, 0, buf, 16);
 *   eeprom.write(&eeprom, 0, buf, 16);
 */
void AT24C02_Create(AT24C02_S *obj, AT24C02_CFG_S *config)
{
    /* 绑定配置 (值拷贝) */
    obj->cfg = *config;

    /* 设置默认值 */
    if (obj->cfg.page_size == 0)
        obj->cfg.page_size = AT24C02_PAGE_SIZE;
    if (obj->cfg.capacity == 0)
        obj->cfg.capacity = AT24C02_CAPACITY;

    /* 绑定函数指针 */
    obj->read  = at24c02_read;
    obj->write = at24c02_write;

    /* 执行初始化，完成内部状态复位 */
    at24c02_init(obj);
}
