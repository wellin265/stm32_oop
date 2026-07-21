# NORFLASH 驱动

SPI NOR Flash 驱动（W25Qxx / BY25Qxx / NM25Qxx 系列），软件 CS 引脚控制。

## 快速开始

```c
#include "norflash.h"

NORFLASH_S norflash;

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_SPI1_Init();
    MX_USART1_UART_Init();

    /* 创建实例，自动读取芯片 ID */
    NORFLASH_CONFIG_T cfg = {&hspi1, NORFLASH_CS_GPIO_Port, NORFLASH_CS_Pin};
    norflash_create(&norflash, &cfg);
    printf("ID: 0x%04X\n", norflash.chip_id);

    uint8_t buf[256];

    /* 写入（自动处理扇区擦除和跨页） */
    memset(buf, 0xAA, 256);
    norflash.write(&norflash, buf, 4096, 256);

    /* 读取 */
    memset(buf, 0, 256);
    norflash.read(&norflash, buf, 4096, 256);

    /* DMA 写入 / 读取（大数据量推荐，零额外 RAM 开销） */
    memset(buf, 0xBB, 256);
    norflash.write_dma(&norflash, buf, 8192, 256);
    memset(buf, 0, 256);
    norflash.read_dma(&norflash, buf, 8192, 256);
}
```

## API

### 结构体

```c
typedef struct {
    SPI_HandleTypeDef* hspi;   // SPI 句柄
    GPIO_TypeDef*      cs_port; // CS 引脚端口
    uint16_t           cs_pin;  // CS 引脚编号
} NORFLASH_CONFIG_T;

typedef struct NORFLASH_S {
    NORFLASH_CONFIG_T config;
    uint16_t chip_id;          // 芯片 ID（自动识别）

    void (*init)(NORFLASH_S* obj);
    uint16_t (*read_id)(NORFLASH_S* obj);
    void (*read)(NORFLASH_S* obj, uint8_t* buf, uint32_t addr, uint16_t len);
    void (*write)(NORFLASH_S* obj, uint8_t* buf, uint32_t addr, uint16_t len);
    void (*read_dma)(NORFLASH_S* obj, uint8_t* buf, uint32_t addr, uint16_t len);
    void (*write_dma)(NORFLASH_S* obj, uint8_t* buf, uint32_t addr, uint16_t len);
    void (*erase_sector)(NORFLASH_S* obj, uint32_t saddr);
    void (*erase_chip)(NORFLASH_S* obj);
} NORFLASH_S;
```

### 方法一览

| 方法 | 传输方式 | 说明 |
|------|---------|------|
| `read(buf, addr, len)` | 阻塞 | 读取数据，最大 65535 字节/次 |
| `write(buf, addr, len)` | 阻塞 | 写入数据，自动处理扇区擦除和跨页 |
| `read_dma(buf, addr, len)` | DMA | 同上，DMA 传输，**零额外 RAM 开销** |
| `write_dma(buf, addr, len)` | DMA | 同上，DMA 传输数据段 |
| `erase_sector(saddr)` | 阻塞 | 擦除扇区（saddr × 4096 = 字节地址） |
| `erase_chip()` | 阻塞 | 整片擦除 |
| `read_id()` | 阻塞 | 返回 16 位芯片 ID |

### 阻塞 vs DMA

```
阻塞版:  命令+地址+数据 全部逐字节轮询发送
DMA 版:  命令+地址 (4~5字节, 阻塞) → 数据段 (DMA, CPU 可响应中断)
```

DMA 版本**直接使用用户传入的 buf** 作为 DMA 缓冲区，不额外分配静态内存。
`read_dma` 内部 `memset(buf, 0xFF, len)` 后启动 DMA 发送/接收，接收到的 Flash 数据直接覆盖 buf。

### 扇区管理

Flash 写入前必须先擦除（只能把 1 写成 0，不能反过来）。

`write` / `write_dma` 已内置自动扇区管理：

1. 读目标扇区到 `sector_buf`（4096 字节静态缓冲区）
2. 检查目标区间是否全为 `0xFF`（已擦除）
3. 若需擦除 → `erase_sector` → 合并用户数据 → 写回整扇区
4. 无需擦除 → 直接写入
5. 自动处理扇区边界和跨页

## 支持芯片

| ID | 型号 | 容量 |
|----|------|------|
| `0xEF13` | W25Q80 | 1 MB |
| `0xEF14` | W25Q16 | 2 MB |
| `0xEF15` | W25Q32 | 4 MB |
| `0xEF16` | W25Q64 | 8 MB |
| `0xEF17` | W25Q128 | 16 MB |
| `0xEF18` | W25Q256 | 32 MB（自动 4 字节地址） |
| `0x6816` | BY25Q64 | 8 MB |
| `0x6817` | BY25Q128 | 16 MB |
| `0x5216` | NM25Q64 | 8 MB |
| `0x5217` | NM25Q128 | 16 MB |

## 硬件连接

```
STM32F103          W25Q64
───────────────────────────
PA4  (CS)   ───→  /CS  (1)    外部 10kΩ 上拉推荐
PA5  (SCK)  ───→  CLK  (6)
PA6  (MISO) ───←  DO   (2)    需上拉，防止浮空
PA7  (MOSI) ───→  DI   (5)
VCC          ───→  VCC  (8)   3.3V
GND          ───→  GND  (4)
```

> **注意**：CS 和 MISO 引脚务必配置内部上拉，MCU 复位期间 CS 浮空可能使 Flash 进入不确定状态，导致首次读 ID 异常。外加 10kΩ 上拉电阻最为可靠。

## SPI 配置

- 模式：Mode 3（CPOL=1, CPHA=1）或 Mode 0（CPOL=0, CPHA=0），推荐 Mode 0
- 数据宽度：8 bit，MSB first
- 速率：≤ 80 MHz（W25Q64），建议 ≤ 18 MHz 确保信号完整性
- NSS：软件控制（GPIO 模拟 CS）
- DMA：需配置 SPI1_TX (DMA1_Channel3) + SPI1_RX (DMA1_Channel2)

## printf 重定向

```c
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}
```

> 注意：newlib-nano stdout 为行缓冲，printf 不带 `\n` 的数据不会立即输出。请在结尾加 `\n` 或调用 `fflush(stdout)`。
