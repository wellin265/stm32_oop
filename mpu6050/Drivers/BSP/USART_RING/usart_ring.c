/**
 * @file    usart_ring.c
 * @brief   UART DMA + 环形缓冲区 + IDLE 中断 驱动实现
 * @note    面向对象风格: 所有函数通过 USART_RING_S 对象调用
 *          零动态内存分配, 用户传入静态缓冲区
 */

#include "usart_ring.h"
#include <string.h>
#include <stdio.h>

/* ======================== 内部辅助 ======================== */

/** 环形缓冲区已用字节数 (调用前需关中断) */
static inline uint16_t ring_used_unlocked(USART_RING_S *handle)
{
    if (handle->head >= handle->tail)
        return handle->head - handle->tail;
    else
        return handle->cfg.rx_buf_size - handle->tail + handle->head;
}

/* ======================== 核心功能 ======================== */

/**
 * @brief  初始化硬件: 配置波特率, 分配缓冲区, 启动 DMA 接收
 * @note   会先 stop() 确保上一次 DMA 完全停止, 然后重新配置
 */
static void usart_ring_init(USART_RING_S *handle)
{
    /* 停止当前传输 */
    handle->stop(handle);

    /* 清空缓冲区 */
    handle->head = 0;
    handle->tail = 0;
    memset(handle->cfg.rx_buf, 0, handle->cfg.rx_buf_size);
    memset(handle->cfg.tx_buf, 0, handle->cfg.tx_buf_size);

    /* 配置波特率 (仅当与当前不一致时重新初始化 UART) */
    if (handle->cfg.baud_rate != handle->cfg.huart->Init.BaudRate)
    {
        handle->cfg.huart->Init.BaudRate = handle->cfg.baud_rate;
        HAL_UART_Init(handle->cfg.huart);
    }

    /* 启动 DMA 接收 + IDLE 中断 */
    handle->start(handle);
}

/**
 * @brief  启动 DMA 接收 + IDLE 中断
 */
static void usart_ring_start(USART_RING_S *handle)
{
    __HAL_UART_ENABLE_IT(handle->cfg.huart, UART_IT_IDLE);
    HAL_UART_Receive_DMA(handle->cfg.huart, handle->cfg.rx_buf,
                         handle->cfg.rx_buf_size);
}

/**
 * @brief  停止 DMA 接收 + IDLE 中断
 */
static void usart_ring_stop(USART_RING_S *handle)
{
    HAL_UART_DMAStop(handle->cfg.huart);
    __HAL_UART_DISABLE_IT(handle->cfg.huart, UART_IT_IDLE);
}

/**
 * @brief  重置: 清空缓冲区和指针, 重新开始接收
 */
static void usart_ring_reset(USART_RING_S *handle)
{
    handle->stop(handle);

    __disable_irq();
    handle->head = 0;
    handle->tail = 0;
    __enable_irq();

    memset(handle->cfg.rx_buf, 0, handle->cfg.rx_buf_size);
    memset(handle->cfg.tx_buf, 0, handle->cfg.tx_buf_size);

    handle->start(handle);
}

/* ======================== 接收操作 ======================== */

/**
 * @brief  获取环形缓冲区中可读的字节数
 */
static uint16_t usart_ring_available(USART_RING_S *handle)
{
    uint16_t avail;
    __disable_irq();
    avail = ring_used_unlocked(handle);
    __enable_irq();
    return avail;
}

/**
 * @brief  从环形缓冲区读取数据 (非阻塞)
 * @param  buf: 用户缓冲区
 * @param  len: 期望读取长度
 * @return 实际读取的字节数 (0 = 无数据)
 */
static uint16_t usart_ring_read(USART_RING_S *handle, uint8_t *buf, uint16_t len)
{
    if (buf == NULL || len == 0)
        return 0;

    uint16_t avail;
    uint16_t read_len;

    /* 在临界区内计算可用字节数, 防止 ISR 更新 head */
    __disable_irq();
    avail = ring_used_unlocked(handle);
    if (avail == 0)
    {
        __enable_irq();
        return 0;
    }

    read_len = (len > avail) ? avail : len;

    /* 复制第一部分: 从 tail 到缓冲区末尾或 read_len */
    uint16_t chunk1 = handle->cfg.rx_buf_size - handle->tail;
    if (chunk1 > read_len) chunk1 = read_len;
    memcpy(buf, &handle->cfg.rx_buf[handle->tail], chunk1);

    /* 复制第二部分: 缓冲区头部 (环形回绕) */
    if (chunk1 < read_len)
    {
        memcpy(buf + chunk1, handle->cfg.rx_buf, read_len - chunk1);
    }

    /* 更新 tail (在临界区内) */
    handle->tail = (handle->tail + read_len) % handle->cfg.rx_buf_size;
    __enable_irq();

    return read_len;
}

/**
 * @brief  清空接收缓冲区 (丢弃所有未读数据)
 */
static void usart_ring_flush(USART_RING_S *handle)
{
    __disable_irq();
    handle->tail = handle->head;
    __enable_irq();
}

/**
 * @brief  IDLE 中断处理函数 — 帧结束时更新 head 指针
 * @note   必须在 USARTx_IRQHandler 中调用, 例如:
 *         void USART1_IRQHandler(void) {
 *             usart.irq_handler(&usart);
 *             HAL_UART_IRQHandler(&huart1);
 *         }
 */
static void usart_ring_irq_handler(USART_RING_S *handle)
{
    if (__HAL_UART_GET_FLAG(handle->cfg.huart, UART_FLAG_IDLE) != RESET)
    {
        __HAL_UART_CLEAR_IDLEFLAG(handle->cfg.huart);

        /* DMA 剩余计数 → 已接收字节数 → head 位置 */
        uint16_t received = handle->cfg.rx_buf_size
                          - __HAL_DMA_GET_COUNTER(handle->cfg.huart->hdmarx);
        handle->head = received % handle->cfg.rx_buf_size;
    }
}

/* ======================== 发送操作 ======================== */

/**
 * @brief  格式化发送 (printf 风格)
 * @param  format: printf 格式字符串
 * @param  ...: 可变参数
 * @return USART_RING_OK  = 启动成功
 *         USART_RING_BUSY = 上次发送未完成 (本次数据丢弃)
 */
static int usart_ring_send(USART_RING_S *handle, const char *format, ...)
{
    if (HAL_DMA_GetState(handle->cfg.huart->hdmatx) != HAL_DMA_STATE_READY)
        return USART_RING_BUSY;

    uint16_t len;
    va_list args;
    va_start(args, format);
    len = vsnprintf((char *)handle->cfg.tx_buf, handle->cfg.tx_buf_size,
                    format, args);
    va_end(args);

    /* 截断超长数据 */
    if (len >= handle->cfg.tx_buf_size)
        len = handle->cfg.tx_buf_size - 1;

    HAL_UART_Transmit_DMA(handle->cfg.huart, handle->cfg.tx_buf, len);
    return USART_RING_OK;
}

/**
 * @brief  原始字节发送 (二进制数据)
 * @param  data: 要发送的数据
 * @param  len:  数据长度
 * @return USART_RING_OK  = 启动成功
 *         USART_RING_BUSY = 上次发送未完成
 *         USART_RING_ERR  = 数据超出 tx_buf_size
 */
static int usart_ring_send_raw(USART_RING_S *handle, const uint8_t *data,
                               uint16_t len)
{
    if (data == NULL || len == 0)
        return USART_RING_ERR;

    if (HAL_DMA_GetState(handle->cfg.huart->hdmatx) != HAL_DMA_STATE_READY)
        return USART_RING_BUSY;

    if (len > handle->cfg.tx_buf_size)
        return USART_RING_ERR;

    memcpy(handle->cfg.tx_buf, data, len);
    HAL_UART_Transmit_DMA(handle->cfg.huart, handle->cfg.tx_buf, len);
    return USART_RING_OK;
}

/**
 * @brief  查询 DMA 发送是否正在进行
 * @return 1 = 忙 (不可发送新数据), 0 = 空闲
 */
static uint8_t usart_ring_tx_busy(USART_RING_S *handle)
{
    return (HAL_DMA_GetState(handle->cfg.huart->hdmatx) != HAL_DMA_STATE_READY);
}

/* ======================== 高级功能 ======================== */

/**
 * @brief  动态切换波特率
 * @note   会先关闭 DMA, 重新配置 UART, 清空缓冲区, 再重启接收
 */
static void usart_ring_set_baud_rate(USART_RING_S *handle, uint32_t baud_rate)
{
    handle->stop(handle);

    handle->cfg.baud_rate = baud_rate;
    handle->cfg.huart->Init.BaudRate = baud_rate;
    HAL_UART_Init(handle->cfg.huart);

    /* 清空旧波特率下可能接收到的垃圾数据 */
    handle->head = 0;
    handle->tail = 0;
    memset(handle->cfg.rx_buf, 0, handle->cfg.rx_buf_size);
    memset(handle->cfg.tx_buf, 0, handle->cfg.tx_buf_size);

    handle->start(handle);
}

/* ======================== 构造函数 ======================== */

/**
 * @brief  USART_RING 对象构造函数
 * @param  handle: 对象指针
 * @param  config: 硬件配置 (包含用户分配的 rx_buf/tx_buf)
 * @note   必须使用静态/全局缓冲区, 不建议用栈上变量
 *
 * @example 使用示例:
 *   static uint8_t rx_buf[512], tx_buf[256];
 *   USART_RING_S uart;
 *   USART_RING_CFG_S cfg = {
 *       .huart = &huart1, .rx_buf = rx_buf, .rx_buf_size = sizeof(rx_buf),
 *       .tx_buf = tx_buf, .tx_buf_size = sizeof(tx_buf), .baud_rate = 115200
 *   };
 *   USART_RING_Create(&uart, &cfg);
 *
 *   // 发送
 *   uart.send(&uart, "Hello %d\r\n", 123);
 *
 *   // 等待发送完成 (OTA 场景必须)
 *   while (uart.tx_busy(&uart));
 *
 *   // 接收
 *   uint16_t n = uart.available(&uart);
 *   if (n > 0) {
 *       uint8_t data[64];
 *       uint16_t r = uart.read(&uart, data, sizeof(data));
 *   }
 */
void USART_RING_Create(USART_RING_S *handle, USART_RING_CFG_S *config)
{
    /* 绑定配置 */
    handle->cfg = *config;

    /* 参数校验 */
    if (config->rx_buf == NULL || config->rx_buf_size == 0)
        return;
    if (config->tx_buf == NULL || config->tx_buf_size == 0)
        return;

    /* 绑定函数指针 */
    handle->init          = usart_ring_init;
    handle->start         = usart_ring_start;
    handle->stop          = usart_ring_stop;
    handle->reset         = usart_ring_reset;
    handle->read          = usart_ring_read;
    handle->available     = usart_ring_available;
    handle->flush         = usart_ring_flush;
    handle->send          = usart_ring_send;
    handle->send_raw      = usart_ring_send_raw;
    handle->tx_busy       = usart_ring_tx_busy;
    handle->irq_handler   = usart_ring_irq_handler;
    handle->set_baud_rate = usart_ring_set_baud_rate;

    /* 初始化硬件 + 启动 DMA 接收 */
    handle->init(handle);
}
