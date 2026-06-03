#ifndef INC_AHT20_H_
#define INC_AHT20_H_

#include "main.h"
#include "i2c.h"

#define AHT20_ADDRESS	0x70
/* 与 BMP280 共用 I2C2（PC4=SCL、PA8=SDA） */
#define AHT20_I2C		hi2c2
#define AHT20_TIMEOUT	100

void AHT20_Init(void);
void AHT20_Read(float *Temperature, float *Humidity);

#endif
