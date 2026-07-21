# ESP8266 MQTT 驱动

ESP8266 WiFi + MQTT 客户端，基于 `USART_RING` 串口驱动。通过 AT 指令控制 ESP8266 连接阿里云物联网平台，支持 MQTT 消息收发。

## 特性

- **继承 USART_RING** — 复用 DMA + 环形缓冲区 + IDLE 中断的可靠串口通信
- **面向对象封装** — 函数指针绑定，与 USART_RING / OLED 等驱动风格统一
- **支持阿里云 IoT** — 内置 MQTT 用户配置、连接、订阅流程
- **多类型发布** — 支持 int32_t / float / 字符串三种数据类型
- **JSON 自动封装** — `pub_int/float/str` 自动生成 `{"params":{...},"version":"1.0.0"}` 格式

## 快速开始

### 1. 硬件连接

| ESP8266 | STM32 | 引脚功能 |
|---------|-------|---------|
| TX | PA3 | USART2 RX |
| RX | PA2 | USART2 TX |
| RST | PA4 | 硬件复位（低有效） |
| CH_PD (EN) | PA5 | 模块使能（高有效） |
| VCC | 3.3V | 供电（**需独立供电 ≥500mA**） |
| GND | GND | 共地 |

> **注意**：ESP8266 峰值电流可达 300mA+，MCU 的 3.3V LDO 通常无法提供足够电流，请使用独立的 3.3V 电源模块。

### 2. CubeMX 配置

- USART2：异步模式，115200-8-N-1
- DMA：USART2_RX (DMA1_Channel6, Circular) + USART2_TX (DMA1_Channel7, Normal)
- 中断：USART2 global interrupt + DMA 中断
- GPIO：PA4 (RST)、PA5 (EN) 配置为推挽输出

### 3. 代码示例

```c
#include "usart_ring.h"
#include "esp8266_mqtt.h"

// USART2 环形缓冲区
USART_RING_S usart2_ring;
static uint8_t rx_buf[512];
static uint8_t tx_buf[256];

// ESP8266 对象
ESP8266_MQTT_S esp;

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    // 1. 创建 USART_RING 对象
    USART_RING_CFG_S ucfg = {
        .huart       = &huart2,
        .rx_buf      = rx_buf,
        .rx_buf_size = sizeof(rx_buf),
        .tx_buf      = tx_buf,
        .tx_buf_size = sizeof(tx_buf),
        .baud_rate   = 115200
    };
    USART_RING_Create(&usart2_ring, &ucfg);

    // 2. 等待 ESP8266 上电稳定（视实际供电情况调整）
    HAL_Delay(2000);

    // 3. 创建 ESP8266 MQTT 对象（自动完成 WiFi 连接 + MQTT 初始化）
    ESP8266_MQTT_CONFIG_T cfg = {
        .usart_ring     = &usart2_ring,
        .rst_port       = ESP8266_RST_GPIO_Port,
        .rst_pin        = ESP8266_RST_Pin,
        .ch_pd_port     = ESP8266_EN_GPIO_Port,
        .ch_pd_pin      = ESP8266_EN_Pin,
        .ssid           = "WiFi名称",
        .password       = "WiFi密码",
        .mqtt_broker    = "iot-xxx.mqtt.iothub.aliyuncs.com",
        .mqtt_client_id = "clientId|securemode=2,signmethod=hmacsha256,timestamp=...|",
        .mqtt_username  = "deviceName&productKey",
        .mqtt_password  = "hmacsha256签名结果",
        .mqtt_pub_topic = "/sys/productKey/deviceName/thing/event/property/post",
        .mqtt_sub_topic = "/sys/productKey/deviceName/thing/service/property/set"
    };
    esp8266_mqtt_create(&esp, &cfg);

    // 4. 主循环
    while (1) {
        esp.recv(&esp);                              // 处理 MQTT 接收消息
        esp.pub_int(&esp, "temperature", 25);        // 发布整数
        esp.pub_float(&esp, "humidity", 65.5, 1);    // 发布浮点（1位小数）
        esp.pub_str(&esp, "status", "running");      // 发布字符串
        HAL_Delay(5000);
    }
}
```

### 4. 中断配置

在 `stm32f1xx_it.c` 中调用 `irq_handler`：

```c
#include "usart_ring.h"

extern USART_RING_S usart2_ring;
extern UART_HandleTypeDef huart2;

void USART2_IRQHandler(void)
{
    usart2_ring.irq_handler(&usart2_ring);  // IDLE 中断 → 更新接收指针
    HAL_UART_IRQHandler(&huart2);           // HAL 中断处理
}
```

> CubeMX 已生成 DMA 中断处理 `DMA1_Channel6_IRQHandler` (RX) 和 `DMA1_Channel7_IRQHandler` (TX)，无需手动添加。

## API 参考

### 构造函数

```c
void esp8266_mqtt_create(ESP8266_MQTT_S *obj, ESP8266_MQTT_CONFIG_T *config);
```

创建对象后**自动执行初始化流程**：GPIO 初始化 → 硬件复位 → AT 测试 → STA 模式 → DHCP → WiFi 连接 → MQTT 用户配置 → MQTT 连接 → MQTT 订阅。

> 初始化过程会阻塞约 15~20 秒（取决于网络状况），在此期间无法执行其他任务。

### 配置结构体

```c
typedef struct {
    USART_RING_S*  usart_ring;     // USART_RING 对象指针
    GPIO_TypeDef*  rst_port;       // RST 引脚端口
    uint16_t       rst_pin;        // RST 引脚编号
    GPIO_TypeDef*  ch_pd_port;     // CH_PD (EN) 引脚端口
    uint16_t       ch_pd_pin;      // CH_PD (EN) 引脚编号
    char*          ssid;           // WiFi SSID
    char*          password;       // WiFi 密码
    char*          mqtt_broker;    // MQTT 服务器地址
    char*          mqtt_client_id; // MQTT 客户端 ID
    char*          mqtt_username;  // MQTT 用户名
    char*          mqtt_password;  // MQTT 密码
    char*          mqtt_pub_topic; // 默认发布主题
    char*          mqtt_sub_topic; // 默认订阅主题
} ESP8266_MQTT_CONFIG_T;
```

### 核心方法

| 方法 | 说明 |
|------|------|
| `esp.init(&esp)` | 重新初始化（`create` 时已调用，失败后可手动重试） |
| `esp.recv(&esp)` | 轮询接收 MQTT 消息（需在主循环中周期性调用） |
| `esp.pub_int(&esp, name, val)` | 发布 `int32_t` 类型数据 |
| `esp.pub_float(&esp, name, val, precision)` | 发布 `float` 类型数据，`precision` 为小数位数 |
| `esp.pub_str(&esp, name, val)` | 发布字符串类型数据 |

### 连接状态

```c
esp.mqtt_connected   // 1 = MQTT 已连接, 0 = 未连接
```

### 扩展数据

```c
esp.user_data        // void* 指针，用户可填充，在 recv 中解析后回调
```

## 消息格式

### 发布 (pub_int / pub_float / pub_str)

发送到 ESP8266 的 AT 指令格式：

```
AT+MQTTPUB=0,"<topic>","{\"params\":{\"<name>\":<val>},\"version\":\"1.0.0\"}",0,0
```

### 接收 (recv)

`recv()` 检查串口接收缓冲区，当收到包含 `+MQTTSUBRECV:` 的订阅消息时，通过 `printf` 输出主题和 payload。其他 ESP8266 主动上报的消息（如 `+MQTTDISCONNECTED:`）也会打印。

## 文件结构

```
Drivers/BSP/ESP8266_MQTT/
├── esp8266_mqtt.h    # 接口定义
├── esp8266_mqtt.c    # 驱动实现
└── README.md         # 本文件
```
