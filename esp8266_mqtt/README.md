# ESP8266_MQTT

## 这个工程能学什么
这是一个 ESP8266 WiFi 模块 + MQTT 物联网通信实验，以面向对象的方式层层封装了从串口驱动、AT 指令控制到 MQTT 消息收发的完整链路。
他演示了如何将已有的 `USART_RING` 驱动组合进更上层的模块——ESP8266 对象持有 `USART_RING` 指针，复用其 DMA + 环形缓冲区能力，在上层通过 AT 指令完成 WiFi 联网和 MQTT 通信。

## 运行后你会看到什么
- 串口打印初始化过程：AT 测试 → WiFi 连接 → MQTT 用户配置 → MQTT 连接 → MQTT 订阅
- 最终输出 "ESP8266 MQTT init success"
- 主循环中持续监听 MQTT 订阅消息，收到消息时打印 "MQTTSUB: ..."
- 可调用 `pub_int/float/str` 向云端上报数据

## 代码做了什么
主流程是：
1. 创建 `USART_RING` 对象（USART2 与 ESP8266 通信）
2. 创建 `ESP8266_MQTT` 对象
3. 创建配置选项（绑定 USART_RING、RST/EN 引脚、WiFi 账号密码、MQTT 服务器/凭证/主题）
4. 注册 ESP8266_MQTT 对象（构造函数内部自动完成 9 步初始化：GPIO → 复位 → AT 测试 → STA → DHCP → WiFi → MQTT 配置 → MQTT 连接 → MQTT 订阅）
5. 在主循环中周期性调用 `recv()` 轮询接收 MQTT 消息

## 建议重点看哪几段
- BSP/ESP8266_MQTT 下的驱动代码（对象组合模式——ESP8266_MQTT 持有 USART_RING 指针）
- `esp8266_at_cmd()` 中的 AT 指令交互模式：发送命令 → 轮询环形缓冲区等待回复 → 超时处理
- `ESP8266_MQTT_Init()` 中的初始化流程编排（9 步顺序执行，每步都有超时和重试）
- `ESP8266_MQTT_PubInt/Float/Str()` 中的 JSON 自动封装（阿里云 IoT 物模型格式）

## 你会学到的知识点
- AT 指令控制 WiFi 模块（AT / AT+CWMODE / AT+CWJAP / AT+MQTTxxx 系列）
- 对象组合模式——将底层驱动（USART_RING）作为上层模块（ESP8266_MQTT）的依赖注入
- AT 指令的请求-回复模式实现：超时轮询 + 字符串匹配
- ESP8266 的硬件复位时序（CH_PD 使能 → RST 拉低 → 延时 → 释放）
- MQTT 协议的基本概念（Broker / ClientID / Username / Password / Publish / Subscribe）
- 阿里云 IoT 平台的设备认证方式（HMAC-SHA256 签名）
- JSON 格式的传感器数据上报（物模型 `{"params":{...},"version":"1.0.0"}`）
- 浮点数转字符串的纯整数实现（避免 `printf` 的 `%f` 依赖）
- 初始化过程中的错误处理与重试机制
- 面向对象风格的多层驱动封装
