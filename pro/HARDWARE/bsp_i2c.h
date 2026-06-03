#ifndef __BSP_I2C_H
#define	__BSP_I2C_H

#include "stm32g4xx_hal.h"

/*********************软件IIC使用的宏****************************/
#define Soft_I2C_SCL_PORT    GPIOA
#define Soft_I2C_SCL_PIN     GPIO_PIN_15

#define Soft_I2C_SDA_PORT    GPIOB
#define Soft_I2C_SDA_PIN     GPIO_PIN_7
//
#define Soft_I2C_SCL_1()      HAL_GPIO_WritePin(Soft_I2C_SCL_PORT, Soft_I2C_SCL_PIN, GPIO_PIN_SET)
#define Soft_I2C_SCL_0()      HAL_GPIO_WritePin(Soft_I2C_SCL_PORT, Soft_I2C_SCL_PIN, GPIO_PIN_RESET)
#define Soft_I2C_SDA_1()      HAL_GPIO_WritePin(Soft_I2C_SDA_PORT, Soft_I2C_SDA_PIN, GPIO_PIN_SET)
#define Soft_I2C_SDA_0()      HAL_GPIO_WritePin(Soft_I2C_SDA_PORT, Soft_I2C_SDA_PIN, GPIO_PIN_RESET)

/*等待超时时间*/
//#define I2CT_FLAG_TIMEOUT         ((uint32_t)0x1000)
//#define I2CT_LONG_TIMEOUT         ((uint32_t)(10 * I2CT_FLAG_TIMEOUT))


void I2C_Bus_Init(void);

void Set_I2C_Retry(unsigned short ml_sec);
unsigned short Get_I2C_Retry(void);
int Sensors_I2C_ReadRegister(unsigned char Address, unsigned char RegisterAddr, 
                                          unsigned short RegisterLen, unsigned char *RegisterValue);
int Sensors_I2C_WriteRegister(unsigned char Address, unsigned char RegisterAddr, 
                                           unsigned short RegisterLen, const unsigned char *RegisterValue);

#endif
