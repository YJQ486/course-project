/**
 * @file    bsp_mpu6050.c
 * @brief   MPU6050 驱动实现（硬件 I2C1，句柄 hi2c1 由 CubeMX 生成）
 */
#include "bsp_mpu6050.h"
#include "app_config.h"

extern I2C_HandleTypeDef hi2c1;     /* CubeMX 生成 */

#define MPU_ADDR        (0x68 << 1) /* AD0=0 时地址 0x68，左移 1 位给 HAL */

/* 寄存器地址 */
#define REG_SMPLRT_DIV      0x19
#define REG_CONFIG          0x1A
#define REG_GYRO_CONFIG     0x1B
#define REG_ACCEL_CONFIG    0x1C
#define REG_MOT_THR         0x1F
#define REG_MOT_DUR         0x20
#define REG_INT_PIN_CFG     0x37
#define REG_INT_ENABLE      0x38
#define REG_INT_STATUS      0x3A
#define REG_ACCEL_XOUT_H    0x3B
#define REG_SIGNAL_PATH_RESET 0x68
#define REG_MOT_DETECT_CTRL 0x69
#define REG_PWR_MGMT_1      0x6B
#define REG_WHO_AM_I        0x75

static uint8_t mpu_write(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(&hi2c1, MPU_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                             &val, 1, 100);
}

static uint8_t mpu_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c1, MPU_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                            buf, len, 100);
}

uint8_t MPU6050_Init(void)
{
    uint8_t id = 0;
    HAL_Delay(50);
    if (mpu_read(REG_WHO_AM_I, &id, 1) != HAL_OK) return 1;
    if (id != 0x68) return 2;               /* 设备识别失败 */

    if (mpu_write(REG_PWR_MGMT_1, 0x00) != HAL_OK) return 3; /* 解除休眠 */
    mpu_write(REG_SMPLRT_DIV, 0x07);        /* 采样分频 -> 1kHz/8 = 125Hz */
    mpu_write(REG_CONFIG, 0x06);            /* DLPF ~5Hz 低通，利于计步 */
    mpu_write(REG_GYRO_CONFIG, 0x18);       /* ±2000 dps */
    mpu_write(REG_ACCEL_CONFIG, 0x01);      /* ±2g, 高通 5Hz（bit2:0=001） */
    return 0;
}

uint8_t MPU6050_Read(MPU_Data_t *out)
{
    uint8_t b[14];
    if (mpu_read(REG_ACCEL_XOUT_H, b, 14) != HAL_OK) return 1;
    out->ax = (int16_t)((b[0]  << 8) | b[1]);
    out->ay = (int16_t)((b[2]  << 8) | b[3]);
    out->az = (int16_t)((b[4]  << 8) | b[5]);
    /* b[6],b[7] = 温度，跳过 */
    out->gx = (int16_t)((b[8]  << 8) | b[9]);
    out->gy = (int16_t)((b[10] << 8) | b[11]);
    out->gz = (int16_t)((b[12] << 8) | b[13]);
    return 0;
}

uint8_t MPU6050_EnableMotionInt(uint8_t thr, uint8_t dur)
{
    /* MPU6050 wake-on-motion 标准时序（eluke.nl / kriswiner 等多方验证）。
     * 此前“抬腕怎么动都唤不醒、按键才行”的两个真正根因（EXTI/STOP 链路本身
     * 没问题——按键能唤醒即证明——坏在 MPU 根本不产生 INT 脉冲）：
     *
     *  ① 缺信号通路复位：不写 SIGNAL_PATH_RESET，运动检测前置的高通滤波器
     *     残留旧状态，阈值比较器不翻转 → INT 从不产生。这是本时序的必备首步。
     *  ② INT 用了 ~50us 脉冲模式：脉冲太短，经杜邦线/输入电容后 STM32 EXTI
     *     常常抓不到上升沿。改锁存模式：INT 触发后保持高电平直到读 0x3A 清除，
     *     给 EXTI 一个稳定、明确的电平+边沿，唤醒可靠得多。
     *     （代价是 INT 高电平会保持，故进 STOP 前、唤醒后、以及清醒时
     *      Task_Sensor 每帧都读一次 0x3A 重新“布防”，见 app_tasks.c。） */
    mpu_write(REG_PWR_MGMT_1, 0x00);          /* 唤醒、内部时钟 */
    mpu_write(REG_SIGNAL_PATH_RESET, 0x07);   /* ①复位加速度/陀螺/温度信号通路 */
    HAL_Delay(2);
    /* DLPF 放宽到 44Hz：Init 为计步配的 5Hz 低通会把抬腕动作能量滤掉；
     * 计步侧靠软件 8 点滑动平均平滑，不依赖硬件重低通。 */
    mpu_write(REG_CONFIG, 0x03);
    mpu_write(REG_ACCEL_CONFIG, 0x01);        /* ±2g，加速度高通 5Hz（运动检测用） */
    mpu_write(REG_MOT_THR, thr);              /* 运动阈值（1 LSB≈1mg） */
    mpu_write(REG_MOT_DUR, dur);              /* 运动持续（1 LSB=1ms） */
    mpu_write(REG_MOT_DETECT_CTRL, 0x15);     /* 加速度上电延时 + 运动计数器递减 */
    mpu_write(REG_INT_PIN_CFG, 0x20);         /* ②锁存、高有效、推挽；读 0x3A 清除 */
    mpu_write(REG_INT_ENABLE, 0x40);          /* 使能 Motion 中断 */
    MPU6050_ReadIntStatus();                  /* 清一次，从干净状态起步 */
    return 0;
}

/* 读 INT_STATUS(0x3A) 会清除运动中断标志；进 STOP 前调用以保证 INT 线为低 */
uint8_t MPU6050_ReadIntStatus(void)
{
    uint8_t st = 0;
    mpu_read(REG_INT_STATUS, &st, 1);
    return st;
}
