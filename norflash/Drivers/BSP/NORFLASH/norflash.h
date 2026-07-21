#ifndef __NORFLASH_H_
#define __NORFLASH_H_

#include "stm32f1xx_hal.h"

/* FLASH芯片ID列表 */
#define W25Q80      0XEF13
#define W25Q16      0XEF14
#define W25Q32      0XEF15
#define W25Q64      0XEF16
#define W25Q128     0XEF17
#define W25Q256     0XEF18
#define BY25Q64     0X6816
#define BY25Q128    0X6817
#define NM25Q64     0X5216
#define NM25Q128    0X5217

/* FLASH指令表 */
#define FLASH_WriteEnable           0x06
#define FLASH_WriteDisable          0x04
#define FLASH_ReadStatusReg1        0x05
#define FLASH_ReadStatusReg2        0x35
#define FLASH_ReadStatusReg3        0x15
#define FLASH_WriteStatusReg1       0x01
#define FLASH_WriteStatusReg2       0x31
#define FLASH_WriteStatusReg3       0x11
#define FLASH_ReadData              0x03
#define FLASH_FastReadData          0x0B
#define FLASH_FastReadDual          0x3B
#define FLASH_FastReadQuad          0xEB
#define FLASH_PageProgram           0x02
#define FLASH_PageProgramQuad       0x32
#define FLASH_BlockErase            0xD8
#define FLASH_SectorErase           0x20
#define FLASH_ChipErase             0xC7
#define FLASH_PowerDown             0xB9
#define FLASH_ReleasePowerDown      0xAB
#define FLASH_DeviceID              0xAB
#define FLASH_ManufactDeviceID      0x90
#define FLASH_JedecDeviceID         0x9F
#define FLASH_Enable4ByteAddr       0xB7
#define FLASH_Exit4ByteAddr         0xE9
#define FLASH_SetReadParam          0xC0
#define FLASH_EnterQPIMode          0x38
#define FLASH_ExitQPIMode           0xFF

typedef struct {
    SPI_HandleTypeDef* hspi;
    GPIO_TypeDef* cs_port;
    uint16_t cs_pin;
} NORFLASH_CONFIG_T;

typedef struct NORFLASH_S NORFLASH_S;

struct NORFLASH_S {
    NORFLASH_CONFIG_T config;
    uint16_t chip_id;

    void (*init)(NORFLASH_S* obj);
    uint16_t (*read_id)(NORFLASH_S* obj);
    void (*read)(NORFLASH_S* obj, uint8_t* buf, uint32_t addr, uint16_t len);
    void (*write)(NORFLASH_S* obj, uint8_t* buf, uint32_t addr, uint16_t len);
    void (*read_dma)(NORFLASH_S* obj, uint8_t* buf, uint32_t addr, uint16_t len);
    void (*write_dma)(NORFLASH_S* obj, uint8_t* buf, uint32_t addr, uint16_t len);
    void (*erase_sector)(NORFLASH_S* obj, uint32_t saddr);
    void (*erase_chip)(NORFLASH_S* obj);
};

void norflash_create(NORFLASH_S* obj, NORFLASH_CONFIG_T* config);

#endif
