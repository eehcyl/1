#include "esp8266_onenet.h"

#include <stdio.h>
#include <string.h>

#define ESP8266_RX_BUFFER_SIZE     512U
#define ESP8266_CMD_BUFFER_SIZE    512U
#define ESP8266_RECONNECT_MS       15000U

static UART_HandleTypeDef *esp_uart = NULL;
static char esp_rx_buffer[ESP8266_RX_BUFFER_SIZE];
static ESP8266_OneNET_Status_t esp_status = ESP8266_ONENET_STATUS_OFFLINE;
static ESP8266_OneNET_Error_t esp_last_error = ESP8266_ONENET_ERROR_NONE;
static uint32_t last_reconnect_tick = 0U;

static const uint32_t esp_baud_list[] = {
  115200U,
  9600U,
  57600U,
  38400U
};

static uint8_t ESP8266_IsConfigured(void)
{
  // 修复：检查所有配置都不是示例值
  if ((strcmp(ONENET_WIFI_SSID, "YOUR_WIFI_SSID") == 0) ||
      (strcmp(ONENET_WIFI_PASSWORD, "YOUR_WIFI_PASSWORD") == 0) ||
      (strcmp(ONENET_PRODUCT_ID, "YOUR_PRODUCT_ID") == 0) ||
      (strcmp(ONENET_DEVICE_NAME, "YOUR_DEVICE_NAME") == 0) ||
      (strcmp(ONENET_MQTT_TOKEN, "YOUR_ONENET_TOKEN") == 0))
  {
    return 0U;
  }

  // 修复：增加长度检查，避免空字符串
  if ((strlen(ONENET_WIFI_SSID) == 0) ||
      (strlen(ONENET_WIFI_PASSWORD) == 0) ||
      (strlen(ONENET_PRODUCT_ID) == 0) ||
      (strlen(ONENET_DEVICE_NAME) == 0) ||
      (strlen(ONENET_MQTT_TOKEN) == 0))
  {
    return 0U;
  }

  return 1U;
}

static void ESP8266_ClearUartErrors(void)
{
  if (esp_uart == NULL)
  {
    return;
  }

  __HAL_UART_CLEAR_OREFLAG(esp_uart);
  __HAL_UART_CLEAR_FEFLAG(esp_uart);
  __HAL_UART_CLEAR_NEFLAG(esp_uart);
  __HAL_UART_CLEAR_PEFLAG(esp_uart);
  esp_uart->ErrorCode = HAL_UART_ERROR_NONE;
}

static void ESP8266_DrainRx(void)
{
  uint8_t ch;

  if (esp_uart == NULL)
  {
    return;
  }

  ESP8266_ClearUartErrors();

  while (HAL_UART_Receive(esp_uart, &ch, 1U, 1U) == HAL_OK)
  {
  }

  ESP8266_ClearUartErrors();
}

static uint8_t ESP8266_WaitFor(const char *expect, uint32_t timeout_ms)
{
  uint8_t ch;
  uint16_t index = 0U;
  uint32_t start_tick = HAL_GetTick();

  memset(esp_rx_buffer, 0, sizeof(esp_rx_buffer));
  ESP8266_ClearUartErrors();

  while ((HAL_GetTick() - start_tick) < timeout_ms)
  {
    if (HAL_UART_Receive(esp_uart, &ch, 1U, 20U) == HAL_OK)
    {
      if (index < (ESP8266_RX_BUFFER_SIZE - 1U))
      {
        esp_rx_buffer[index++] = (char)ch;
        esp_rx_buffer[index] = '\0';
      }

      if (strstr(esp_rx_buffer, expect) != NULL)
      {
        return 1U;
      }

      if ((strstr(esp_rx_buffer, "ERROR") != NULL) ||
          (strstr(esp_rx_buffer, "FAIL") != NULL))
      {
        return 0U;
      }
    }
  }

  return 0U;
}

static uint8_t ESP8266_WaitForEither(const char *expect1, const char *expect2, uint32_t timeout_ms)
{
  uint8_t ch;
  uint16_t index = 0U;
  uint32_t start_tick = HAL_GetTick();

  memset(esp_rx_buffer, 0, sizeof(esp_rx_buffer));
  ESP8266_ClearUartErrors();

  while ((HAL_GetTick() - start_tick) < timeout_ms)
  {
    if (HAL_UART_Receive(esp_uart, &ch, 1U, 20U) == HAL_OK)
    {
      if (index < (ESP8266_RX_BUFFER_SIZE - 1U))
      {
        esp_rx_buffer[index++] = (char)ch;
        esp_rx_buffer[index] = '\0';
      }

      if (((expect1 != NULL) && (strstr(esp_rx_buffer, expect1) != NULL)) ||
          ((expect2 != NULL) && (strstr(esp_rx_buffer, expect2) != NULL)))
      {
        return 1U;
      }

      if ((strstr(esp_rx_buffer, "ERROR") != NULL) ||
          (strstr(esp_rx_buffer, "FAIL") != NULL))
      {
        return 0U;
      }
    }
  }

  return 0U;
}

static uint8_t ESP8266_SendCommand(const char *cmd, const char *expect, uint32_t timeout_ms)
{
  size_t len;

  if ((esp_uart == NULL) || (cmd == NULL) || (expect == NULL))
  {
    return 0U;
  }

  ESP8266_DrainRx();

  len = strlen(cmd);
  if (HAL_UART_Transmit(esp_uart, (uint8_t *)cmd, (uint16_t)len, 1000U) != HAL_OK)
  {
    return 0U;
  }

  if (HAL_UART_Transmit(esp_uart, (uint8_t *)"\r\n", 2U, 1000U) != HAL_OK)
  {
    return 0U;
  }

  return ESP8266_WaitFor(expect, timeout_ms);
}

static uint8_t ESP8266_SendRawAfterPrompt(const char *cmd, const char *raw, const char *expect, uint32_t timeout_ms)
{
  size_t raw_len;

  if ((esp_uart == NULL) || (cmd == NULL) || (raw == NULL) || (expect == NULL))
  {
    return 0U;
  }

  if (ESP8266_SendCommand(cmd, ">", timeout_ms) == 0U)
  {
    return 0U;
  }

  raw_len = strlen(raw);
  if (HAL_UART_Transmit(esp_uart, (uint8_t *)raw, (uint16_t)raw_len, 1000U) != HAL_OK)
  {
    return 0U;
  }

  return ESP8266_WaitFor(expect, timeout_ms);
}

static uint8_t ESP8266_SetBaudRate(uint32_t baud_rate)
{
  if (esp_uart == NULL)
  {
    return 0U;
  }

  if (HAL_UART_DeInit(esp_uart) != HAL_OK)
  {
    return 0U;
  }

  esp_uart->Init.BaudRate = baud_rate;

  if (HAL_UART_Init(esp_uart) != HAL_OK)
  {
    return 0U;
  }

  HAL_Delay(100U);
  return 1U;
}

static uint8_t ESP8266_FindBaudRate(void)
{
  uint32_t i;
  uint32_t retry;

  for (i = 0U; i < (sizeof(esp_baud_list) / sizeof(esp_baud_list[0])); i++)
  {
    if (ESP8266_SetBaudRate(esp_baud_list[i]) == 0U)
    {
      continue;
    }

    for (retry = 0U; retry < 2U; retry++)
    {
      ESP8266_DrainRx();
      (void)HAL_UART_Transmit(esp_uart, (uint8_t *)"\r\n", 2U, 1000U);
      HAL_Delay(100U);

      if (ESP8266_SendCommand("AT", "OK", 1200U) != 0U)
      {
        return 1U;
      }

      HAL_Delay(200U);
    }
  }

  return 0U;
}

static uint8_t ESP8266_LoopbackSelfTest(void)
{
  uint8_t tx = 0x55U;
  uint8_t rx = 0U;

  if (ESP8266_SetBaudRate(115200U) == 0U)
  {
    return 0U;
  }

  ESP8266_DrainRx();
  ESP8266_ClearUartErrors();

  if (HAL_UART_Transmit(esp_uart, &tx, 1U, 1000U) != HAL_OK)
  {
    return 0U;
  }

  HAL_Delay(2U);
  ESP8266_ClearUartErrors();

  if (HAL_UART_Receive(esp_uart, &rx, 1U, 1000U) != HAL_OK)
  {
    return 0U;
  }

  return (rx == tx) ? 1U : 0U;
}

static uint8_t ESP8266_GPIO_LoopbackSelfTest(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_PinState low_state;
  GPIO_PinState high_state;
  GPIO_TypeDef *gpio_port;
  uint16_t tx_pin;
  uint16_t rx_pin;

  if (esp_uart == NULL)
  {
    return 0U;
  }

  if (esp_uart->Instance == USART1)
  {
    gpio_port = GPIOA;
    tx_pin = GPIO_PIN_9;
    rx_pin = GPIO_PIN_10;
    __HAL_RCC_GPIOA_CLK_ENABLE();
  }
  else if (esp_uart->Instance == USART3)
  {
    gpio_port = GPIOB;
    tx_pin = GPIO_PIN_10;
    rx_pin = GPIO_PIN_11;
    __HAL_RCC_GPIOB_CLK_ENABLE();
  }
  else
  {
    return 0U;
  }

  (void)HAL_UART_DeInit(esp_uart);

  GPIO_InitStruct.Pin = tx_pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(gpio_port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = rx_pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(gpio_port, &GPIO_InitStruct);

  HAL_GPIO_WritePin(gpio_port, tx_pin, GPIO_PIN_RESET);
  HAL_Delay(2U);
  low_state = HAL_GPIO_ReadPin(gpio_port, rx_pin);

  HAL_GPIO_WritePin(gpio_port, tx_pin, GPIO_PIN_SET);
  HAL_Delay(2U);
  high_state = HAL_GPIO_ReadPin(gpio_port, rx_pin);

  (void)ESP8266_SetBaudRate(115200U);

  return ((low_state == GPIO_PIN_RESET) && (high_state == GPIO_PIN_SET)) ? 1U : 0U;
}

static uint8_t ESP8266_RxIdleHighTest(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_PinState rx_state;
  GPIO_TypeDef *gpio_port;
  uint16_t rx_pin;

  if (esp_uart == NULL)
  {
    return 0U;
  }

  if (esp_uart->Instance == USART2)
  {
    gpio_port = GPIOA;
    rx_pin = GPIO_PIN_10;
    __HAL_RCC_GPIOA_CLK_ENABLE();
  }
  else if (esp_uart->Instance == USART3)
  {
    gpio_port = GPIOB;
    rx_pin = GPIO_PIN_11;
    __HAL_RCC_GPIOB_CLK_ENABLE();
  }
  else
  {
    return 1U;
  }

  (void)HAL_UART_DeInit(esp_uart);

  GPIO_InitStruct.Pin = rx_pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(gpio_port, &GPIO_InitStruct);

  HAL_Delay(2U);
  rx_state = HAL_GPIO_ReadPin(gpio_port, rx_pin);

  (void)ESP8266_SetBaudRate(115200U);

  return (rx_state == GPIO_PIN_SET) ? 1U : 0U;
}

static uint8_t ESP8266_ConnectWifi(void)
{
  char cmd[ESP8266_CMD_BUFFER_SIZE];

  if (ESP8266_FindBaudRate() == 0U)
  {
#if (ESP8266_ENABLE_LOOPBACK_TEST != 0U)
    if (ESP8266_LoopbackSelfTest() != 0U)
    {
      esp_last_error = ESP8266_ONENET_ERROR_LOOPBACK_OK;
      return 0U;
    }

    if (ESP8266_GPIO_LoopbackSelfTest() != 0U)
    {
      esp_last_error = ESP8266_ONENET_ERROR_GPIO_LOOPBACK_OK;
      return 0U;
    }

    esp_last_error = ESP8266_ONENET_ERROR_GPIO_LOOPBACK_FAIL;
    return 0U;
#else
    if (ESP8266_RxIdleHighTest() == 0U)
    {
      esp_last_error = ESP8266_ONENET_ERROR_RX_IDLE_LOW;
    }

    if (ESP8266_FindBaudRate() != 0U)
    {
      (void)ESP8266_SendCommand("ATE0", "OK", 1000U);
      goto baud_ready;
    }

    esp_last_error = ESP8266_ONENET_ERROR_AT;
    return 0U;
#endif
  }

baud_ready:
  (void)ESP8266_SendCommand("ATE0", "OK", 1000U);

  if (ESP8266_SendCommand("AT+CWMODE=1", "OK", 1000U) == 0U)
  {
    esp_last_error = ESP8266_ONENET_ERROR_MODE;
    return 0U;
  }

  (void)ESP8266_SendCommand("AT+CWAUTOCONN=0", "OK", 1000U);

  (void)snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"",
                 ONENET_WIFI_SSID, ONENET_WIFI_PASSWORD);
  ESP8266_DrainRx();
  if (HAL_UART_Transmit(esp_uart, (uint8_t *)cmd, (uint16_t)strlen(cmd), 1000U) != HAL_OK)
  {
    esp_last_error = ESP8266_ONENET_ERROR_JOIN_AP;
    return 0U;
  }
  if (HAL_UART_Transmit(esp_uart, (uint8_t *)"\r\n", 2U, 1000U) != HAL_OK)
  {
    esp_last_error = ESP8266_ONENET_ERROR_JOIN_AP;
    return 0U;
  }
  // 修复：增加等待时间从25秒到40秒，提高连接成功率
  if (ESP8266_WaitForEither("WIFI GOT IP", "OK", 40000U) == 0U)
  {
    esp_last_error = ESP8266_ONENET_ERROR_JOIN_AP;
    return 0U;
  }

  esp_status = ESP8266_ONENET_STATUS_WIFI_CONNECTED;
  return 1U;
}

static uint8_t ESP8266_ConnectMQTT(void)
{
  char cmd[ESP8266_CMD_BUFFER_SIZE];
  char topic[160];
  const char *mqtt_hosts[] = {
    ONENET_MQTT_HOST,
    ONENET_MQTT_HOST_BACKUP,
    ONENET_MQTT_HOST_BACKUP2
  };
  uint32_t i;

  (void)ESP8266_SendCommand("AT+MQTTCLEAN=0", "OK", 2000U);

  (void)snprintf(cmd, sizeof(cmd),
                 "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"x\",0,0,\"\"",
                 ONENET_DEVICE_NAME, ONENET_PRODUCT_ID);
  if (ESP8266_SendCommand(cmd, "OK", 3000U) == 0U)
  {
    (void)snprintf(cmd, sizeof(cmd),
                   "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"",
                   ONENET_DEVICE_NAME, ONENET_PRODUCT_ID, ONENET_MQTT_TOKEN);
    if (ESP8266_SendCommand(cmd, "OK", 3000U) == 0U)
    {
      esp_last_error = ESP8266_ONENET_ERROR_USERCFG;
      return 0U;
    }
  }
  else
  {
    (void)snprintf(cmd, sizeof(cmd), "AT+MQTTLONGPASSWORD=0,%u",
                   (unsigned int)strlen(ONENET_MQTT_TOKEN));
    if (ESP8266_SendRawAfterPrompt(cmd, ONENET_MQTT_TOKEN, "OK", 3000U) == 0U)
    {
      (void)snprintf(cmd, sizeof(cmd),
                     "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"",
                     ONENET_DEVICE_NAME, ONENET_PRODUCT_ID, ONENET_MQTT_TOKEN);
      if (ESP8266_SendCommand(cmd, "OK", 3000U) == 0U)
      {
        esp_last_error = ESP8266_ONENET_ERROR_LONG_PASSWORD;
        return 0U;
      }
    }
  }

  if (ESP8266_SendCommand("AT+MQTTCONNCFG=0,120,0,\"\",\"\",0,0", "OK", 3000U) == 0U)
  {
    esp_last_error = ESP8266_ONENET_ERROR_CONN_CFG;
    return 0U;
  }

  for (i = 0U; i < (sizeof(mqtt_hosts) / sizeof(mqtt_hosts[0])); i++)
  {
    (void)snprintf(cmd, sizeof(cmd), "AT+MQTTCONN=0,\"%s\",%u,1",
                   mqtt_hosts[i], (unsigned int)ONENET_MQTT_PORT);
    // 修复：增加MQTT连接超时从30秒到60秒
    if (ESP8266_SendCommand(cmd, "+MQTTCONNECTED", 60000U) != 0U)
    {
      break;
    }
  }

  if (i >= (sizeof(mqtt_hosts) / sizeof(mqtt_hosts[0])))
  {
    esp_last_error = ESP8266_ONENET_ERROR_MQTT_CONNECT;
    return 0U;
  }

  (void)snprintf(topic, sizeof(topic),
                 "$sys/%s/%s/thing/property/post/reply",
                 ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
  (void)snprintf(cmd, sizeof(cmd), "AT+MQTTSUB=0,\"%s\",0", topic);
  (void)ESP8266_SendCommand(cmd, "OK", 3000U);

  esp_status = ESP8266_ONENET_STATUS_MQTT_CONNECTED;
  esp_last_error = ESP8266_ONENET_ERROR_NONE;
  return 1U;
}

static uint8_t ESP8266_Reconnect(void)
{
  esp_status = ESP8266_ONENET_STATUS_OFFLINE;

  if (ESP8266_IsConfigured() == 0U)
  {
    esp_status = ESP8266_ONENET_STATUS_CONFIG_MISSING;
    esp_last_error = ESP8266_ONENET_ERROR_CONFIG;
    return 0U;
  }

  if (ESP8266_ConnectWifi() == 0U)
  {
    return 0U;
  }

  return ESP8266_ConnectMQTT();
}

void ESP8266_OneNET_Init(UART_HandleTypeDef *huart)
{
  esp_uart = huart;
  last_reconnect_tick = HAL_GetTick() - ESP8266_RECONNECT_MS;

  if (esp_uart == NULL)
  {
    esp_status = ESP8266_ONENET_STATUS_OFFLINE;
    return;
  }

  esp_status = ESP8266_ONENET_STATUS_OFFLINE;
  esp_last_error = ESP8266_ONENET_ERROR_NONE;
}

void ESP8266_OneNET_Task(void)
{
  uint32_t now_tick = HAL_GetTick();

  if ((esp_uart == NULL) || (esp_status == ESP8266_ONENET_STATUS_MQTT_CONNECTED))
  {
    return;
  }

  if ((now_tick - last_reconnect_tick) >= ESP8266_RECONNECT_MS)
  {
    last_reconnect_tick = now_tick;
    (void)ESP8266_Reconnect();
  }
}

uint8_t ESP8266_OneNET_PostTempHumi(float temperature, float humidity)
{
  char topic[160];
  char payload[180];
  char cmd[ESP8266_CMD_BUFFER_SIZE];

  if (esp_status != ESP8266_ONENET_STATUS_MQTT_CONNECTED)
  {
    return 0U;
  }

  (void)snprintf(topic, sizeof(topic),
                 "$sys/%s/%s/thing/property/post",
                 ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
  (void)snprintf(payload, sizeof(payload),
                 "{\"id\":\"123\",\"version\":\"1.0\",\"params\":{\"TEMP\":{\"value\":%.1f},\"HUMI\":{\"value\":%.1f}}}",
                 temperature,humidity);
  (void)snprintf(cmd, sizeof(cmd),
                 "AT+MQTTPUBRAW=0,\"%s\",%u,0,0",
                 topic, (unsigned int)strlen(payload));

  if (ESP8266_SendCommand(cmd, ">", 3000U) == 0U)
  {
    esp_status = ESP8266_ONENET_STATUS_OFFLINE;
    esp_last_error = ESP8266_ONENET_ERROR_PUBLISH;
    return 0U;
  }

  if (HAL_UART_Transmit(esp_uart, (uint8_t *)payload, (uint16_t)strlen(payload), 1000U) != HAL_OK)
  {
    esp_status = ESP8266_ONENET_STATUS_OFFLINE;
    esp_last_error = ESP8266_ONENET_ERROR_PUBLISH;
    return 0U;
  }

  if (ESP8266_WaitForEither("+MQTTPUB:OK", "OK", 5000U) == 0U)
  {
    esp_status = ESP8266_ONENET_STATUS_OFFLINE;
    esp_last_error = ESP8266_ONENET_ERROR_PUBLISH;
    return 0U;
  }

  return 1U;
}

ESP8266_OneNET_Status_t ESP8266_OneNET_GetStatus(void)
{
  return esp_status;
}

ESP8266_OneNET_Error_t ESP8266_OneNET_GetLastError(void)
{
  return esp_last_error;
}

const char *ESP8266_OneNET_GetLastResponse(void)
{
  return esp_rx_buffer;
}
