/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"     //MQ135
#include "i2c.h"
#include "rtc.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "LCD_Driver.h"
#include "delay.h"
#include "stdio.h"
#include "AHT20.h"
#include "bmp280.h"
#include "max30102.h"
#include "max30102_fir.h"
#include "string.h"
#include "esp8266_onenet.h"
#include "common.h"
#include "mqttkit.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// 修复：添加OneNet上传间隔定义（30秒上传一次）
#define ONENET_UPLOAD_INTERVAL_MS  30000U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */


/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
RTC_TimeTypeDef sTime;
RTC_DateTypeDef sDate;



// MAX30102 ??
uint16_t HeartRate = 0;
float SpO2 = 0;
float max30102_data[2] = {0};
float fir_output[2] = {0};
uint8_t data_ready = 0;
char lcd_buf[32];

//MQ135
uint32_t mq135_adc_value = 0;      // ADC??? (0-4095)
float mq135_voltage = 0.0f;        // ???????
uint8_t mq135_alarm = 0;           // DO?????? (0=??,1=??)

int16_t temperature,humidity;
static uint32_t last_onenet_upload = 0U;
uint8_t Uart2_RxData = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

void RTC_SetTime(uint8_t hh, uint8_t mm, uint8_t ss);
void RTC_GetNowTime(void);


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint16_t last_hr = 0;
static float last_spo2 = 0;

int fputc(int ch, FILE *f)
{
	HAL_UART_Transmit(&huart1, (uint8_t*)&ch, 1, 100);
	return ch;
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
	
	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */
	
	/* MCU Configuration--------------------------------------------------------*/
	
	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();
	
	/* USER CODE BEGIN Init */
	
	/* USER CODE END Init */
	
	/* Configure the system clock */
	SystemClock_Config();
	
	/* USER CODE BEGIN SysInit */
	
	/* USER CODE END SysInit */
	
	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_I2C1_Init();            // MAX30102
	MX_USART2_UART_Init();     // ESP-01S
	MX_USART3_UART_Init();
	MX_I2C2_Init();            // AHT20 + BMP280
	MX_RTC_Init();
	MX_USART1_UART_Init();
	MX_ADC1_Init();            // MQ135    ???ADC1 (PA0)
	/* USER CODE BEGIN 2 */
	
	HAL_ADC_Start(&hadc1);     // ??ADC??
	
	RTC_SetTime(14, 30, 0);
	
	// LCD ???
	Lcd_Init();
	Lcd_Clear(BLACK);
	// 修复1：使用正确的UART接口 - ESP-01S连接在USART2上，不是USART3
	ESP8266_OneNET_Init(&huart2);
	// ??????
	
	
	// AHT20 ???
	AHT20_Init();
	LCD_Show_String(0, 0, "AHT20 OK");
	HAL_Delay(500);
	
	// BMP280 ???(???)
	uint8_t bmp_ok = 0;
	for (int i = 0; i < 3; i++)
	{
		if (BMP280_Init())
		{
			bmp_ok = 1;
			break;
		}
		HAL_Delay(100);
	}
	
	if (bmp_ok)
		LCD_Show_String(0, 16, "BMP280 OK");
	else
		LCD_Show_String(0, 16, "BMP280 ERR");
	HAL_Delay(500);
	
	// MAX30102 ???
	LCD_Show_String(0, 32, "MAX30102 INIT...");
	max30102_init();
	max30102_fir_init();
	HAL_Delay(500);
	
	// ??PART_ID??
	uint8_t part_id = 0;
	max30102_i2c_read(0xFF, &part_id, 1);
	sprintf(lcd_buf, "ID:0x%02X", part_id);
	LCD_Show_String(0, 48, lcd_buf);
	HAL_Delay(1000);
	
	if(part_id == 0x15)
		LCD_Show_String(0, 64, "MAX30102 OK");
	else
		LCD_Show_String(0, 64, "MAX30102 ERR");
	
	
	// ??MAX30102 FIFO
	cache_counter = 0;
	
	
	HAL_Delay(1500);
	Lcd_Clear(BLACK);
	
	
	float temp, humi;
	float press_hPa = 0.0f;
	float bmp_temp_c = 0.0f;
	
	/* USER CODE END 2 */
	
	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	
	//ESP8266_Init();					//???ESP8266,??wifi
	//while(OneNet_DevLink())			//??OneNET
	//HAL_Delay(1000);
	//printf("ONENET OK");
	
	while (1)
	{AHT20_Read(&temp, &humi);
		
		// ??? OneNET ???????
		temperature = (int16_t)temp;
		humidity = (int16_t)humi;
		
		BMP280_Read(&press_hPa, &bmp_temp_c);
		(void)bmp_temp_c;
		RTC_GetNowTime();
		
		//MQ135
		// ??PA0?ADC?
		if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
		{
			mq135_adc_value = HAL_ADC_GetValue(&hadc1);
			mq135_voltage = mq135_adc_value * 3.3f / 4096.0f;
		}
		// ??PB0????? (????,?????????)
		mq135_alarm = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0);
		
		
		// ??MAX30102??
		data_ready = MAX30102_Get_DATA(&HeartRate, &SpO2, max30102_data, fir_output);
		
		// CH340 ????(??)
		char uart_buf[64];
		sprintf(uart_buf, "TEMP:%.1f,HUMI:%.1f,MQ135:%.2fV,ALARM:%d\r\n", temp, humi, mq135_voltage, mq135_alarm == 0 ? 1 : 0);
		HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), 100);
		
		
		// LCD ??(???,??????)
		sprintf(lcd_buf, "TIME:%02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds);
		LCD_Show_String(0, 0, lcd_buf);
		
		sprintf(lcd_buf, "TEMP:%.1f C", temp);
		LCD_Show_String(0, 16, lcd_buf);
		
		sprintf(lcd_buf, "HUMI:%.1f %%", humi);
		LCD_Show_String(0, 32, lcd_buf);
		
		sprintf(lcd_buf, "PRS:%.1f hPa", press_hPa);
		LCD_Show_String(0, 48, lcd_buf);
		
		if (HeartRate < 1u)
		{
			last_hr = 0u;
			sprintf(lcd_buf, "HR: ---bpm");
		}
		else if (HeartRate >= MAX30102_HR_VALID_LO && HeartRate <= MAX30102_HR_VALID_HI)
		{
			last_hr = HeartRate;
			sprintf(lcd_buf, "HR:%3d bpm", HeartRate);
		}
		else if (last_hr >= MAX30102_HR_VALID_LO && last_hr <= MAX30102_HR_VALID_HI)
			sprintf(lcd_buf, "HR:%3d~bpm", last_hr);
		else
			sprintf(lcd_buf, "HR: ---bpm");
		LCD_Show_String(0, 64, lcd_buf);
		
		if (SpO2 < 1.0f)
		{
			last_spo2 = 0.0f;
			sprintf(lcd_buf, "SPO2: ---%%");
		}
		else if (SpO2 >= 65.0f && SpO2 <= 100.0f)
		{
			last_spo2 = SpO2;
			sprintf(lcd_buf, "SPO2:%3.0f %%", SpO2);
		}
		else if (last_spo2 >= 65.0f && last_spo2 <= 100.0f)
			sprintf(lcd_buf, "SPO2:%3.0f~%%", last_spo2);
		else
			sprintf(lcd_buf, "SPO2: ---%%");
		LCD_Show_String(0, 80, lcd_buf);
		
		//MQ135
		sprintf(lcd_buf, "MQ135:%.2fV", mq135_voltage);
		LCD_Show_String(0, 96, lcd_buf);
		if (mq135_alarm == 0)
			LCD_Show_String(0, 112, "AIR:POOR");
		else
			LCD_Show_String(0, 112, "AIR:GOOD");
		
		LCD_Show_String(0, 128, "S:");
		LCD_Show_Num(16, 128, ESP8266_OneNET_GetStatus(), 1);
		LCD_Show_String(32, 128, "E:");
		LCD_Show_Num(48, 128, ESP8266_OneNET_GetLastError(), 2);
		
		// 修复2：定期调用任务函数处理连接
		ESP8266_OneNET_Task();

        // 修复3：定期上传数据到OneNET
        if ((HAL_GetTick() - last_onenet_upload) >= ONENET_UPLOAD_INTERVAL_MS)
        {
            last_onenet_upload = HAL_GetTick();
            (void)ESP8266_OneNET_PostTempHumi((float)temperature, (float)humidity);
        }
		HAL_Delay(100);
		/* USER CODE END WHILE */
		
		
				
		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
	
	HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);
	
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSE;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.LSEState = RCC_LSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}
	
	HAL_Delay(100);
	
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
	| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
	
	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
	{
		Error_Handler();
	}
}

/* USER CODE BEGIN 4 */
// ==================== RTC ?? ====================
void RTC_SetTime(uint8_t hh, uint8_t mm, uint8_t ss)
{
	RTC_TimeTypeDef sTimeSet = {0};
	sTimeSet.Hours = hh;
	sTimeSet.Minutes = mm;
	sTimeSet.Seconds = ss;
	sTimeSet.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sTimeSet.StoreOperation = RTC_STOREOPERATION_RESET;
	
	HAL_RTC_SetTime(&hrtc, &sTimeSet, RTC_FORMAT_BIN);
}

void RTC_GetNowTime(void)
{
	HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
