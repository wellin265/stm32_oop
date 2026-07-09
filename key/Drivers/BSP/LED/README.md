# LED 驱动

面向对象风格的 GPIO LED 驱动，支持高/低电平激活极性自动处理。

## 使用

```c
LED_S led;
LED_CFG_S cfg = {
    .GPIOx    = LED_GPIO_Port,
    .GPIO_Pin = LED_Pin,
    .polarity = LED_ACTIVE_LOW      // 低电平点亮 (负极接 IO, 正极接 VCC)
};
LED_Create(&led, &cfg);

led.on(&led);       // 点亮
led.off(&led);      // 熄灭
led.toggle(&led);   // 翻转
```

## 极性

| 常量 | 接线方式 | 点亮电平 |
|------|---------|---------|
| `LED_ACTIVE_HIGH` | 正极接 IO, 负极接 GND | 高电平 |
| `LED_ACTIVE_LOW` | 负极接 IO, 正极接 VCC | 低电平 |

驱动内部自动处理，`on()`/`off()` 调用者无需关心极性。

## API 参考

| 成员函数 | 说明 |
|----------|------|
| `led.on(&led)` | 点亮 |
| `led.off(&led)` | 熄灭 |
| `led.toggle(&led)` | 翻转状态 |
| `led.get_state(&led)` → `LED_STATE_T` | 获取当前状态 (`LED_ON` / `LED_OFF`) |

## 文件结构

```
Drivers/BSP/LED/
├── led.h          # 接口定义
├── led.c          # 驱动实现
└── README.md      # 本文件
```
