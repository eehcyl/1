#ifndef __DELAY_H
#define __DELAY_H 			   
#include "stm32g4xx.h"

void delay_init(unsigned char SYSCLK);
void delay_ms(unsigned short int nms);
void delay_us(unsigned int nus);

#endif





























