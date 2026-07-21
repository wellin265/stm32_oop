/**
 * @file    sort_timer.h
 * @brief   软件定时器驱动 — 面向对象封装
 * @note    依赖 1ms 周期中断源（如 TIM2），在中断回调中调用 SORT_TIMER_S::tick()
 *          主循环中通过 SORT_TIMER_S::check() 轮询超时状态
 *          所有定时器条目嵌入对象结构体，无动态内存分配
 *
 * @example 使用示例:
 *   // 1. 定义并构造
 *   SORT_TIMER_S st;
 *   SORT_TIMER_CFG_S cfg = { .max_timers = 8 };
 *   SORT_TIMER_Create(&st, &cfg);
 *
 *   // 2. 注册定时器
 *   int led_id = st.start(&st, 500, SORT_TIMER_MODE_AUTO);   // 每 500ms 自动重载
 *   int once   = st.start(&st, 3000, SORT_TIMER_MODE_ONCE);  // 3 秒后单次触发
 *
 *   // 3. ISR 中驱动（TIM2 1ms 中断）
 *   void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
 *       if (htim->Instance == TIM2) g_st.tick(&g_st);
 *   }
 *
 *   // 4. 主循环中检查
 *   while (1) {
 *       if (st.check(&st, led_id)) { LED_TOGGLE(); }
 *       if (st.check(&st, once))   { printf("done!\n"); }
 *   }
 */

#ifndef _SORT_TIMER_H_
#define _SORT_TIMER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* ======================== 宏定义 ======================== */

#define SORT_TIMER_MAX_COUNT  10U    /**< 最大定时器数量上限 */

/* ======================== 枚举 ======================== */

/** @brief 软件定时器工作模式 */
typedef enum {
    SORT_TIMER_MODE_ONCE = 0,        /**< 一次工作模式，超时后自动停用 */
    SORT_TIMER_MODE_AUTO = 1,        /**< 自动定时模式，超时后自动重载 */
} SORT_TIMER_MODE_T;

/* ======================== 结构体 ======================== */

/** @brief 单个软件定时器条目 */
typedef struct {
    volatile uint8_t  mode;          /**< 工作模式 @see SORT_TIMER_MODE_T */
    volatile uint8_t  flag;          /**< 定时到达标志，1 = 已超时 */
    volatile uint32_t count;         /**< 当前递减计数值 (ms) */
    volatile uint32_t preload;       /**< 重装载值 (ms) */
    volatile uint8_t  active;        /**< 是否激活，1 = 已注册 */
} SORT_TIMER_ENTRY_S;

/** @brief 软件定时器配置 */
typedef struct {
    uint8_t max_timers;              /**< 实际使用的定时器数量 (1 ~ SORT_TIMER_MAX_COUNT) */
} SORT_TIMER_CFG_S;

/* 前向声明 */
typedef struct SORT_TIMER_S SORT_TIMER_S;

/* ======================== 函数指针类型 ======================== */

/** @brief 时基驱动：每 1ms 调用一次，递减所有活跃计数器 */
typedef void   (*SORT_TIMER_TICK_FUNC)   (SORT_TIMER_S *self);

/** @brief 注册定时器：分配空闲槽位，返回定时器 ID */
typedef int    (*SORT_TIMER_START_FUNC)  (SORT_TIMER_S *self, uint32_t period_ms, SORT_TIMER_MODE_T mode);

/** @brief 停用定时器：释放槽位 */
typedef void   (*SORT_TIMER_STOP_FUNC)   (SORT_TIMER_S *self, int id);

/** @brief 检查超时：返回 1 = 已超时，读取后自动清除 flag */
typedef uint8_t (*SORT_TIMER_CHECK_FUNC) (SORT_TIMER_S *self, int id);

/** @brief 重启定时器：重置 count = preload */
typedef void   (*SORT_TIMER_RESTART_FUNC)(SORT_TIMER_S *self, int id);

/* ======================== 对象结构体 ======================== */

struct SORT_TIMER_S {
    SORT_TIMER_CFG_S   cfg;                              /**< 用户配置 */
    SORT_TIMER_ENTRY_S timers[SORT_TIMER_MAX_COUNT];     /**< 定时器池 */

    /* ---- 成员函数 ---- */
    SORT_TIMER_TICK_FUNC    tick;    /**< 每 1ms 中断调用，驱动所有计数器 */
    SORT_TIMER_START_FUNC   start;   /**< 注册定时器，成功返回 ID (>= 0)，失败返回 -1 */
    SORT_TIMER_STOP_FUNC    stop;    /**< 停用并释放指定定时器 */
    SORT_TIMER_CHECK_FUNC   check;   /**< 查询是否超时，1 = 超时（读取后自动清标志） */
    SORT_TIMER_RESTART_FUNC restart; /**< 重置计数值，重新开始计时 */
};

/* ======================== 构造函数 ======================== */

/**
 * @brief  构造软件定时器管理器
 * @param  self: 对象指针（由调用者静态分配，推荐全局或 static）
 * @param  config: 配置指针 @see SORT_TIMER_CFG_S
 * @note   对象生命周期由调用者管理，内部无动态分配
 */
void SORT_TIMER_Create(SORT_TIMER_S *self, SORT_TIMER_CFG_S *config);

#ifdef __cplusplus
}
#endif

#endif /* _SORT_TIMER_H_ */
