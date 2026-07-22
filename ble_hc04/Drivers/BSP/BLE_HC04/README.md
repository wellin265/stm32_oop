# BLE 驱动

蓝牙 AT 指令模块驱动，继承 USART_RING。自动检测波特率、切换目标波特率。

## 使用

```c
BLE_S ble;
USART_RING_S usart;
// ... 初始化 usart（9600 波特率） ...

ble_create(&ble, &usart);  // 自动检测波特率 → 切换到 115200

ble.test(&ble, 500);                        // AT 测试
ble.set_led(&ble, 1, 500);                 // 开 LED
uint8_t role = ble.get_role(&ble, 500);     // 查询角色
```

## AT 命令

命令定义在 `ble_command.h`，格式为 `"AT+CMD\r\n"`。
