# BLE_HC04

## 这个工程能学什么
这是一个 HC-04 蓝牙透传模块实验，用面向对象的方式封装了 AT 指令驱动，并复用了 `USART_RING` 驱动进行可靠串口通信。
他演示了一个实用的初始化模式——自动波特率检测与自动切换：在多个常见波特率中轮询探测模块当前速率，检测到后自动将其切换为目标高速率（115200），无需手动预知或配置模块的出厂波特率。

## 运行后你会看到什么
- 串口打印初始化过程：轮询探测波特率 → 检测成功 → 获取版本号 → 切换波特率 → 最终验证
- 最终输出 "BLE module initialization successful!" 及模块状态（LED、BT 模式、角色）
- 主循环中每秒通过蓝牙发送 "hello world"，同时回显收到的蓝牙数据

## 代码做了什么
主流程是：
1. 创建 `USART_RING` 对象（USART2 与 HC-04 通信）
2. 调用 `ble_create()` 创建 BLE 对象（构造函数自动完成：波特率探测 → 版本查询 → 波特率切换 → 状态查询 → 最终验证）
3. 构造函数返回后调用 `test()` 和 `set_led()` 验证功能
4. 在主循环中每秒发送 "hello world" 并通过 `available()/read()` 回显收到的数据

## 建议重点看哪几段
- BSP/BLE_HC04 下的驱动代码（对象组合模式——BLE_S 持有 USART_RING* 指针）
- `ble_auto_detect_baud_rate()` 中的波特率自动探测——遍历预定义波特率列表，逐个尝试 AT 测试
- `ble_switch_to_target_baud_rate()` 中的波特率切换流程——发命令 → 等 300ms 让模块完成切换 → MCU 侧切波特率 → 验证
- `ble_command.h` 中所有 AT 指令的宏定义（`AT+BAUD?` / `AT+ROLE=S` 等）
- 各 AT 命令函数中的统一通信模式：`send()` → 轮询 `read()` → `strstr()` 匹配回复 → 超时返回

## 你会学到的知识点
- AT 指令控制蓝牙透传模块（AT / AT+BAUD / AT+VERSION / AT+LED / AT+ROLE / AT+BTMODE 等）
- 对象组合模式——将底层 USART_RING 作为依赖注入上层 BLE 驱动
- 波特率自动检测的实现（多速率轮询 + AT 测试握手）
- 波特率动态切换的时序处理——指令确认（OK）后需等 300ms 再切换 MCU 侧 UART，确保模块侧已完成时钟切换
- AT 指令的统一通信模式：发送 → 超时轮询环形缓冲区 → `strstr()` 匹配响应 → 返回结果
- HC-04 的三种角色模式（Slave / SPP Master / BLE Master）
- 面向对象风格的多层驱动封装
