# AT24C02 驱动

硬件 I2C EEPROM 读写（256 字节）。

## 使用

```c
AT24C02_S eeprom;
AT24C02_CONFIG_T cfg = {&hi2c1, 0xA0, 8, 256};  // I2C句柄, 设备地址, 页大小, 容量
at24c02_create(&eeprom, &cfg);

uint8_t buf[16];
eeprom.read(&eeprom, 0, buf, 16);     // 从地址0读16字节
eeprom.write(&eeprom, 0, buf, 16);    // 从地址0写16字节（自动跨页）
```

## 方法

| 方法 | 说明 | 返回值 |
|------|------|--------|
| `read(obj, addr, buf, len)` | 读取 len 字节 | 0=成功 |
| `write(obj, addr, buf, len)` | 写入 len 字节 | 0=成功 |

> 写入自动处理页边界（AT24C02 每页 8 字节），每次写后等待 5ms。
