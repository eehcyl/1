#ifndef __DS1302_H
#define __DS1302_H

#include "stm32g4xx_hal.h"

// ????? PB13/PB14/PB15 ??
#define DS1302_RST_PIN    GPIO_PIN_13
#define DS1302_RST_PORT   GPIOB

#define DS1302_SCLK_PIN   GPIO_PIN_15
#define DS1302_SCLK_PORT  GPIOB

#define DS1302_DAT_PIN    GPIO_PIN_14
#define DS1302_DAT_PORT   GPIOB

// ?????
typedef struct
{
    uint8_t year;   // ??(00-99)
    uint8_t mon;    // ??(1-12)
    uint8_t day;    // ??(1-31)
    uint8_t week;   // ??(1-7)
    uint8_t hour;   // ??(0-23)
    uint8_t min;    // ??(0-59)
    uint8_t sec;    // ??(0-59)
} DS1302_TIME;

void DS1302_Init(void);
void DS1302_SetTime(DS1302_TIME *t);
void DS1302_ReadTime(DS1302_TIME *t);

#endif
