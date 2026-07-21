# DS18B20 驱动

OneWire 温度传感器。

## 使用

```c
DS18B20_S ds18b20;
DS18B20_CONFIG_T cfg = {DQ_GPIO_Port, DQ_Pin};
ds18b20_create(&ds18b20, &cfg);

ds18b20.start(&ds18b20);                  // 启动温度转换
delay_ms(750);                             // 等待转换完成
float temp = ds18b20.read_temp(&ds18b20);  // 读取温度
```

## 方法

| 方法 | 说明 |
|------|------|
| `start(obj)` | 启动温度转换 |
| `read_temp(obj)` | 读取温度（℃） |
