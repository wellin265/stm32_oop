#include "norflash.h"
#include "spi.h"
#include <string.h>

/* ---- CS引脚控制 ---- */
static void norflash_cs(NORFLASH_S* obj, uint8_t level)
{
    HAL_GPIO_WritePin(obj->config.cs_port, obj->config.cs_pin,
                      level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ---- SPI读写一个字节 ---- */
static uint8_t norflash_spi_rw(NORFLASH_S* obj, uint8_t txdata)
{
    uint8_t rxdata = 0xFF;
    if (HAL_SPI_TransmitReceive(obj->config.hspi, &txdata, &rxdata, 1, 1000) != HAL_OK)
    {
        /* SPI传输异常，返回0xFF（与Flash空闲状态一致） */
        return 0xFF;
    }
    return rxdata;
}

/* ---- GPIO时钟使能 ---- */
static void norflash_gpio_clk_enable(GPIO_TypeDef* port)
{
    if      (port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
    else if (port == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
    else if (port == GPIOE) __HAL_RCC_GPIOE_CLK_ENABLE();
}

/* ---- 读状态寄存器 ---- */
static uint8_t norflash_read_sr(NORFLASH_S* obj, uint8_t regno)
{
    uint8_t cmd = FLASH_ReadStatusReg1;
    switch (regno) {
    case 1: cmd = FLASH_ReadStatusReg1; break;
    case 2: cmd = FLASH_ReadStatusReg2; break;
    case 3: cmd = FLASH_ReadStatusReg3; break;
    }
    norflash_cs(obj, 0);
    norflash_spi_rw(obj, cmd);
    uint8_t byte = norflash_spi_rw(obj, 0xFF);
    norflash_cs(obj, 1);
    return byte;
}

/* ---- 写状态寄存器 ---- */
static void norflash_write_sr(NORFLASH_S* obj, uint8_t regno, uint8_t sr)
{
    uint8_t cmd = FLASH_WriteStatusReg1;
    switch (regno) {
    case 1: cmd = FLASH_WriteStatusReg1; break;
    case 2: cmd = FLASH_WriteStatusReg2; break;
    case 3: cmd = FLASH_WriteStatusReg3; break;
    }
    norflash_cs(obj, 0);
    norflash_spi_rw(obj, cmd);
    norflash_spi_rw(obj, sr);
    norflash_cs(obj, 1);
}


/* ---- 等待空闲 ---- */
static void norflash_wait_busy(NORFLASH_S* obj)
{
    while ((norflash_read_sr(obj, 1) & 0x01) == 0x01);
}

/* ---- 写使能 ---- */
static void norflash_write_enable(NORFLASH_S* obj)
{
    norflash_cs(obj, 0);
    norflash_spi_rw(obj, FLASH_WriteEnable);
    norflash_cs(obj, 1);
}

/* ---- 发送地址(24/32位) ---- */
static void norflash_send_address(NORFLASH_S* obj, uint32_t addr)
{
    if (obj->chip_id == W25Q256)
        norflash_spi_rw(obj, (uint8_t)(addr >> 24));
    norflash_spi_rw(obj, (uint8_t)(addr >> 16));
    norflash_spi_rw(obj, (uint8_t)(addr >> 8));
    norflash_spi_rw(obj, (uint8_t)addr);
}

/* ---- 页写入(<=256字节) ---- */
static void norflash_write_page(NORFLASH_S* obj, uint8_t* buf, uint32_t addr, uint16_t len)
{
    norflash_write_enable(obj);
    norflash_cs(obj, 0);
    norflash_spi_rw(obj, FLASH_PageProgram);
    norflash_send_address(obj, addr);
    for (uint16_t i = 0; i < len; i++)
        norflash_spi_rw(obj, buf[i]);
    norflash_cs(obj, 1);
    norflash_wait_busy(obj);
}

/* ---- 无校验写(自动跨页) ---- */
static void norflash_write_nocheck(NORFLASH_S* obj, uint8_t* buf, uint32_t addr, uint16_t len)
{
    uint16_t page_remain = 256 - (addr % 256);
    if (len <= page_remain) page_remain = len;

    while (1)
    {
        norflash_write_page(obj, buf, addr, page_remain);
        if (len == page_remain) break;

        buf  += page_remain;
        addr += page_remain;
        len  -= page_remain;
        page_remain = (len > 256) ? 256 : len;
    }
}

/* ========== DMA 传输支持 ========== */
/*
 * 设计要点：不额外分配 DMA 缓冲区。
 *   - read_dma:  命令+地址用阻塞发送（4~5字节），数据段直接将用户 buf
 *                填 0xFF 后作为 TX，DMA 接收的同时覆盖为 Flash 数据。
 *   - write_dma: 命令+地址用阻塞发送，数据段直接 DMA 发送用户 buf。
 */

/* ---- 等待DMA传输完成 ---- */
static HAL_StatusTypeDef norflash_dma_wait(NORFLASH_S* obj, uint32_t timeout_ms)
{
    uint32_t tickstart = HAL_GetTick();
    while (HAL_SPI_GetState(obj->config.hspi) != HAL_SPI_STATE_READY)
    {
        if ((HAL_GetTick() - tickstart) > timeout_ms)
        {
            HAL_SPI_Abort(obj->config.hspi);
            return HAL_TIMEOUT;
        }
    }
    return HAL_OK;
}

/* ---- DMA页写入(<=256字节) ---- */
static void norflash_write_page_dma(NORFLASH_S* obj, uint8_t* buf, uint32_t addr, uint16_t len)
{
    norflash_write_enable(obj);

    norflash_cs(obj, 0);
    norflash_spi_rw(obj, FLASH_PageProgram);           /* 命令: 阻塞 */
    norflash_send_address(obj, addr);                   /* 地址: 阻塞 */
    HAL_SPI_Transmit_DMA(obj->config.hspi, buf, len);  /* 数据: DMA发送用户buf */
    norflash_dma_wait(obj, 5000);
    norflash_cs(obj, 1);
    norflash_wait_busy(obj);
}

/* ---- DMA无校验写(自动跨页) ---- */
static void norflash_write_nocheck_dma(NORFLASH_S* obj, uint8_t* buf, uint32_t addr, uint16_t len)
{
    uint16_t page_remain = 256 - (addr % 256);
    if (len <= page_remain) page_remain = len;

    while (1)
    {
        norflash_write_page_dma(obj, buf, addr, page_remain);
        if (len == page_remain) break;

        buf  += page_remain;
        addr += page_remain;
        len  -= page_remain;
        page_remain = (len > 256) ? 256 : len;
    }
}

/* ========== 公开接口实现 ========== */

static void NORFLASH_Init_Impl(NORFLASH_S* obj)
{
    norflash_gpio_clk_enable(obj->config.cs_port);

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = obj->config.cs_pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(obj->config.cs_port, &gpio);
    norflash_cs(obj, 1);

    obj->chip_id = obj->read_id(obj);

    if (obj->chip_id == W25Q256)
    {
        uint8_t sr3 = norflash_read_sr(obj, 3);
        if ((sr3 & 0x01) == 0)
        {
            norflash_write_enable(obj);
            sr3 |= 1 << 1;
            norflash_write_sr(obj, 3, sr3);
            norflash_cs(obj, 0);
            norflash_spi_rw(obj, FLASH_Enable4ByteAddr);
            norflash_cs(obj, 1);
        }
    }
}

static uint16_t NORFLASH_ReadID_Impl(NORFLASH_S* obj)
{
    uint16_t id;
    norflash_cs(obj, 0);
    norflash_spi_rw(obj, FLASH_ManufactDeviceID);
    norflash_spi_rw(obj, 0);
    norflash_spi_rw(obj, 0);
    norflash_spi_rw(obj, 0);
    id  = norflash_spi_rw(obj, 0xFF) << 8;
    id |= norflash_spi_rw(obj, 0xFF);
    norflash_cs(obj, 1);
    return id;
}

static void NORFLASH_Read_Impl(NORFLASH_S* obj, uint8_t* buf, uint32_t addr, uint16_t len)
{
    norflash_cs(obj, 0);
    norflash_spi_rw(obj, FLASH_ReadData);
    norflash_send_address(obj, addr);
    for (uint16_t i = 0; i < len; i++)
        buf[i] = norflash_spi_rw(obj, 0xFF);
    norflash_cs(obj, 1);
}

static void NORFLASH_Write_Impl(NORFLASH_S* obj, uint8_t* buf, uint32_t addr, uint16_t len)
{
    static uint8_t sector_buf[4096];
    uint32_t secpos   = addr / 4096;
    uint16_t secoff   = addr % 4096;
    uint16_t sec_remain = 4096 - secoff;
    uint16_t chunk    = (len <= sec_remain) ? len : sec_remain;

    while (1)
    {
        obj->read(obj, sector_buf, secpos * 4096, 4096);

        uint16_t i;
        for (i = 0; i < chunk; i++)
        {
            if (sector_buf[secoff + i] != 0xFF) break;
        }

        if (i < chunk)
        {
            obj->erase_sector(obj, secpos);
            for (i = 0; i < chunk; i++)
                sector_buf[secoff + i] = buf[i];
            norflash_write_nocheck(obj, sector_buf, secpos * 4096, 4096);
        }
        else
        {
            norflash_write_nocheck(obj, buf, addr, chunk);
        }

        if (len == chunk) break;

        buf  += chunk;
        addr += chunk;
        len  -= chunk;
        secpos++;
        secoff = 0;
        chunk = (len > 4096) ? 4096 : len;
    }
}

static void NORFLASH_EraseSector_Impl(NORFLASH_S* obj, uint32_t saddr)
{
    saddr *= 4096;
    norflash_write_enable(obj);
    norflash_wait_busy(obj);
    norflash_cs(obj, 0);
    norflash_spi_rw(obj, FLASH_SectorErase);
    norflash_send_address(obj, saddr);
    norflash_cs(obj, 1);
    norflash_wait_busy(obj);
}

static void NORFLASH_EraseChip_Impl(NORFLASH_S* obj)
{
    norflash_write_enable(obj);
    norflash_wait_busy(obj);
    norflash_cs(obj, 0);
    norflash_spi_rw(obj, FLASH_ChipErase);
    norflash_cs(obj, 1);
    norflash_wait_busy(obj);
}

static void NORFLASH_Read_DMA_Impl(NORFLASH_S* obj, uint8_t* buf, uint32_t addr, uint16_t len)
{
    /* 阶段1: 命令+地址用阻塞发送（仅4~5字节，CS保持低） */
    norflash_cs(obj, 0);
    norflash_spi_rw(obj, FLASH_ReadData);
    norflash_send_address(obj, addr);

    /* 阶段2: 将用户buf填0xFF作为TX空字节，DMA发送的同时接收Flash数据到同一buf */
    memset(buf, 0xFF, len);
    HAL_SPI_TransmitReceive_DMA(obj->config.hspi, buf, buf, len);
    norflash_dma_wait(obj, 5000);
    norflash_cs(obj, 1);
    /* buf 中现已是接收到的 Flash 数据 */
}

static void NORFLASH_Write_DMA_Impl(NORFLASH_S* obj, uint8_t* buf, uint32_t addr, uint16_t len)
{
    static uint8_t sector_buf[4096];
    uint32_t secpos   = addr / 4096;
    uint16_t secoff   = addr % 4096;
    uint16_t sec_remain = 4096 - secoff;
    uint16_t chunk    = (len <= sec_remain) ? len : sec_remain;

    while (1)
    {
        obj->read(obj, sector_buf, secpos * 4096, 4096);

        uint16_t i;
        for (i = 0; i < chunk; i++)
        {
            if (sector_buf[secoff + i] != 0xFF) break;
        }

        if (i < chunk)
        {
            obj->erase_sector(obj, secpos);
            for (i = 0; i < chunk; i++)
                sector_buf[secoff + i] = buf[i];
            norflash_write_nocheck_dma(obj, sector_buf, secpos * 4096, 4096);
        }
        else
        {
            norflash_write_nocheck_dma(obj, buf, addr, chunk);
        }

        if (len == chunk) break;

        buf  += chunk;
        addr += chunk;
        len  -= chunk;
        secpos++;
        secoff = 0;
        chunk = (len > 4096) ? 4096 : len;
    }
}

/* ========== 构造函数 ========== */

void norflash_create(NORFLASH_S* obj, NORFLASH_CONFIG_T* config)
{
    if (obj == NULL || config == NULL) return;
    memset(obj, 0, sizeof(NORFLASH_S));
    obj->config = *config;

    obj->init         = NORFLASH_Init_Impl;
    obj->read_id      = NORFLASH_ReadID_Impl;
    obj->read         = NORFLASH_Read_Impl;
    obj->write        = NORFLASH_Write_Impl;
    obj->read_dma     = NORFLASH_Read_DMA_Impl;
    obj->write_dma    = NORFLASH_Write_DMA_Impl;
    obj->erase_sector = NORFLASH_EraseSector_Impl;
    obj->erase_chip   = NORFLASH_EraseChip_Impl;

    obj->init(obj);
}
