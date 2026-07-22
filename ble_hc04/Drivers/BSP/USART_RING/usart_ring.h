/**
 * @file    usart_ring.h
 * @brief   UART DMA + 环形缓冲区 + IDLE 中断 驱动 — 面向对象封装
 * @note    适用于 BLE OTA 固件更新等高可靠性场景:
 *          - 用户提供静态缓冲区, 无动态内存分配
 *          - DMA 发送支持完成检测, 避免静默丢帧
 *          - 环形缓冲区读写均有关中断保护
 *
 * @example 基本使用:
 *   USART_RING_S usart;
 *   uint8_t rx_buf[256];
 *   uint8_t tx_buf[256];
 *
 *   USART_RING_CFG_S cfg = { .huart = &huart1, .rx_buf = rx_buf,
 *       .rx_buf_size = sizeof(rx_buf), .tx_buf = tx_buf,
 *       .tx_buf_size = sizeof(tx_buf) };
 *   USART_RING_Create(&usart, &cfg);
 *
 *   // 接收
 *   uint16_t n = usart.available(&usart);
 *   uint8_t data[64];
 *   usart.read(&usart, data, sizeof(data));
 *
 *   // 发送
 *   usart.send(&usart, "Hello %d\r\n", 123);
 *   usart.send_raw(&usart, raw_data, 5);
 */

#ifndef _USART_RING_H_
#define _USART_RING_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdarg.h>

/* ======================== 常量定义 ======================== */

/** 返回值 */
#define USART_RING_OK       0
#define USART_RING_ERR      (-1)
#define USART_RING_BUSY     (-2)

/* ======================== 类型定义 ======================== */

/** UART 环形缓冲区配置 */
typedef struct
{
    UART_HandleTypeDef *huart;      /**< UART 句柄指针 */
    uint8_t *rx_buf;                /**< 接收环形缓冲区 (用户分配) */
    uint16_t rx_buf_size;           /**< 接收缓冲区大小 */
    uint8_t *tx_buf;                /**< 发送缓冲区 (用户分配, 需 >= tx_buf_size) */
    uint16_t tx_buf_size;           /**< 发送缓冲区大小 */
    uint32_t baud_rate;             /**< 初始波特率 */
} USART_RING_CFG_S;

/* 前向声明 */
typedef struct USART_RING_S USART_RING_S;

/** 函数指针类型定义 */
typedef void    (*USART_RING_INIT_FUNC)(USART_RING_S *handle);
typedef void    (*USART_RING_START_FUNC)(USART_RING_S *handle);
typedef void    (*USART_RING_STOP_FUNC)(USART_RING_S *handle);
typedef void    (*USART_RING_RESET_FUNC)(USART_RING_S *handle);
typedef uint16_t(*USART_RING_READ_FUNC)(USART_RING_S *handle, uint8_t *buf, uint16_t len);
typedef uint16_t(*USART_RING_AVAIL_FUNC)(USART_RING_S *handle);
typedef void    (*USART_RING_FLUSH_FUNC)(USART_RING_S *handle);
typedef int     (*USART_RING_SEND_FUNC)(USART_RING_S *handle, const char *format, ...);
typedef int     (*USART_RING_SEND_RAW_FUNC)(USART_RING_S *handle, const uint8_t *data, uint16_t len);
typedef uint8_t (*USART_RING_TX_BUSY_FUNC)(USART_RING_S *handle);
typedef void    (*USART_RING_IRQ_FUNC)(USART_RING_S *handle);
typedef void    (*USART_RING_SET_BAUD_FUNC)(USART_RING_S *handle, uint32_t baud_rate);

/** UART 环形缓冲区对象 */
struct USART_RING_S
{
    USART_RING_CFG_S cfg;                           /**< 硬件配置 */

    /* ---- 内部状态 (用户不应直接访问) ---- */
    volatile uint16_t head;                         /**< 写指针 (ISR 更新) */
    volatile uint16_t tail;                         /**< 读指针 (用户侧更新) */

    /* ---- 核心功能 ---- */
    USART_RING_INIT_FUNC     init;                  /**< 初始化/重新配置 */
    USART_RING_START_FUNC    start;                 /**< 启动 DMA 接收 */
    USART_RING_STOP_FUNC     stop;                  /**< 停止 DMA 接收 */
    USART_RING_RESET_FUNC    reset;                 /**< 重置缓冲区 (清空数据) */

    /* ---- 接收 ---- */
    USART_RING_READ_FUNC     read;                  /**< 读取数据 (非阻塞) */
    USART_RING_AVAIL_FUNC    available;             /**< 获取可读字节数 */
    USART_RING_FLUSH_FUNC    flush;                 /**< 清空接收缓冲区 */

    /* ---- 发送 ---- */
    USART_RING_SEND_FUNC     send;                  /**< 格式化发送 (返回 OK/BUSY) */
    USART_RING_SEND_RAW_FUNC send_raw;              /**< 二进制发送 (返回 OK/BUSY) */
    USART_RING_TX_BUSY_FUNC  tx_busy;               /**< 查询发送是否忙 (1=忙) */

    /* ---- 中断 ---- */
    USART_RING_IRQ_FUNC      irq_handler;           /**< IDLE 中断处理 (需在 USART_IRQHandler 中调用) */
    USART_RING_SET_BAUD_FUNC set_baud_rate;         /**< 动态切换波特率 */
};

/* ======================== 构造函数 ======================== */
void USART_RING_Create(USART_RING_S *handle, USART_RING_CFG_S *config);

#ifdef __cplusplus
}
#endif

#endif /* _USART_RING_H_ */
