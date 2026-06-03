#include "DS1302.h"
#include "delay.h"

// 简单延时函数，不依赖SysTick，适配16MHz HSI时钟
static void DS1302_Delay(void)
{
    volatile int i;
    for(i = 0; i < 50; i++);  // 增加延时，确保时序稳定
}

// 引脚操作宏
#define RST_H()  HAL_GPIO_WritePin(DS1302_RST_PORT, DS1302_RST_PIN, GPIO_PIN_SET)
#define RST_L()  HAL_GPIO_WritePin(DS1302_RST_PORT, DS1302_RST_PIN, GPIO_PIN_RESET)

#define SCLK_H() HAL_GPIO_WritePin(DS1302_SCLK_PORT, DS1302_SCLK_PIN, GPIO_PIN_SET)
#define SCLK_L() HAL_GPIO_WritePin(DS1302_SCLK_PORT, DS1302_SCLK_PIN, GPIO_PIN_RESET)

#define DAT_H()  HAL_GPIO_WritePin(DS1302_DAT_PORT, DS1302_DAT_PIN, GPIO_PIN_SET)
#define DAT_L()  HAL_GPIO_WritePin(DS1302_DAT_PORT, DS1302_DAT_PIN, GPIO_PIN_RESET)
#define DAT_RD() HAL_GPIO_ReadPin(DS1302_DAT_PORT, DS1302_DAT_PIN)

// 把DAT引脚设为输出模式
static void DAT_OUT(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = DS1302_DAT_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DS1302_DAT_PORT, &gpio);
}

// 把DAT引脚设为输入模式
static void DAT_IN(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = DS1302_DAT_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;  // 输入模式要用上拉
    HAL_GPIO_Init(DS1302_DAT_PORT, &gpio);
}

// 写一个字节到DS1302（低位在前）
static void DS1302_WriteByte(uint8_t dat)
{
    uint8_t i;
    DAT_OUT();
    for(i = 0; i < 8; i++)
    {
        SCLK_L();
        DS1302_Delay();
        if(dat & 0x01)
            DAT_H();
        else
            DAT_L();
        dat >>= 1;
        DS1302_Delay();
        SCLK_H();
        DS1302_Delay();
    }
}

// 从DS1302读一个字节（低位在前）
static uint8_t DS1302_ReadByte(void)
{
    uint8_t i, dat = 0;
    DAT_IN();
    for(i = 0; i < 8; i++)
    {
        SCLK_L();
        DS1302_Delay();
        if(DAT_RD())
            dat |= (0x01 << i);  // 低位在前，正确放置位
        DS1302_Delay();
        SCLK_H();
        DS1302_Delay();
    }
    return dat;
}

// 写数据到指定寄存器
static void DS1302_WriteReg(uint8_t addr, uint8_t dat)
{
    RST_L();
    DS1302_Delay();
    SCLK_L();
    DS1302_Delay();
    RST_H();
    DS1302_Delay();
    DS1302_WriteByte(addr);
    DS1302_WriteByte(dat);
    RST_L();
}

// 从指定寄存器读数据
static uint8_t DS1302_ReadReg(uint8_t addr)
{
    uint8_t d;
    RST_L();
    DS1302_Delay();
    SCLK_L();
    DS1302_Delay();
    RST_H();
    DS1302_Delay();
    DS1302_WriteByte(addr);
    DS1302_Delay();
    d = DS1302_ReadByte();
    RST_L();
    return d;
}

// BCD码转十进制
static uint8_t BCD_DEC(uint8_t val)
{
    return ((val >> 4) & 0x0F) * 10 + (val & 0x0F);
}

// 十进制转BCD码
static uint8_t DEC_BCD(uint8_t val)
{
    return ((val / 10) << 4) | (val % 10);
}

// DS1302初始化（只初始化硬件，不重置时间）
void DS1302_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // 配置RST和SCLK为推挽输出
    gpio.Pin = DS1302_RST_PIN | DS1302_SCLK_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);

    RST_L();
    SCLK_L();

    // 关闭写保护（不重置时间）
    DS1302_WriteReg(0x8E, 0x00);
}

// 设置时间
void DS1302_SetTime(DS1302_TIME *t)
{
    DS1302_WriteReg(0x8E, 0x00); // 关闭写保护

    DS1302_WriteReg(0x80, DEC_BCD(t->sec));
    DS1302_WriteReg(0x82, DEC_BCD(t->min));
    DS1302_WriteReg(0x84, DEC_BCD(t->hour));
    DS1302_WriteReg(0x86, DEC_BCD(t->day));
    DS1302_WriteReg(0x88, DEC_BCD(t->mon));
    DS1302_WriteReg(0x8A, DEC_BCD(t->week));
    DS1302_WriteReg(0x8C, DEC_BCD(t->year));

    DS1302_WriteReg(0x8E, 0x80); // 开启写保护
}

// 读取时间
void DS1302_ReadTime(DS1302_TIME *t)
{
    t->sec  = BCD_DEC(DS1302_ReadReg(0x81) & 0x7F);
    t->min  = BCD_DEC(DS1302_ReadReg(0x83) & 0x7F);
    t->hour = BCD_DEC(DS1302_ReadReg(0x85) & 0x3F);
    t->day  = BCD_DEC(DS1302_ReadReg(0x87) & 0x3F);
    t->mon  = BCD_DEC(DS1302_ReadReg(0x89) & 0x1F);
    t->week = BCD_DEC(DS1302_ReadReg(0x8B) & 0x07);
    t->year = BCD_DEC(DS1302_ReadReg(0x8D));
}
