# USART_RING 驱动

UART DMA + 环形缓冲区 + IDLE 中断驱动，用于 BLE OTA 固件更新等需要可靠串口通信的场景。

## 特性

- **零动态内存分配** — 用户提供静态缓冲区
- **IDLE 中断帧检测** — 硬件自动识别帧边界，不丢数据
- **DMA 发送 + 忙检测** — 发送前检查 DMA 状态，避免数据覆盖
- **环形缓冲区** — 接收数据持续写入，应用层随机读取
- **面向对象 API** — 与 LED/OLED 驱动风格统一

## 快速开始

```c
#include "usart_ring.h"

// 1. 分配静态缓冲区
static uint8_t rx_buf[512];
static uint8_t tx_buf[256];
USART_RING_S uart;

int main(void)
{
    // 2. 系统初始化
    HAL_Init();
    SystemClock_Config();
    MX_DMA_Init();
    MX_USART1_UART_Init();

    // 3. 创建对象
    USART_RING_CFG_S cfg = {
        .huart        = &huart1,
        .rx_buf       = rx_buf,
        .rx_buf_size  = sizeof(rx_buf),
        .tx_buf       = tx_buf,
        .tx_buf_size  = sizeof(tx_buf),
        .baud_rate    = 115200
    };
    USART_RING_Create(&uart, &cfg);

    while (1)
    {
        // 4. 接收: 先查可用再读
        uint16_t n = uart.available(&uart);
        if (n > 0)
        {
            uint8_t data[128];
            uint16_t r = uart.read(&uart, data, sizeof(data));
            // 处理 data[0..r-1]
        }

        // 5. 发送
        uart.send(&uart, "Sensor: %d\r\n", value);
        // OTA 场景: 等待发送完成再发下一帧
        while (uart.tx_busy(&uart));
    }
}
```

## 中断配置

在 `stm32f1xx_it.c` 中调用 `irq_handler`：

```c
extern USART_RING_S uart;

void USART1_IRQHandler(void)
{
    uart.irq_handler(&uart);        // IDLE 中断 → 更新接收指针
    HAL_UART_IRQHandler(&huart1);   // HAL 中断处理 (错误等)
}
```

> CubeMX 已生成 DMA 中断处理 `DMA1_Channel5_IRQHandler` (RX) 和 `DMA1_Channel4_IRQHandler` (TX)，无需手动添加。

## API 参考

### 构造函数

```c
void USART_RING_Create(USART_RING_S *handle, USART_RING_CFG_S *config);
```

### 配置结构体

```c
typedef struct {
    UART_HandleTypeDef *huart;    // UART 句柄
    uint8_t *rx_buf;              // 接收缓冲区 (用户分配)
    uint16_t rx_buf_size;         // 接收缓冲区大小
    uint8_t *tx_buf;              // 发送缓冲区 (用户分配)
    uint16_t tx_buf_size;         // 发送缓冲区大小
    uint32_t baud_rate;           // 初始波特率
} USART_RING_CFG_S;
```

### 接收操作

| 成员函数 | 说明 |
|----------|------|
| `uart.available(&uart)` → `uint16_t` | 可读字节数 |
| `uart.read(&uart, buf, len)` → `uint16_t` | 读取数据，返回实际读取数 |
| `uart.flush(&uart)` | 丢弃所有缓存数据 |

### 发送操作

| 成员函数 | 说明 |
|----------|------|
| `uart.send(&uart, "fmt", ...)` → `int` | 格式化发送，返回 OK/BUSY |
| `uart.send_raw(&uart, data, len)` → `int` | 二进制发送，返回 OK/BUSY/ERR |
| `uart.tx_busy(&uart)` → `uint8_t` | 查询是否发送中 (1=忙) |

### 返回值

| 常量 | 值 | 说明 |
|------|----|------|
| `USART_RING_OK` | 0 | 操作成功 |
| `USART_RING_ERR` | -1 | 参数错误 |
| `USART_RING_BUSY` | -2 | DMA 忙，需等待 |

### 控制操作

| 成员函数 | 说明 |
|----------|------|
| `uart.init(&uart)` | 重新初始化 (Create 已调用) |
| `uart.reset(&uart)` | 重置: 清空缓冲区 + 重启接收 |
| `uart.stop(&uart)` | 停止 DMA |
| `uart.start(&uart)` | 启动 DMA |
| `uart.set_baud_rate(&uart, 9600)` | 动态切换波特率 |

## OTA 固件更新示例

```c
// BLE 模块通过 UART 接收固件包
void ota_process_packet(void)
{
    uint8_t packet[256];
    uint16_t len;

    // 读完整帧
    len = uart.available(&uart);
    if (len < 4) return;               // 至少需要包头
    uart.read(&uart, packet, len);

    // 验证+写入 Flash...
    if (ota_write_flash(packet, len) == OK)
    {
        // 发送 ACK (二进制)
        uint8_t ack[] = {0xAA, 0x01, 0x00};
        uart.send_raw(&uart, ack, sizeof(ack));
        // 等待 DMA 完成才能发下一帧
        while (uart.tx_busy(&uart));
    }
    else
    {
        // 发生错误: 丢弃接收缓冲，重新同步
        uart.flush(&uart);

        uart.send(&uart, "ERR\r\n");
        while (uart.tx_busy(&uart));
    }
}

// 接收超时检测
uint8_t ota_wait_packet(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    while (uart.available(&uart) == 0)
    {
        if (HAL_GetTick() - start > timeout_ms)
            return 0;   // 超时
    }
    return 1;           // 有数据
}
```

## 移植

1. 替换 `#include "main.h"` 为对应平台 HAL 头
2. 确保 UART 已配置 DMA (CubeMX 中 UART → DMA Settings → Add RX/TX)

## 文件结构

```
Drivers/BSP/USART_RING/
├── usart_ring.h    # 接口定义
├── usart_ring.c    # 驱动实现
└── README.md       # 本文件
```
