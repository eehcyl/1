#ifndef __LCD_DRIVER_H
#define __LCD_DRIVER_H

#include "main.h"

// ??
#define RED     0xf800
#define GREEN   0x07e0
#define BLUE    0x001f
#define WHITE   0xffff
#define BLACK   0x0000
#define YELLOW  0xFFE0

// ==================== HAL ?????? ====================
#define LCD_SCL_SET     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET)
#define LCD_SCL_CLR     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET)

#define LCD_SDA_SET     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET)
#define LCD_SDA_CLR     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET)

#define LCD_CS_SET      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET)
#define LCD_CS_CLR      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET)

#define LCD_RST_SET     HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET)
#define LCD_RST_CLR     HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET)

#define LCD_RS_SET      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET)
#define LCD_RS_CLR      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET)

#define X_MAX_PIXEL	    128
#define Y_MAX_PIXEL	    160

// ????
void Lcd_Init(void);
void Lcd_Clear(uint16_t Color);
void Gui_DrawPoint(uint16_t x,uint16_t y,uint16_t Data);
void Lcd_SetRegion(uint16_t x_start,uint16_t y_start,uint16_t x_end,uint16_t y_end);
void LCD_WriteData_16Bit(uint16_t Data);
void LCD_Show_String(uint16_t x,uint16_t y,char *str);
void LCD_ShowTime(uint16_t x,uint16_t y,uint8_t h,uint8_t m,uint8_t s);
void LCD_Show_Num(uint16_t x, uint16_t y, int num, uint8_t len);
#endif
