#ifndef __BH1750_H__
#define __BH1750_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdint.h>

/* 
 * STM32 HAL ? I2C ????????1?????
 * BH1750 7-bit ??:
 *   ADDR = Low -> 0x23
 *   ADDR = High -> 0x5C
 */
#define BH1750_ADDR_LOW    (0x23 << 1)
#define BH1750_ADDR_HIGH   (0x5C << 1)

/* BH1750 ???? */
typedef enum
{
    BH1750_POWER_DOWN           = 0x00,
    BH1750_POWER_ON             = 0x01,
    BH1750_RESET                = 0x07,
    BH1750_CONT_H_RES_MODE      = 0x10,   // ????????1
    BH1750_CONT_H_RES_MODE2     = 0x11,   // ????????2
    BH1750_CONT_L_RES_MODE      = 0x13,   // ????????
    BH1750_ONE_TIME_H_RES_MODE  = 0x20,   // ????????1
    BH1750_ONE_TIME_H_RES_MODE2 = 0x21,   // ????????2
    BH1750_ONE_TIME_L_RES_MODE  = 0x23    // ????????
} BH1750_Mode_t;

typedef struct
{
    I2C_HandleTypeDef *hi2c;
    uint16_t devAddr;
    BH1750_Mode_t mode;
} BH1750_t;

/* API */
HAL_StatusTypeDef BH1750_Init(BH1750_t *dev, I2C_HandleTypeDef *hi2c, uint16_t devAddr);
HAL_StatusTypeDef BH1750_IsReady(BH1750_t *dev);
HAL_StatusTypeDef BH1750_PowerOn(BH1750_t *dev);
HAL_StatusTypeDef BH1750_PowerDown(BH1750_t *dev);
HAL_StatusTypeDef BH1750_ResetData(BH1750_t *dev);
HAL_StatusTypeDef BH1750_SetMode(BH1750_t *dev, BH1750_Mode_t mode);
HAL_StatusTypeDef BH1750_ReadRaw(BH1750_t *dev, uint16_t *rawData);
HAL_StatusTypeDef BH1750_ReadLux(BH1750_t *dev, float *lux);

#ifdef __cplusplus
}
#endif
#endif

