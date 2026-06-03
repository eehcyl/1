#include "bmp280.h"

#define REG_ID          0xD0u
#define REG_RESET       0xE0u
#define REG_STATUS      0xF3u
#define REG_CTRL_MEAS   0xF4u
#define REG_CONFIG      0xF5u
#define REG_PRESS_MSB   0xF7u
#define REG_CALIB0      0x88u

#define BMP280_CHIP_ID  0x58u

/* forced 模式：单次测量，osrs_t/osrs_p 各 x1，转换约 6ms */
#define CTRL_MEAS_FORCED_OSRS1    0x25u

static uint8_t g_addr7 = BMP280_ADDR7_PRIMARY;
static uint16_t g_i2c_addr8;

static uint16_t dig_T1;
static int16_t dig_T2, dig_T3;
static uint16_t dig_P1;
static int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static int32_t t_fine;

static HAL_StatusTypeDef bmp280_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(&BMP280_I2C, g_i2c_addr8, reg, I2C_MEMADD_SIZE_8BIT, buf, len, BMP280_TIMEOUT_MS);
}

static HAL_StatusTypeDef bmp280_write_reg(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(&BMP280_I2C, g_i2c_addr8, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, BMP280_TIMEOUT_MS);
}

/* 温度补偿，返回 0.01°C 为单位的整数（例：2534 -> 25.34°C） */
static int32_t bmp280_comp_temp(int32_t adc_T)
{
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8;
}

/* 气压补偿，单位 Pa（整数） */
static uint32_t bmp280_comp_press(int32_t adc_P)
{
    int64_t var1;
    int64_t var2;
    int64_t p;

    var1 = (int64_t)t_fine - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;
    if (var1 == 0)
        return 0u;

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
    return (uint32_t)p;
}

uint8_t BMP280_Init(void)
{
    uint8_t id = 0;
    uint8_t cal[24];
    uint8_t retry = 0;

    /* 设置 I2C 地址 */
    g_addr7 = BMP280_ADDR7_PRIMARY;
    g_i2c_addr8 = (uint16_t)(g_addr7 << 1u);

    /* 最多重试 3 次 */
    for (retry = 0; retry < 3; retry++)
    {
        if (bmp280_read_regs(REG_ID, &id, 1u) == HAL_OK && id == BMP280_CHIP_ID)
            break;
        HAL_Delay(20);
    }
    
    if (retry >= 3)
        return 0u;

    /* 复位设备 */
    (void)bmp280_write_reg(REG_RESET, 0xB6u);
    HAL_Delay(50);

    /* 读取校准数据 */
    if (bmp280_read_regs(REG_CALIB0, cal, sizeof(cal)) != HAL_OK)
        return 0u;

    dig_T1 = (uint16_t)cal[0] | ((uint16_t)cal[1] << 8);
    dig_T2 = (int16_t)((uint16_t)cal[2] | ((uint16_t)cal[3] << 8));
    dig_T3 = (int16_t)((uint16_t)cal[4] | ((uint16_t)cal[5] << 8));
    dig_P1 = (uint16_t)cal[6] | ((uint16_t)cal[7] << 8);
    dig_P2 = (int16_t)((uint16_t)cal[8] | ((uint16_t)cal[9] << 8));
    dig_P3 = (int16_t)((uint16_t)cal[10] | ((uint16_t)cal[11] << 8));
    dig_P4 = (int16_t)((uint16_t)cal[12] | ((uint16_t)cal[13] << 8));
    dig_P5 = (int16_t)((uint16_t)cal[14] | ((uint16_t)cal[15] << 8));
    dig_P6 = (int16_t)((uint16_t)cal[16] | ((uint16_t)cal[17] << 8));
    dig_P7 = (int16_t)((uint16_t)cal[18] | ((uint16_t)cal[19] << 8));
    dig_P8 = (int16_t)((uint16_t)cal[20] | ((uint16_t)cal[21] << 8));
    dig_P9 = (int16_t)((uint16_t)cal[22] | ((uint16_t)cal[23] << 8));

    /* 配置：待机时间 0，滤波 0 */
    if (bmp280_write_reg(REG_CONFIG, 0x00u) != HAL_OK)
        return 0u;

    return 1u;
}

void BMP280_Read(float *press_hPa, float *temp_bmp_c)
{
    uint8_t raw[6];
    int32_t adc_P;
    int32_t adc_T;
    int32_t t_x100;
    uint32_t p_pa;

    if (press_hPa == NULL || temp_bmp_c == NULL)
        return;

    *press_hPa = 0.0f;
    *temp_bmp_c = 0.0f;

    /* 启动 forced 测量 */
    if (bmp280_write_reg(REG_CTRL_MEAS, CTRL_MEAS_FORCED_OSRS1) != HAL_OK)
        return;

    /* 等待测量完成（约 6-10ms） */
    HAL_Delay(15);

    /* 读取数据 */
    if (bmp280_read_regs(REG_PRESS_MSB, raw, 6u) != HAL_OK)
        return;

    adc_P = (int32_t)(((uint32_t)raw[0] << 12) | ((uint32_t)raw[1] << 4) | ((uint32_t)raw[2] >> 4));
    adc_T = (int32_t)(((uint32_t)raw[3] << 12) | ((uint32_t)raw[4] << 4) | ((uint32_t)raw[5] >> 4));

    t_x100 = bmp280_comp_temp(adc_T);
    *temp_bmp_c = (float)t_x100 / 100.0f;

    p_pa = bmp280_comp_press(adc_P);
    *press_hPa = (float)p_pa / 100.0f;
}
