#include "bsp_i2c.h"
#include "stm32g4xx_hal.h"
#include "delay.h"
#include "stdio.h"

#define mdelay HAL_Delay

#define Soft_I2C_SDA_STATE()  HAL_GPIO_ReadPin(Soft_I2C_SDA_PORT, Soft_I2C_SDA_PIN)
#define Soft_I2C_DELAY 			Soft_I2C_Delay(1000)
#define Soft_I2C_NOP			Soft_I2C_Delay(10)

#define Soft_I2C_READY		0x00
#define Soft_I2C_BUS_BUSY	0x01
#define Soft_I2C_BUS_ERROR	0x02

#define Soft_I2C_NACK  	    0x00
#define Soft_I2C_ACK		0x01

#define I2C_Direction_Transmitter  0
#define I2C_Direction_Receiver     1

static void Soft_I2C_Delay(uint32_t dly)
{
	while(--dly);	//dly=100: 8.75us; dly=100: 85.58 us (SYSCLK=72MHz)
}

static unsigned short RETRY_IN_MLSEC  = 55;

/**
  * @brief  设置iic重试时间
  * @param  ml_sec：重试的时间，单位毫秒
  * @retval 重试的时间，单位毫秒
  */
void Set_I2C_Retry(unsigned short ml_sec)
{
    RETRY_IN_MLSEC = ml_sec;
}

/**
  * @brief  获取设置的iic重试时间
  * @param  none
  * @retval none
  */
unsigned short Get_I2C_Retry(void)
{
    return RETRY_IN_MLSEC;
}

static void Soft_I2C_Configuration(void)//SCL接PB6，SDA接PB7
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // PA15 SCL
    GPIO_InitStruct.Pin = Soft_I2C_SCL_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(Soft_I2C_SCL_PORT, &GPIO_InitStruct);

    // PB7 SDA
    GPIO_InitStruct.Pin = Soft_I2C_SDA_PIN;
    HAL_GPIO_Init(Soft_I2C_SDA_PORT, &GPIO_InitStruct);

    Soft_I2C_SCL_1();
    Soft_I2C_SDA_1();
    Soft_I2C_DELAY;
}

void I2C_Bus_Init(void)
{
	Set_I2C_Retry(5);
	Soft_I2C_Configuration();
}

static uint8_t Soft_I2C_START(void)
{
	 Soft_I2C_SDA_1();
    Soft_I2C_NOP;

    Soft_I2C_SCL_1();
    Soft_I2C_NOP;

    if(!Soft_I2C_SDA_STATE())
        return Soft_I2C_BUS_BUSY;

    Soft_I2C_SDA_0();
    Soft_I2C_NOP;

    Soft_I2C_SCL_0();
    Soft_I2C_NOP;

    if(Soft_I2C_SDA_STATE())
        return Soft_I2C_BUS_ERROR;

    return Soft_I2C_READY;
}

static void Soft_I2C_STOP(void)
{
 	  Soft_I2C_SDA_0();
    Soft_I2C_NOP;

    Soft_I2C_SCL_1();
    Soft_I2C_NOP;

    Soft_I2C_SDA_1();
    Soft_I2C_NOP;
}

static void Soft_I2C_SendACK(void)
{
 	  Soft_I2C_SDA_0();
    Soft_I2C_NOP;
    Soft_I2C_SCL_1();
    Soft_I2C_NOP;
    Soft_I2C_SCL_0();
    Soft_I2C_NOP;
}

static void Soft_I2C_SendNACK(void)
{
	Soft_I2C_SDA_1();
	Soft_I2C_NOP;
	Soft_I2C_SCL_1();
	Soft_I2C_NOP;
	Soft_I2C_SCL_0();
	Soft_I2C_NOP;
}

/**
  * @brief  等待应答信号到来
  * @retval 返回值：1，接收应答失败
	*									0，接收应答成功
  */
uint8_t Soft_I2C_Wait_Ack(void)
{
	uint8_t ucErrTime=0;

	Soft_I2C_SDA_1();
	Soft_I2C_NOP;
	Soft_I2C_SCL_1();
	Soft_I2C_NOP;

	while(Soft_I2C_SDA_STATE())
	{
		ucErrTime ++;
		if(ucErrTime > 250)
		{
			Soft_I2C_STOP();
			return Soft_I2C_BUS_ERROR;
		}
	}
	Soft_I2C_SCL_0();//时钟输出0
	return 0;
}

static uint8_t Soft_I2C_SendByte(uint8_t soft_i2c_data)
{
 	uint8_t i;

	Soft_I2C_SCL_0();
 	for(i=0; i<8; i++)
 	{
  		if(soft_i2c_data & 0x80)
            Soft_I2C_SDA_1();
   		else
            Soft_I2C_SDA_0();
  		soft_i2c_data <<= 1;
  		Soft_I2C_NOP;

  		Soft_I2C_SCL_1();
  		Soft_I2C_NOP;
  		Soft_I2C_SCL_0();
  		Soft_I2C_NOP;
 	}
	return Soft_I2C_Wait_Ack();
}

static uint8_t Soft_I2C_ReceiveByte(void)
{
	uint8_t i,soft_i2c_data=0;
    Soft_I2C_SDA_1();
    Soft_I2C_SCL_0();

    for(i=0; i<8; i++)
    {
        Soft_I2C_SCL_1();
        Soft_I2C_NOP;
        soft_i2c_data <<= 1;
        if(Soft_I2C_SDA_STATE())
            soft_i2c_data |= 0x01;
        Soft_I2C_SCL_0();
        Soft_I2C_NOP;
    }
    Soft_I2C_SendNACK();
    return soft_i2c_data;
}

static uint8_t Soft_I2C_ReceiveByte_WithACK(void)
{
	 uint8_t i,soft_i2c_data=0;
    Soft_I2C_SDA_1();
    Soft_I2C_SCL_0();

    for(i=0; i<8; i++)
    {
        Soft_I2C_SCL_1();
        Soft_I2C_NOP;
        soft_i2c_data <<= 1;
        if(Soft_I2C_SDA_STATE())
            soft_i2c_data |= 0x01;
        Soft_I2C_SCL_0();
        Soft_I2C_NOP;
    }
    Soft_I2C_SendACK();
    return soft_i2c_data;
}

static uint8_t Soft_DMP_I2C_Write(uint8_t dev, uint8_t reg, uint8_t len, uint8_t *buf)
{
    uint8_t i, res;
    Soft_I2C_START();
    res = Soft_I2C_SendByte((dev<<1) | I2C_Direction_Transmitter);
    if(res) return res;
    res = Soft_I2C_SendByte(reg);
    if(res) return res;
    for(i=0;i<len;i++)
    {
        res = Soft_I2C_SendByte(buf[i]);
        if(res) return res;
    }
    Soft_I2C_STOP();
    return 0;
}

static uint8_t Soft_DMP_I2C_Read(uint8_t soft_dev_addr, uint8_t soft_reg_addr, uint8_t soft_i2c_len,unsigned char *soft_i2c_data_buf)
{
	uint8_t result;

	Soft_I2C_START();
	result  = Soft_I2C_SendByte(soft_dev_addr << 1 | I2C_Direction_Transmitter);
	if(result != 0) return result;

	result = Soft_I2C_SendByte(soft_reg_addr);
   //printf("addr:0x%x\n",result);
	if(result != 0) return result;

	Soft_I2C_START();
	result = Soft_I2C_SendByte(soft_dev_addr << 1 | I2C_Direction_Receiver);
	if(result != 0) return result;

    while (soft_i2c_len)
	{
		if (soft_i2c_len == 1)
			*soft_i2c_data_buf = Soft_I2C_ReceiveByte();
        else
        	*soft_i2c_data_buf = Soft_I2C_ReceiveByte_WithACK();
        soft_i2c_data_buf ++;
        soft_i2c_len --;
    }
	Soft_I2C_STOP();
    return 0x00;
}

/**
  * @brief  向IIC设备的寄存器连续写入数据，带超时重试设置，供mpu接口调用
  * @param  Address: IIC设备地址
  * @param  RegisterAddr: 寄存器地址
  * @param  RegisterLen: 要写入数据的长度
  * @param  RegisterValue: 要指向写入数据的指针
  * @retval 0正常，非0异常
  */
int Sensors_I2C_WriteRegister(unsigned char slave_addr,
                                        unsigned char reg_addr,
                                        unsigned short len,
                                        const unsigned char *data_ptr)
{
    char retries = 0;
    int ret = 0;
    unsigned short retry_in_mlsec = Get_I2C_Retry();

tryWriteAgain:
    ret = 0;
    ret = Soft_DMP_I2C_Write( slave_addr, reg_addr, len, ( unsigned char *)data_ptr);

    if(ret && retry_in_mlsec)
    {
        if( retries++ > 4 )
            return ret;

        mdelay(retry_in_mlsec);
        goto tryWriteAgain;
    }
    return ret;
}

/**
  * @brief  向IIC设备的寄存器连续读出数据,带超时重试设置，供mpu接口调用
  * @param  Address: IIC设备地址
  * @param  RegisterAddr: 寄存器地址
  * @param  RegisterLen: 要读取的数据长度
  * @param  RegisterValue: 指向存储读出数据的指针
  * @retval 0正常，非0异常
  */
int Sensors_I2C_ReadRegister(unsigned char slave_addr,
                                       unsigned char reg_addr,
                                       unsigned short len,
                                       unsigned char *data_ptr)
{
    char retries = 0;
    int ret = 0;
    unsigned short retry_in_mlsec = Get_I2C_Retry();

tryReadAgain:
    ret = 0;
    ret = Soft_DMP_I2C_Read( slave_addr, reg_addr, len, ( unsigned char *)data_ptr);

    if(ret && retry_in_mlsec)
    {
        if( retries++ > 4 )
            return ret;

        mdelay(retry_in_mlsec);
        goto tryReadAgain;
    }
    return ret;
}
