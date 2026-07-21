# DHT11 驱动

数字温湿度传感器，开漏输出（不需要切换输入/输出模式）。

## 硬件要求

- **必须外接 4.7kΩ-10kΩ 上拉电阻**在 DATA 和 VCC 之间
- STM32 内部弱上拉（~40kΩ）仅作备份，长线或模块无上拉时信号不可靠

## 使用

```c
DHT11_S dht11;
DHT11_CONFIG_T cfg = {DQ_GPIO_Port, DQ_Pin};
dht11_create(&dht11, &cfg);

if (dht11.read(&dht11)) {
    printf("Temp: %d°C, Hum: %d%%\n", dht11.temperature, dht11.humidity);
}
```

## 方法

| 方法 | 说明 |
|------|------|
| `read(obj)` | 读取一次温湿度（0=失败） |
| `get_temp(obj)` | 获取温度 |
| `get_humidity(obj)` | 获取湿度 |

## 引脚

`DQ_GPIO_Port` / `DQ_Pin` 定义在 `main.h` 中（CubeMX 生成）。

## 原理

引脚始终配置为 `GPIO_MODE_OUTPUT_OD` + `GPIO_PULLUP`：
- 写 `PIN_RESET` → N-MOS 导通 → 总线拉低
- 写 `PIN_SET`   → N-MOS 截止 → 总线释放，由上拉电阻拉高，DHT11 也可以拉低
- `HAL_GPIO_ReadPin()` 在 OD 模式下读取 IDR，反映实际电平
