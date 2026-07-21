/**
 * @file    sort_timer.c
 * @brief   软件定时器驱动实现 — 面向对象封装
 * @note    tick() 必须在 1ms 定时器中断回调中调用
 *          ONCE 模式超时后由 check() 自动停用
 *          AUTO 模式超时后自动重载，无需手动干预
 */

#include "sort_timer.h"

/* ======================== 内部数据 ======================== */

/* 无全局变量，所有状态封装在 SORT_TIMER_S 对象中 */

/* ======================== 内部辅助 ======================== */

/**
 * @brief  查找第一个空闲定时器槽位
 * @param  self: 对象指针
 * @return 空闲槽位索引，-1 = 槽位已满
 */
static int sort_timer_find_free(SORT_TIMER_S *self)
{
    for (uint8_t i = 0U; i < self->cfg.max_timers; i++) {
        if (self->timers[i].active == 0U) {
            return (int)i;
        }
    }
    return -1;
}

/* ======================== 核心功能 ======================== */

/**
 * @brief  时基驱动 — 每 1ms 调用一次
 * @param  self: 对象指针
 * @note   必须在 1ms 定时器 ISR 回调中调用
 *         ONCE 模式: count 归零后置 flag，等待 check() 消费并停用
 *         AUTO 模式: count 归零后置 flag 并自动重载
 */
static void sort_timer_tick(SORT_TIMER_S *self)
{
    SORT_TIMER_ENTRY_S *tmr;

    for (uint8_t i = 0U; i < self->cfg.max_timers; i++) {
        tmr = &self->timers[i];
        if (tmr->active == 0U) {
            continue;
        }
        if (tmr->count > 0U) {
            tmr->count--;
            if (tmr->count == 0U) {
                tmr->flag = 1U;
                if (tmr->mode == (uint8_t)SORT_TIMER_MODE_AUTO) {
                    tmr->count = tmr->preload;
                }
            }
        }
    }
}

/**
 * @brief  注册一个软件定时器
 * @param  self: 对象指针
 * @param  period_ms: 定时周期 (ms)，必须 > 0
 * @param  mode: 工作模式 @see SORT_TIMER_MODE_T
 * @return 定时器 ID (>= 0)，槽位满或参数非法时返回 -1
 */
static int sort_timer_start(SORT_TIMER_S *self, uint32_t period_ms, SORT_TIMER_MODE_T mode)
{
    if (period_ms == 0U) {
        return -1;
    }

    int id = sort_timer_find_free(self);
    if (id < 0) {
        return -1;
    }

    SORT_TIMER_ENTRY_S *tmr = &self->timers[id];

    /* 先填充所有字段，最后置 active=1 确保 ISR 看到完整状态 */
    tmr->mode    = (uint8_t)mode;
    tmr->flag    = 0U;
    tmr->count   = period_ms;
    tmr->preload = period_ms;
    tmr->active  = 1U;

    return id;
}

/**
 * @brief  停用指定定时器
 * @param  self: 对象指针
 * @param  id: 定时器 ID
 * @note   关中断保护 — ISR 中 tick() 也访问定时器条目
 */
static void sort_timer_stop(SORT_TIMER_S *self, int id)
{
    if ((id < 0) || (id >= (int)self->cfg.max_timers)) {
        return;
    }

    __disable_irq();
    self->timers[id].active = 0U;
    self->timers[id].flag   = 0U;
    self->timers[id].count  = 0U;
    __enable_irq();
}

/**
 * @brief  检查定时器是否超时
 * @param  self: 对象指针
 * @param  id: 定时器 ID
 * @return 1 = 已超时，0 = 未超时或 ID 无效
 * @note   读取后自动清除 flag
 *         ONCE 模式第一次返回 1 后自动停用该定时器
 *         关中断保护 flag 读写，防止与 ISR tick() 竞争
 */
static uint8_t sort_timer_check(SORT_TIMER_S *self, int id)
{
    if ((id < 0) || (id >= (int)self->cfg.max_timers)) {
        return 0U;
    }
    if (self->timers[id].active == 0U) {
        return 0U;
    }

    uint8_t flag;
    __disable_irq();
    flag = self->timers[id].flag;
    self->timers[id].flag = 0U;
    __enable_irq();

    if (flag != 0U) {
        if (self->timers[id].mode == (uint8_t)SORT_TIMER_MODE_ONCE) {
            self->timers[id].active = 0U;
        }
        return 1U;
    }

    return 0U;
}

/**
 * @brief  重启定时器（重置计数到预装值）
 * @param  self: 对象指针
 * @param  id: 定时器 ID
 * @note   关中断保护 — 防止与 ISR tick() 竞争 count/flag
 */
static void sort_timer_restart(SORT_TIMER_S *self, int id)
{
    if ((id < 0) || (id >= (int)self->cfg.max_timers)) {
        return;
    }
    if (self->timers[id].active == 0U) {
        return;
    }

    __disable_irq();
    self->timers[id].count = self->timers[id].preload;
    self->timers[id].flag  = 0U;
    __enable_irq();
}

/* ======================== 构造函数 ======================== */

/**
 * @brief  构造软件定时器管理器
 * @param  self: 对象指针（调用者分配，推荐全局或 static）
 * @param  config: 配置指针 @see SORT_TIMER_CFG_S
 * @note   对象生命周期由调用者全权管理，内部无动态分配
 *         max_timers 超出上限时自动截断为 SORT_TIMER_MAX_COUNT
 *         max_timers 为 0 时使用默认值 4
 */
void SORT_TIMER_Create(SORT_TIMER_S *self, SORT_TIMER_CFG_S *config)
{
    /* 1. 复制配置 */
    self->cfg = *config;

    /* 2. 应用默认值 */
    if (self->cfg.max_timers == 0U) {
        self->cfg.max_timers = 4U;
    }
    if (self->cfg.max_timers > SORT_TIMER_MAX_COUNT) {
        self->cfg.max_timers = SORT_TIMER_MAX_COUNT;
    }

    /* 3. 清零定时器池 */
    for (uint8_t i = 0U; i < SORT_TIMER_MAX_COUNT; i++) {
        self->timers[i].mode    = 0U;
        self->timers[i].flag    = 0U;
        self->timers[i].count   = 0U;
        self->timers[i].preload = 0U;
        self->timers[i].active  = 0U;
    }

    /* 4. 绑定成员函数 */
    self->tick    = sort_timer_tick;
    self->start   = sort_timer_start;
    self->stop    = sort_timer_stop;
    self->check   = sort_timer_check;
    self->restart = sort_timer_restart;
}
