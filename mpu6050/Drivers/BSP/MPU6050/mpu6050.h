/**
 * @file    mpu6050.h
 * @brief   MPU6050 六轴传感器 (加速度 + 陀螺仪 + 温度) I2C 驱动 — 面向对象封装
 * @note    硬件依赖: 需要 CubeMX 完成 I2C 外设初始化 (时钟、GPIO 复用)。
 *          本驱动仅依赖已配置好的 I2C_HandleTypeDef 句柄指针，不直接操作 GPIO 寄存器，
 *          因此可移植到任意 STM32 系列 (F1/F4/G0/H7 等)。
 *
 *          芯片特性:
 *          - 三轴加速度计: ±2/±4/±8/±16g 可编程量程
 *          - 三轴陀螺仪:   ±250/±500/±1000/±2000°/s 可编程量程
 *          - 片内温度传感器
 *          - 内置 16 位 ADC，数据寄存器地址连续 (0x3B ~ 0x48)
 *          - 支持 I2C 地址自增，可单次突发读取全部 14 字节传感器数据
 *
 *          数据流:
 *          I2C 突发读 14 字节 (0x3B ~ 0x48)
 *          └─ 加速度 X(2B) + Y(2B) + Z(2B)
 *             + 温度(2B)
 *             + 陀螺仪 X(2B) + Y(2B) + Z(2B)
 *          → 填充 MPU6050_RAW_DATA_S
 *          → accel_to_g() / gyro_to_dps() / temp_to_c() 转换为物理量
 *
 *          双模式支持:
 *          - use_int=0: 轮询模式 — 用户自行控制读取节奏 (HAL_Delay / 定时器)
 *          - use_int=1: INT 模式 — MPU6050 数据就绪时 INT 引脚拉低,
 *            EXTI 回调中调 int_handler() 置标志, 主循环用 is_data_ready() 检查
 *
 *          约定: 本驱动中 I2C 地址指 7 位地址。STM32F1/F0/L0 HAL 在调用
 *          HAL_I2C_Mem_Write/Read 时需要左移 1 位; F4/F7/G0/H7/L4 等新 HAL
 *          直接传入 7 位地址即可。详见 README.md 移植章节。
 *
 * @example 轮询模式 (use_int=0):
 *   MPU6050_S imu;
 *   MPU6050_CFG_S cfg = {
 *       .hi2c        = &hi2c2,
 *       .dev_addr    = MPU6050_DEFAULT_ADDR,
 *       .gyro_range  = MPU6050_GYRO_2000DPS,
 *       .accel_range = MPU6050_ACCEL_2G,
 *       .dlpf        = MPU6050_DLPF_42HZ,
 *       .sample_rate = 100,
 *       .use_int     = 0,              // 轮询模式
 *   };
 *   MPU6050_Create(&imu, &cfg);
 *
 *   MPU6050_RAW_DATA_S raw;
 *   while (1) {
 *       HAL_Delay(10);                 // 用户控制节奏
 *       imu.read_all_raw(&imu, &raw);
 *   }
 *
 * @example INT 模式 (use_int=1, 需 CubeMX 配置 INT→EXTI 下降沿中断):
 *   // 在 HAL_GPIO_EXTI_Callback 中:
 *   void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
 *   {
 *       if (GPIO_Pin == MPU6050_INT_PIN)   // 用户定义的 INT 引脚
 *           imu.int_handler(&imu);         // 仅置标志, 不做 I2C 通信
 *   }
 *
 *   // 主循环 (统一写法, 与轮询模式兼容):
 *   while (1) {
 *       if (imu.is_data_ready(&imu)) {     // INT: 有数据才读; 轮询: 永真
 *           imu.read_all_raw(&imu, &raw);
 *       }
 *   }
 */

#ifndef _MPU6050_H_
#define _MPU6050_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * STM32F1 系列 HAL 头文件。
 * 移植到其他 STM32 系列时, 将下行改为对应系列头文件,
 * 例如 stm32f4xx_hal.h / stm32g0xx_hal.h / stm32h7xx_hal.h
 */
#include "stm32f1xx_hal.h"

/* ======================== 常量定义 ======================== */

/** MPU6050 默认 7 位 I2C 地址 (AD0 引脚接 GND) */
#define MPU6050_DEFAULT_ADDR            0x68

/** MPU6050 备用 7 位 I2C 地址 (AD0 引脚接 VCC) */
#define MPU6050_ALT_ADDR                0x69

/** WHO_AM_I 寄存器出厂固定值 */
#define MPU6050_WHO_AM_I_VAL            0x68

/** I2C 通信超时 (ms) */
#define MPU6050_I2C_TIMEOUT_MS          100

/** 默认采样率 (Hz)，0 表示"使用默认值" */
#define MPU6050_DEFAULT_SAMPLE_RATE     0

/**
 * @brief 将 7 位 I2C 地址转换为 HAL 所需格式
 * @note  STM32F0/F1/F3/L0/L1 HAL 需左移 1 位; F4/F7/G0/H7/L4 等新 HAL 应改为 (addr)
 */
#define MPU6050_HAL_DEVADDR(addr)       ((addr) << 1)

/* ---- 寄存器地址 (按功能分组) ---- */

/** 设备 ID (只读) */
#define MPU6050_RA_WHO_AM_I             0x75

/** 电源管理 1 — 复位、睡眠、时钟源选择 */
#define MPU6050_RA_PWR_MGMT_1           0x6B

/** 电源管理 2 — 各轴加速度计/陀螺仪待机控制 */
#define MPU6050_RA_PWR_MGMT_2           0x6C

/** 采样率分频器 — 采样率 = 1kHz / (1 + div) */
#define MPU6050_RA_SMPLRT_DIV           0x19

/** 数字低通滤波器配置 (DLPF) */
#define MPU6050_RA_CONFIG               0x1A

/** 陀螺仪满量程配置 */
#define MPU6050_RA_GYRO_CONFIG          0x1B

/** 加速度计满量程配置 */
#define MPU6050_RA_ACCEL_CONFIG         0x1C

/** 中断使能 */
#define MPU6050_RA_INT_ENABLE           0x38

/** 中断/旁路引脚配置 */
#define MPU6050_RA_INT_PIN_CFG          0x37

/** 用户控制 — FIFO、I2C 主模式等 */
#define MPU6050_RA_USER_CTRL            0x6A

/** FIFO 使能 */
#define MPU6050_RA_FIFO_EN              0x23

/** 加速度计数据起始寄存器 (X 轴高字节), 连续 6 字节 */
#define MPU6050_RA_ACCEL_XOUT_H         0x3B

/** 温度数据起始寄存器 (高字节), 连续 2 字节 */
#define MPU6050_RA_TEMP_OUT_H           0x41

/** 陀螺仪数据起始寄存器 (X 轴高字节), 连续 6 字节 */
#define MPU6050_RA_GYRO_XOUT_H          0x43

/** 突发读取总长度: 加速度 6 + 温度 2 + 陀螺仪 6 = 14 字节 */
#define MPU6050_BURST_READ_LEN          14

/* ======================== 类型定义 ======================== */

/** 陀螺仪满量程范围 */
typedef enum
{
    MPU6050_GYRO_250DPS  = 0,          /**< ±250°/s  (LSB: 131.0) */
    MPU6050_GYRO_500DPS,               /**< ±500°/s  (LSB:  65.5) */
    MPU6050_GYRO_1000DPS,              /**< ±1000°/s (LSB:  32.8) */
    MPU6050_GYRO_2000DPS,              /**< ±2000°/s (LSB:  16.4) */
} MPU6050_GYRO_RANGE_T;

/** 加速度计满量程范围 */
typedef enum
{
    MPU6050_ACCEL_2G  = 0,             /**< ±2g  (LSB: 16384.0) */
    MPU6050_ACCEL_4G,                  /**< ±4g  (LSB:  8192.0) */
    MPU6050_ACCEL_8G,                  /**< ±8g  (LSB:  4096.0) */
    MPU6050_ACCEL_16G,                 /**< ±16g (LSB:  2048.0) */
} MPU6050_ACCEL_RANGE_T;

/** 数字低通滤波器截止频率 (DLPF_CFG) */
typedef enum
{
    MPU6050_DLPF_256HZ = 0,            /**< 加速度 260Hz / 陀螺仪 256Hz, 延迟 0.98ms, Fs=8kHz */
    MPU6050_DLPF_188HZ,                /**< 加速度 184Hz / 陀螺仪 188Hz, 延迟 1.9ms,  Fs=1kHz */
    MPU6050_DLPF_98HZ,                 /**< 加速度  94Hz / 陀螺仪  98Hz, 延迟 2.8ms,  Fs=1kHz */
    MPU6050_DLPF_42HZ,                 /**< 加速度  44Hz / 陀螺仪  42Hz, 延迟 4.8ms,  Fs=1kHz */
    MPU6050_DLPF_20HZ,                 /**< 加速度  21Hz / 陀螺仪  20Hz, 延迟 8.3ms,  Fs=1kHz */
    MPU6050_DLPF_10HZ,                 /**< 加速度  10Hz / 陀螺仪  10Hz, 延迟 13.4ms, Fs=1kHz */
    MPU6050_DLPF_5HZ,                  /**< 加速度   5Hz / 陀螺仪   5Hz, 延迟 18.6ms, Fs=1kHz */
} MPU6050_DLPF_T;

/** MPU6050 硬件配置 */
typedef struct
{
    I2C_HandleTypeDef    *hi2c;         /**< I2C 句柄 (CubeMX 已初始化, 如 &hi2c2) */
    uint8_t               dev_addr;     /**< 7 位 I2C 地址 (默认 0x68, AD0=GND) */
    MPU6050_GYRO_RANGE_T  gyro_range;   /**< 陀螺仪满量程 */
    MPU6050_ACCEL_RANGE_T accel_range;  /**< 加速度计满量程 */
    MPU6050_DLPF_T        dlpf;         /**< 数字低通滤波器截止频率 */
    uint16_t              sample_rate;  /**< 采样率 (Hz), 0 = 使用默认 100Hz */
    uint8_t               use_int;      /**< 0 = 轮询模式, 1 = INT 引脚中断模式 */
} MPU6050_CFG_S;

/** MPU6050 原始传感器数据 (单次采样, 16 位有符号) */
typedef struct
{
    int16_t accel_x;                    /**< 加速度 X 轴原始值 */
    int16_t accel_y;                    /**< 加速度 Y 轴原始值 */
    int16_t accel_z;                    /**< 加速度 Z 轴原始值 */
    int16_t temp;                       /**< 温度原始值 */
    int16_t gyro_x;                     /**< 陀螺仪 X 轴原始值 */
    int16_t gyro_y;                     /**< 陀螺仪 Y 轴原始值 */
    int16_t gyro_z;                     /**< 陀螺仪 Z 轴原始值 */
} MPU6050_RAW_DATA_S;

/* 前向声明 — 函数指针类型定义中引用自身对象类型 */
typedef struct MPU6050_S MPU6050_S;

/** 函数指针类型定义 */
typedef void     (*MPU6050_INIT_FUNC)(MPU6050_S *imu);
typedef uint8_t  (*MPU6050_RESET_FUNC)(MPU6050_S *imu);
typedef uint8_t  (*MPU6050_WHO_AM_I_FUNC)(MPU6050_S *imu);

typedef uint8_t  (*MPU6050_READ_ALL_RAW_FUNC)(MPU6050_S *imu, MPU6050_RAW_DATA_S *data);
typedef uint8_t  (*MPU6050_READ_ACCEL_RAW_FUNC)(MPU6050_S *imu, MPU6050_RAW_DATA_S *data);
typedef uint8_t  (*MPU6050_READ_GYRO_RAW_FUNC)(MPU6050_S *imu, MPU6050_RAW_DATA_S *data);
typedef uint8_t  (*MPU6050_READ_TEMP_RAW_FUNC)(MPU6050_S *imu, MPU6050_RAW_DATA_S *data);

typedef uint8_t  (*MPU6050_SET_GYRO_RANGE_FUNC)(MPU6050_S *imu, MPU6050_GYRO_RANGE_T range);
typedef uint8_t  (*MPU6050_SET_ACCEL_RANGE_FUNC)(MPU6050_S *imu, MPU6050_ACCEL_RANGE_T range);
typedef uint8_t  (*MPU6050_SET_DLPF_FUNC)(MPU6050_S *imu, MPU6050_DLPF_T dlpf);
typedef uint8_t  (*MPU6050_SET_SAMPLE_RATE_FUNC)(MPU6050_S *imu, uint16_t rate_hz);

typedef float    (*MPU6050_ACCEL_TO_G_FUNC)(MPU6050_S *imu, int16_t raw);
typedef float    (*MPU6050_GYRO_TO_DPS_FUNC)(MPU6050_S *imu, int16_t raw);
typedef float    (*MPU6050_TEMP_TO_C_FUNC)(MPU6050_S *imu, int16_t raw);

typedef void     (*MPU6050_INT_HANDLER_FUNC)(MPU6050_S *imu);
typedef uint8_t  (*MPU6050_IS_DATA_READY_FUNC)(MPU6050_S *imu);

/** MPU6050 外设对象 */
struct MPU6050_S
{
    MPU6050_CFG_S cfg;                              /**< 硬件配置 */

    /* ---- 内部状态 (用户不应直接访问) ---- */
    float    accel_scale;                           /**< 加速度计转换系数 (LSB/g) */
    float    gyro_scale;                            /**< 陀螺仪转换系数 (LSB/(°/s)) */
    volatile uint8_t data_ready;                    /**< INT 模式: ISR 置 1, 主循环消费后清 0 */
    uint8_t  ready;                                 /**< 芯片初始化成功标志 */

    /* ---- 初始化与状态 ---- */
    MPU6050_INIT_FUNC             init;             /**< 重新初始化芯片 (配置寄存器 + 量程 + DLPF) */
    MPU6050_RESET_FUNC            reset;            /**< 软件复位芯片 (写 PWR_MGMT_1[7]=1) */
    MPU6050_WHO_AM_I_FUNC         who_am_i;         /**< 读取 WHO_AM_I 验证芯片存在 */

    /* ---- 数据读取 (原始 ADC 值) ---- */
    MPU6050_READ_ALL_RAW_FUNC     read_all_raw;     /**< 突发读取全部 14 字节 (加速度+温度+陀螺仪, 推荐) */
    MPU6050_READ_ACCEL_RAW_FUNC   read_accel_raw;   /**< 仅读取加速度计 6 字节 (X/Y/Z) */
    MPU6050_READ_GYRO_RAW_FUNC    read_gyro_raw;    /**< 仅读取陀螺仪 6 字节 (X/Y/Z) */
    MPU6050_READ_TEMP_RAW_FUNC    read_temp_raw;    /**< 仅读取温度 2 字节 */

    /* ---- 运行中修改配置 ---- */
    MPU6050_SET_GYRO_RANGE_FUNC   set_gyro_range;   /**< 动态修改陀螺仪量程 (会更新 gyro_scale) */
    MPU6050_SET_ACCEL_RANGE_FUNC  set_accel_range;  /**< 动态修改加速度计量程 (会更新 accel_scale) */
    MPU6050_SET_DLPF_FUNC         set_dlpf;         /**< 动态修改 DLPF 截止频率 */
    MPU6050_SET_SAMPLE_RATE_FUNC  set_sample_rate;  /**< 动态修改采样率 (Hz) */

    /* ---- 数据转换 (原始值 → 物理量) ---- */
    MPU6050_ACCEL_TO_G_FUNC       accel_to_g;       /**< 加速度原始值 → g */
    MPU6050_GYRO_TO_DPS_FUNC      gyro_to_dps;      /**< 陀螺仪原始值 → °/s */
    MPU6050_TEMP_TO_C_FUNC        temp_to_c;        /**< 温度原始值 → °C */

    /* ---- INT 模式 (可选) ---- */
    MPU6050_INT_HANDLER_FUNC      int_handler;      /**< EXTI 回调中调用, 置 data_ready = 1 */
    MPU6050_IS_DATA_READY_FUNC    is_data_ready;    /**< 轮询: 永返 1; INT: 返回并清除 data_ready */
};

/* ======================== 构造函数 ======================== */
void MPU6050_Create(MPU6050_S *imu, MPU6050_CFG_S *config);

#ifdef __cplusplus
}
#endif

#endif /* _MPU6050_H_ */
