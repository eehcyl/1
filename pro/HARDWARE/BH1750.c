#include "BH1750.h"

#define BH1750_TIMEOUT_MS   100

static HAL_StatusTypeDef BH1750_WriteCmd(BH1750_t *dev, uint8_t cmd);
static uint32_t BH1750_GetMeasureDelayMs(BH1750_Mode_t mode);

static HAL_StatusTypeDef BH1750_WriteCmd(BH1750_t *dev, uint8_t cmd)
{
    if (dev == NULL || dev->hi2c == NULL)
    {
        return HAL_ERROR;
    }

    return HAL_I2C_Master_Transmit(dev->hi2c, dev->devAddr, &cmd, 1, BH1750_TIMEOUT_MS);
}

static uint32_t BH1750_GetMeasureDelayMs(BH1750_Mode_t mode)
{
    switch (mode)
    {
        case BH1750_CONT_L_RES_MODE:
        case BH1750_ONE_TIME_L_RES_MODE:
            return 24;   // ???????24ms
        case BH1750_CONT_H_RES_MODE:
        case BH1750_CONT_H_RES_MODE2:
        case BH1750_ONE_TIME_H_RES_MODE:
        case BH1750_ONE_TIME_H_RES_MODE2:
        default:
            return 180;  // ???????180ms
    }
}

HAL_StatusTypeDef BH1750_Init(BH1750_t *dev, I2C_HandleTypeDef *hi2c, uint16_t devAddr)
{
    if (dev == NULL || hi2c == NULL)
    {
        return HAL_ERROR;
    }

    dev->hi2c = hi2c;
    dev->devAddr = devAddr;
    dev->mode = BH1750_ONE_TIME_H_RES_MODE;   // ????????

    if (BH1750_IsReady(dev) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (BH1750_PowerOn(dev) != HAL_OK)
    {
        return HAL_ERROR;
    }

    HAL_Delay(10);

    if (BH1750_ResetData(dev) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef BH1750_IsReady(BH1750_t *dev)
{
    if (dev == NULL || dev->hi2c == NULL)
    {
        return HAL_ERROR;
    }

    return HAL_I2C_IsDeviceReady(dev->hi2c, dev->devAddr, 3, BH1750_TIMEOUT_MS);
}

HAL_StatusTypeDef BH1750_PowerOn(BH1750_t *dev)
{
    return BH1750_WriteCmd(dev, BH1750_POWER_ON);
}

HAL_StatusTypeDef BH1750_PowerDown(BH1750_t *dev)
{
    return BH1750_WriteCmd(dev, BH1750_POWER_DOWN);
}

HAL_StatusTypeDef BH1750_ResetData(BH1750_t *dev)
{
    HAL_StatusTypeDef ret;

    ret = BH1750_PowerOn(dev);
    if (ret != HAL_OK)
    {
        return ret;
    }

    return BH1750_WriteCmd(dev, BH1750_RESET);
}

HAL_StatusTypeDef BH1750_SetMode(BH1750_t *dev, BH1750_Mode_t mode)
{
    HAL_StatusTypeDef ret;

    if (dev == NULL)
    {
        return HAL_ERROR;
    }

    ret = BH1750_PowerOn(dev);
    if (ret != HAL_OK)
    {
        return ret;
    }

    ret = BH1750_WriteCmd(dev, (uint8_t)mode);
    if (ret == HAL_OK)
    {
        dev->mode = mode;
    }

    return ret;
}

HAL_StatusTypeDef BH1750_ReadRaw(BH1750_t *dev, uint16_t *rawData)
{
    HAL_StatusTypeDef ret;
    uint8_t rxBuf[2] = {0};

    if (dev == NULL || rawData == NULL)
    {
        return HAL_ERROR;
    }

    /*
     * ????“????”???????
     * ???????,????????????;
     * ????????,???? BH1750_SetMode(...CONT_H_RES_MODE),
     * ?????????
     */
    ret = BH1750_SetMode(dev, dev->mode);
    if (ret != HAL_OK)
    {
        return ret;
    }

    HAL_Delay(BH1750_GetMeasureDelayMs(dev->mode));

    ret = HAL_I2C_Master_Receive(dev->hi2c, dev->devAddr, rxBuf, 2, BH1750_TIMEOUT_MS);
    if (ret != HAL_OK)
    {
        return ret;
    }

    *rawData = ((uint16_t)rxBuf[0] << 8) | rxBuf[1];
    return HAL_OK;
}

HAL_StatusTypeDef BH1750_ReadLux(BH1750_t *dev, float *lux)
{
    HAL_StatusTypeDef ret;
    uint16_t raw = 0;

    if (dev == NULL || lux == NULL)
    {
        return HAL_ERROR;
    }

    ret = BH1750_ReadRaw(dev, &raw);
    if (ret != HAL_OK)
    {
        return ret;
    }

    /*
     * ?? MTreg = 69 ?,????:
     * lux = raw / 1.2
     */
    *lux = (float)raw / 1.2f;

    return HAL_OK;
}