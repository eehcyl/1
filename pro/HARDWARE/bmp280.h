#ifndef BMP280_H
#define BMP280_H

#include "main.h"
#include "i2c.h"
#include <stdint.h>

/* 与 AHT20 共用 I2C2 */
#define BMP280_I2C          hi2c2
#define BMP280_TIMEOUT_MS   80u

/* 7-bit 地址（二合一模块通常固定为 0x76） */
#define BMP280_ADDR7_PRIMARY    0x76u

/* 返回 1 成功，0 失败 */
uint8_t BMP280_Init(void);

/* 读取气压与温度 */
void BMP280_Read(float *press_hPa, float *temp_bmp_c);

#endif
