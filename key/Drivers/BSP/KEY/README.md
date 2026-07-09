# KEY 驱动

按键检测，支持按下/弹起/长按/连发事件 + FIFO 缓冲。

## 使用

```c
KEY_S key;
KEY_CFG_S cfg = {
    .id = 0, .port = KEY_GPIO_Port, .pin = KEY_Pin,
    .active_level = KEY_ACTIVE_LOW,
    .filter_time_ms = 20,
    .long_press_time_ms = 1000,
    .repeat_delay_ms = 500,
    .repeat_speed_ms = 100
};
KEY_Create(&key, &cfg);

// 定时器中周期性扫描 (10ms 一次)
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    key.scan(&key);
}

// 主循环获取事件
KEY_EVENT_T e = key.get_event(&key);
if (e == KEY_EVENT_DOWN) { /* 按下 */ }
```

## 事件类型

| 事件 | 说明 |
|------|------|
| `KEY_EVENT_NONE` | 无事件 |
| `KEY_EVENT_DOWN` | 按下 (消抖后) |
| `KEY_EVENT_UP` | 弹起 |
| `KEY_EVENT_LONG` | 长按 (超过 `long_press_time_ms`) |
| `KEY_EVENT_REPEAT` | 连发 (长按后每 `repeat_speed_ms`) |

## 配置参数

| 参数 | 默认值 | 说明 |
|------|-------|------|
| `filter_time_ms` | 20 | 消抖时间 (ms) |
| `long_press_time_ms` | 1000 | 长按判定时间 (ms) |
| `repeat_delay_ms` | 500 | 连发启动延时 (ms) |
| `repeat_speed_ms` | 100 | 连发间隔 (ms) |

## API 参考

| 成员函数 | 说明 |
|----------|------|
| `key.scan(&key)` | 扫描状态机 (每 10ms 调用) |
| `key.get_event(&key)` → `KEY_EVENT_T` | 取事件 (非阻塞, 无事件返回 NONE) |
| `key.get_event_timeout(&key, want, ms)` → `KEY_EVENT_T` | 等待事件 (阻塞, 超时返回 NONE) |
| `key.get_state(&key)` → `KEY_STATE_T` | 获取当前状态 |
| `key.event_count(&key)` → `uint8_t` | FIFO 中待处理事件数 |
| `key.get_id(&key)` → `uint8_t` | 获取按键 ID |
| `key.reset(&key)` | 清空状态和 FIFO |

## 文件结构

```
Drivers/BSP/KEY/
├── key.h          # 接口定义
├── key.c          # 驱动实现
└── README.md      # 本文件
```
