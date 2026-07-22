/**
 * @file    mpu6050.c
 * @brief   MPU6050 六轴传感器 I2C 驱动 — 面向对象实现
 * @note    面向对象风格: 所有功能函数通过 MPU6050_S 对象的函数指针调用。
 *          本驱动使用 HAL_I2C_Mem_Write / HAL_I2C_Mem_Read 进行寄存器操作，
 *          不直接操作 GPIO，不依赖 printf，无动态内存分配。
 *
 *          关键设计:
 *          - read_all_raw() 一次 I2C 连续读取 14 字节 (0x3B~0x48),
 *            利用芯片地址自增特性保证加速度/温度/陀螺仪来自同一采样时刻。
 *          - 量程切换时自动重算转换系数 (accel_scale / gyro_scale)。
 *          - 所有公共接口返回 0/1 表示失败/成功。
 */

#include "mpu6050.h"

/* ======================== 内部辅助 ======================== */

/**
 * @brief  向 MPU6050 指定寄存器写入 1 字节
 * @param  imu: 设备对象指针
 * @param  reg: 寄存器地址
 * @param  val: 要写入的值
 * @return 1 = 成功, 0 = I2C 通信失败
 */
static uint8_t i2c_write_reg(MPU6050_S *imu, uint8_t reg, uint8_t val)
{
    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(
        imu->cfg.hi2c,
        MPU6050_HAL_DEVADDR(imu->cfg.dev_addr),
        reg,
        I2C_MEMADD_SIZE_8BIT,
        &val,
        1,
        MPU6050_I2C_TIMEOUT_MS
    );
    return (status == HAL_OK) ? 1 : 0;
}

/**
 * @brief  从 MPU6050 指定寄存器连续读取 N 字节
 * @param  imu: 设备对象指针
 * @param  reg: 起始寄存器地址 (芯片会自动递增地址)
 * @param  buf: 接收缓冲区 (调用者分配)
 * @param  len: 读取字节数
 * @return 1 = 成功, 0 = I2C 通信失败
 */
static uint8_t i2c_read_reg(MPU6050_S *imu, uint8_t reg,
                            uint8_t *buf, uint16_t len)
{
    if (buf == NULL || len == 0) return 0;

    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(
        imu->cfg.hi2c,
        MPU6050_HAL_DEVADDR(imu->cfg.dev_addr),
        reg,
        I2C_MEMADD_SIZE_8BIT,
        buf,
        len,
        MPU6050_I2C_TIMEOUT_MS
    );
    return (status == HAL_OK) ? 1 : 0;
}

/**
 * @brief  将两个字节拼合为有符号 16 位整数 (大端序: MPU6050 先发高字节)
 * @param  high: 高字节
 * @param  low:  低字节
 * @return 拼合后的 int16_t 值
 */
static inline int16_t bytes_to_int16(uint8_t high, uint8_t low)
{
    return (int16_t)(((uint16_t)high << 8) | (uint16_t)low);
}

/**
 * @brief  根据当前量程配置重新计算转换系数
 * @note   在 init() 和 set_*_range() 中调用，保证 accel_scale / gyro_scale 正确
 */
static void compute_scales(MPU6050_S *imu)
{
    /* 加速度计转换系数 (LSB/g) — 参考 MPU6050 数据手册 */
    switch (imu->cfg.accel_range)
    {
    case MPU6050_ACCEL_2G:  imu->accel_scale = 16384.0f; break;
    case MPU6050_ACCEL_4G:  imu->accel_scale =  8192.0f; break;
    case MPU6050_ACCEL_8G:  imu->accel_scale =  4096.0f; break;
    case MPU6050_ACCEL_16G: imu->accel_scale =  2048.0f; break;
    default:                imu->accel_scale = 16384.0f; break;
    }

    /* 陀螺仪转换系数 (LSB/(°/s)) — 参考 MPU6050 数据手册 */
    switch (imu->cfg.gyro_range)
    {
    case MPU6050_GYRO_250DPS:  imu->gyro_scale = 131.0f; break;
    case MPU6050_GYRO_500DPS:  imu->gyro_scale =  65.5f; break;
    case MPU6050_GYRO_1000DPS: imu->gyro_scale =  32.8f; break;
    case MPU6050_GYRO_2000DPS: imu->gyro_scale =  16.4f; break;
    default:                   imu->gyro_scale = 131.0f; break;
    }
}

/* ======================== 核心功能 ======================== */

/**
 * @brief  初始化 MPU6050 — 复位、配置量程/滤波器/采样率
 * @param  imu: 设备对象指针
 * @note   执行流程:
 *          1. 软件复位 (写 0x80 到 PWR_MGMT_1)
 *          2. 等待复位完成 (约 100ms)
 *          3. 唤醒芯片 (PWR_MGMT_1 = 0x00)
 *          4. 配置陀螺仪量程
 *          5. 配置加速度计量程
 *          6. 配置 DLPF
 *          7. 配置采样率
 *          8. 重算转换系数
 *          9. 唤醒全部轴 (PWR_MGMT_2 = 0x00)
 *          10. 选择 PLL X 轴为时钟源 (稳定性最优)
 */
static void mpu6050_init(MPU6050_S *imu)
{
    /* ---- 参数校验 ---- */
    if (imu->cfg.hi2c == NULL) return;

    /* ---- 1. 软件复位 ---- */
    i2c_write_reg(imu, MPU6050_RA_PWR_MGMT_1, 0x80);
    HAL_Delay(100);

    /* ---- 2. 唤醒芯片 (退出睡眠模式) ---- */
    i2c_write_reg(imu, MPU6050_RA_PWR_MGMT_1, 0x00);
    HAL_Delay(10);

    /* ---- 3. 配置量程 ---- */
    uint8_t reg_val;

    /* 陀螺仪量程 (GYRO_CONFIG[4:3]) */
    reg_val = (uint8_t)(imu->cfg.gyro_range << 3);
    i2c_write_reg(imu, MPU6050_RA_GYRO_CONFIG, reg_val);

    /* 加速度计量程 (ACCEL_CONFIG[4:3]) */
    reg_val = (uint8_t)(imu->cfg.accel_range << 3);
    i2c_write_reg(imu, MPU6050_RA_ACCEL_CONFIG, reg_val);

    /* ---- 4. 配置 DLPF ---- */
    reg_val = (uint8_t)imu->cfg.dlpf;
    i2c_write_reg(imu, MPU6050_RA_CONFIG, reg_val);

    /* ---- 5. 配置采样率 ---- */
    uint16_t rate = imu->cfg.sample_rate;
    if (rate == 0) rate = 100;                    /* 默认 100Hz */

    if (rate > 1000) rate = 1000;
    if (rate < 4)    rate = 4;

    reg_val = (uint8_t)(1000 / rate - 1);
    i2c_write_reg(imu, MPU6050_RA_SMPLRT_DIV, reg_val);

    /* ---- 6. 重算转换系数 ---- */
    compute_scales(imu);

    /* ---- 7. 配置中断 (根据 use_int 决定) ---- */
    if (imu->cfg.use_int)
        i2c_write_reg(imu, MPU6050_RA_INT_ENABLE, 0x01);  /* 使能 DATA_RDY */
    else
        i2c_write_reg(imu, MPU6050_RA_INT_ENABLE, 0x00);  /* 关闭全部中断 */

    /* ---- 8. 关闭 FIFO 和 I2C 主模式 ---- */
    i2c_write_reg(imu, MPU6050_RA_FIFO_EN,   0x00);
    i2c_write_reg(imu, MPU6050_RA_USER_CTRL, 0x00);

    /* ---- 9. INT 引脚配置 (低电平有效, 推挽输出) ---- */
    i2c_write_reg(imu, MPU6050_RA_INT_PIN_CFG, 0x80);

    /* ---- 10. 选择 PLL X 轴为时钟源, 全部轴工作 ---- */
    i2c_write_reg(imu, MPU6050_RA_PWR_MGMT_1, 0x01);
    i2c_write_reg(imu, MPU6050_RA_PWR_MGMT_2, 0x00);

    imu->ready = 1;
}

/**
 * @brief  软件复位 MPU6050 (写 PWR_MGMT_1[7]=1)
 * @param  imu: 设备对象指针
 * @return 1 = 复位命令已发送, 0 = I2C 通信失败
 * @note   复位后寄存器恢复默认值, 建议重新调用 init() 完成配置
 */
static uint8_t mpu6050_reset(MPU6050_S *imu)
{
    imu->ready = 0;
    uint8_t ret = i2c_write_reg(imu, MPU6050_RA_PWR_MGMT_1, 0x80);
    if (ret) HAL_Delay(100);
    return ret;
}

/**
 * @brief  读取 WHO_AM_I 寄存器, 验证芯片是否存在
 * @param  imu: 设备对象指针
 * @return 1 = 检测到 MPU6050 (ID = 0x68), 0 = 未检测到或 I2C 通信失败
 */
static uint8_t mpu6050_who_am_i(MPU6050_S *imu)
{
    uint8_t id = 0;
    if (!i2c_read_reg(imu, MPU6050_RA_WHO_AM_I, &id, 1))
        return 0;

    return (id == MPU6050_WHO_AM_I_VAL) ? 1 : 0;
}

/* ======================== 数据读取 ======================== */

/**
 * @brief  突发读取全部传感器数据 (加速度 + 温度 + 陀螺仪) — 推荐使用
 * @param  imu:  设备对象指针
 * @param  data: 原始数据输出结构体
 * @return 1 = 读取成功, 0 = I2C 通信失败
 * @note   从 ACCEL_XOUT_H (0x3B) 开始连续读取 14 字节,
 *         芯片内部自动递增地址, 一次 I2C 事务完成全部读取。
 *         这是效率最高的读取方式: 保证所有数据来自同一采样时刻,
 *         且仅产生一次 I2C 总线开销 (Start + 地址 + 14字节 + Stop)。
 *
 * @example 使用示例:
 *   MPU6050_RAW_DATA_S raw;
 *   if (imu.read_all_raw(&imu, &raw))
 *   {
 *       // 加速度 (g)
 *       float ax = imu.accel_to_g(&imu, raw.accel_x);
 *       // 陀螺仪 (°/s)
 *       float gz = imu.gyro_to_dps(&imu, raw.gyro_z);
 *       // 温度 (°C)
 *       float t  = imu.temp_to_c(&imu, raw.temp);
 *   }
 */
static uint8_t mpu6050_read_all_raw(MPU6050_S *imu, MPU6050_RAW_DATA_S *data)
{
    if (data == NULL) return 0;

    uint8_t buf[MPU6050_BURST_READ_LEN];
    if (!i2c_read_reg(imu, MPU6050_RA_ACCEL_XOUT_H, buf, MPU6050_BURST_READ_LEN))
        return 0;

    /* 解析: 大端序, 高字节在前
       字节顺序: AX_H, AX_L, AY_H, AY_L, AZ_H, AZ_L,
                  T_H,  T_L,  GX_H, GX_L, GY_H, GY_L, GZ_H, GZ_L */
    data->accel_x = bytes_to_int16(buf[0],  buf[1]);
    data->accel_y = bytes_to_int16(buf[2],  buf[3]);
    data->accel_z = bytes_to_int16(buf[4],  buf[5]);
    data->temp    = bytes_to_int16(buf[6],  buf[7]);
    data->gyro_x  = bytes_to_int16(buf[8],  buf[9]);
    data->gyro_y  = bytes_to_int16(buf[10], buf[11]);
    data->gyro_z  = bytes_to_int16(buf[12], buf[13]);

    return 1;
}

/**
 * @brief  仅读取加速度计原始值 (X / Y / Z)
 * @param  imu:  设备对象指针
 * @param  data: 原始数据输出结构体 (仅填充 accel_x / accel_y / accel_z)
 * @return 1 = 读取成功, 0 = I2C 通信失败
 */
static uint8_t mpu6050_read_accel_raw(MPU6050_S *imu, MPU6050_RAW_DATA_S *data)
{
    if (data == NULL) return 0;

    uint8_t buf[6];
    if (!i2c_read_reg(imu, MPU6050_RA_ACCEL_XOUT_H, buf, 6))
        return 0;

    data->accel_x = bytes_to_int16(buf[0], buf[1]);
    data->accel_y = bytes_to_int16(buf[2], buf[3]);
    data->accel_z = bytes_to_int16(buf[4], buf[5]);

    return 1;
}

/**
 * @brief  仅读取陀螺仪原始值 (X / Y / Z)
 * @param  imu:  设备对象指针
 * @param  data: 原始数据输出结构体 (仅填充 gyro_x / gyro_y / gyro_z)
 * @return 1 = 读取成功, 0 = I2C 通信失败
 */
static uint8_t mpu6050_read_gyro_raw(MPU6050_S *imu, MPU6050_RAW_DATA_S *data)
{
    if (data == NULL) return 0;

    uint8_t buf[6];
    if (!i2c_read_reg(imu, MPU6050_RA_GYRO_XOUT_H, buf, 6))
        return 0;

    data->gyro_x = bytes_to_int16(buf[0], buf[1]);
    data->gyro_y = bytes_to_int16(buf[2], buf[3]);
    data->gyro_z = bytes_to_int16(buf[4], buf[5]);

    return 1;
}

/**
 * @brief  仅读取温度传感器原始值
 * @param  imu:  设备对象指针
 * @param  data: 原始数据输出结构体 (仅填充 temp)
 * @return 1 = 读取成功, 0 = I2C 通信失败
 */
static uint8_t mpu6050_read_temp_raw(MPU6050_S *imu, MPU6050_RAW_DATA_S *data)
{
    if (data == NULL) return 0;

    uint8_t buf[2];
    if (!i2c_read_reg(imu, MPU6050_RA_TEMP_OUT_H, buf, 2))
        return 0;

    data->temp = bytes_to_int16(buf[0], buf[1]);

    return 1;
}

/* ======================== 配置修改 ======================== */

/**
 * @brief  动态修改陀螺仪满量程范围 (运行中可调用)
 * @param  imu:   设备对象指针
 * @param  range: 新的量程枚举值
 * @return 1 = 成功, 0 = I2C 通信失败
 * @note   会自动更新 gyro_scale 转换系数, 后续 gyro_to_dps() 使用新系数
 */
static uint8_t mpu6050_set_gyro_range(MPU6050_S *imu,
                                      MPU6050_GYRO_RANGE_T range)
{
    uint8_t reg_val = (uint8_t)(range << 3);
    if (!i2c_write_reg(imu, MPU6050_RA_GYRO_CONFIG, reg_val))
        return 0;

    imu->cfg.gyro_range = range;
    compute_scales(imu);
    return 1;
}

/**
 * @brief  动态修改加速度计满量程范围 (运行中可调用)
 * @param  imu:   设备对象指针
 * @param  range: 新的量程枚举值
 * @return 1 = 成功, 0 = I2C 通信失败
 * @note   会自动更新 accel_scale 转换系数, 后续 accel_to_g() 使用新系数
 */
static uint8_t mpu6050_set_accel_range(MPU6050_S *imu,
                                       MPU6050_ACCEL_RANGE_T range)
{
    uint8_t reg_val = (uint8_t)(range << 3);
    if (!i2c_write_reg(imu, MPU6050_RA_ACCEL_CONFIG, reg_val))
        return 0;

    imu->cfg.accel_range = range;
    compute_scales(imu);
    return 1;
}

/**
 * @brief  动态修改数字低通滤波器截止频率
 * @param  imu:  设备对象指针
 * @param  dlpf: 新的 DLPF 枚举值
 * @return 1 = 成功, 0 = I2C 通信失败
 */
static uint8_t mpu6050_set_dlpf(MPU6050_S *imu, MPU6050_DLPF_T dlpf)
{
    uint8_t reg_val = (uint8_t)dlpf;
    if (!i2c_write_reg(imu, MPU6050_RA_CONFIG, reg_val))
        return 0;

    imu->cfg.dlpf = dlpf;
    return 1;
}

/**
 * @brief  动态修改采样率
 * @param  imu:     设备对象指针
 * @param  rate_hz: 新的采样率 (Hz), 范围 4~1000
 * @return 1 = 成功, 0 = I2C 通信失败
 * @note   采样率 = 1kHz / (1 + SMPLRT_DIV), 通过写 SMPLRT_DIV 寄存器实现
 */
static uint8_t mpu6050_set_sample_rate(MPU6050_S *imu, uint16_t rate_hz)
{
    if (rate_hz > 1000) rate_hz = 1000;
    if (rate_hz < 4)    rate_hz = 4;

    uint8_t reg_val = (uint8_t)(1000 / rate_hz - 1);
    if (!i2c_write_reg(imu, MPU6050_RA_SMPLRT_DIV, reg_val))
        return 0;

    imu->cfg.sample_rate = rate_hz;
    return 1;
}

/* ======================== 数据转换 ======================== */

/**
 * @brief  加速度计原始值 → g 值
 * @param  imu: 设备对象指针
 * @param  raw: 加速度计原始 ADC 值 (int16_t)
 * @return 加速度值 (g)
 * @note   转换公式: g = raw / accel_scale
 *         accel_scale 在 init() 或 set_accel_range() 中根据量程自动计算
 */
static float mpu6050_accel_to_g(MPU6050_S *imu, int16_t raw)
{
    return (float)raw / imu->accel_scale;
}

/**
 * @brief  陀螺仪原始值 → °/s
 * @param  imu: 设备对象指针
 * @param  raw: 陀螺仪原始 ADC 值 (int16_t)
 * @return 角速度 (°/s)
 * @note   转换公式: dps = raw / gyro_scale
 *         gyro_scale 在 init() 或 set_gyro_range() 中根据量程自动计算
 */
static float mpu6050_gyro_to_dps(MPU6050_S *imu, int16_t raw)
{
    return (float)raw / imu->gyro_scale;
}

/**
 * @brief  温度原始值 → °C
 * @param  imu: 设备对象指针 (本函数不使用 imu 状态, 保留参数以统一接口)
 * @param  raw: 温度原始 ADC 值 (int16_t)
 * @return 温度 (°C)
 * @note   转换公式 (MPU6050 数据手册):
 *         Temperature(°C) = raw / 340.0 + 36.53
 */
static float mpu6050_temp_to_c(MPU6050_S *imu, int16_t raw)
{
    (void)imu;  /* 未使用, 消除警告 */
    return (float)raw / 340.0f + 36.53f;
}

/* ======================== INT 模式 ======================== */

/**
 * @brief  INT 中断处理 — EXTI 回调中调用, 仅置标志位
 * @param  imu: 设备对象指针
 * @note   在 HAL_GPIO_EXTI_Callback 中调用, 只做 data_ready = 1,
 *         不做任何 I2C 通信或耗时操作, 确保 ISR 快速返回。
 *         轮询模式 (use_int=0) 下本函数为空操作。
 *
 * @example EXTI 回调示例 (CubeMX 配置 INT → EXTI 下降沿中断后):
 *   void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
 *   {
 *       if (GPIO_Pin == MPU6050_INT_PIN)
 *           imu.int_handler(&imu);
 *   }
 */
static void mpu6050_int_handler(MPU6050_S *imu)
{
    if (imu->cfg.use_int)
        imu->data_ready = 1;
}

/**
 * @brief  查询是否有新数据就绪
 * @param  imu: 设备对象指针
 * @return 轮询模式: 永远返回 1 (由用户控制节奏)
 *         INT 模式:   data_ready 为 1 时返回 1 并清除标志, 否则返回 0
 * @note   INT 模式下使用临界区保护 data_ready 的读-清除操作,
 *         防止 ISR 在中间状态写入导致标志丢失。
 */
static uint8_t mpu6050_is_data_ready(MPU6050_S *imu)
{
    if (!imu->cfg.use_int)
        return 1;                  /* 轮询模式: 随时可读 */

    uint8_t ready;
    __disable_irq();
    ready = imu->data_ready;
    imu->data_ready = 0;
    __enable_irq();
    return ready;
}

/* ======================== 构造函数 ======================== */

/**
 * @brief  MPU6050 对象构造函数
 * @param  imu:    设备对象指针 (调用者负责分配内存, 可以是栈上或全局变量)
 * @param  config: 硬件配置指针 (调用者负责分配内存)
 * @note   构造函数按以下顺序完成初始化:
 *          1. 绑定配置 (值拷贝)
 *          2. 填充未设置的默认值
 *          3. 绑定函数指针 (面向对象多态)
 *          4. 调用 init() 完成芯片寄存器配置
 *
 *         调用后芯片即就绪, 可通过 imu.read_all_raw() 等成员函数直接读取数据。
 *         配置中的 sample_rate = 0 会被替换为默认值 100Hz。
 *         配置中的 dev_addr = 0 会被替换为默认地址 0x68。
 *
 * @example 使用示例:
 *   MPU6050_S imu;
 *   MPU6050_CFG_S cfg = {
 *       .hi2c        = &hi2c2,
 *       .dev_addr    = MPU6050_DEFAULT_ADDR,
 *       .gyro_range  = MPU6050_GYRO_2000DPS,
 *       .accel_range = MPU6050_ACCEL_2G,
 *       .dlpf        = MPU6050_DLPF_42HZ,
 *       .sample_rate = 100,
 *   };
 *   MPU6050_Create(&imu, &cfg);
 *
 *   if (imu.who_am_i(&imu))
 *   {
 *       MPU6050_RAW_DATA_S raw;
 *       while (1)
 *       {
 *           if (imu.read_all_raw(&imu, &raw))
 *           {
 *               float ax = imu.accel_to_g(&imu, raw.accel_x);
 *               float gz = imu.gyro_to_dps(&imu, raw.gyro_z);
 *               // 用户自行处理数据...
 *           }
 *           HAL_Delay(10);  // 约 100Hz 读取
 *       }
 *   }
 */
void MPU6050_Create(MPU6050_S *imu, MPU6050_CFG_S *config)
{
    /* ---- 1. 绑定配置 (值拷贝) ---- */
    imu->cfg = *config;

    /* ---- 2. 填充默认值 ---- */
    if (imu->cfg.dev_addr == 0)
        imu->cfg.dev_addr = MPU6050_DEFAULT_ADDR;

    if (imu->cfg.sample_rate == 0)
        imu->cfg.sample_rate = MPU6050_DEFAULT_SAMPLE_RATE;

    /* ---- 3. 初始化内部状态 ---- */
    imu->accel_scale = 16384.0f;
    imu->gyro_scale  = 131.0f;
    imu->data_ready  = 0;
    imu->ready       = 0;

    /* ---- 4. 绑定函数指针 ---- */
    imu->init            = mpu6050_init;
    imu->reset           = mpu6050_reset;
    imu->who_am_i        = mpu6050_who_am_i;

    imu->read_all_raw    = mpu6050_read_all_raw;
    imu->read_accel_raw  = mpu6050_read_accel_raw;
    imu->read_gyro_raw   = mpu6050_read_gyro_raw;
    imu->read_temp_raw   = mpu6050_read_temp_raw;

    imu->set_gyro_range  = mpu6050_set_gyro_range;
    imu->set_accel_range = mpu6050_set_accel_range;
    imu->set_dlpf        = mpu6050_set_dlpf;
    imu->set_sample_rate = mpu6050_set_sample_rate;

    imu->accel_to_g      = mpu6050_accel_to_g;
    imu->gyro_to_dps     = mpu6050_gyro_to_dps;
    imu->temp_to_c       = mpu6050_temp_to_c;

    imu->int_handler     = mpu6050_int_handler;
    imu->is_data_ready   = mpu6050_is_data_ready;

    /* ---- 5. 执行硬件初始化 ---- */
    imu->init(imu);
}
