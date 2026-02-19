#include "FreeRTOS.h"
#include "ad7190.h"
#include "cmsis_os2.h"
#include "main.h"
#include "semphr.h"
#include "task_dac.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// AD7190 calibration coefficients
#define AD7190_V1_B 7535960
#define AD7190_V1_K 1.491210803524029e-6

extern SPI_HandleTypeDef hspi1;
extern SemaphoreHandle_t semaphore_spi1;

uint32_t ad7190_val[4];

static void AD7190_SPI1_Init(void) {
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK) {
    Error_Handler();
  }
}

// 将 clk 的电平切换到 High
void SPI_Send_Nop() {
  const uint8_t nop_data = 0;
  HAL_SPI_Transmit(&hspi1, &nop_data, 1, 1000);
}
void AD7190_CS_Low() {
  HAL_GPIO_WritePin(ADC_CS_GPIO_Port, ADC_CS_Pin, GPIO_PIN_RESET);
}
void AD7190_CS_High() {
  HAL_GPIO_WritePin(ADC_CS_GPIO_Port, ADC_CS_Pin, GPIO_PIN_SET);
}
void AD7190_Spi_WriteByte(uint8_t cmd) {
  HAL_SPI_Transmit(&hspi1, &cmd, 1, 1000);
}
uint8_t AD7190_Spi_ReadByte() {
  uint8_t val = 0;
  int resp = HAL_SPI_TransmitReceive(&hspi1, 0x00, &val, 1, 1000);
  if (resp != HAL_OK) {
    return 0;
  }
  return val;
}
void AD7190_Spi_Write(uint8_t *cmd, uint16_t len) {
  HAL_SPI_Transmit(&hspi1, cmd, len, 1000);
}
void AD7190_Spi_Read(uint8_t *cmd, uint16_t len) {
  for (int i = 0; i < len; i++) {
    uint8_t send = cmd[i];
    uint8_t read = 0;
    int resp = HAL_SPI_TransmitReceive(&hspi1, &send, &read, 1, 1000);
    if (resp != HAL_OK) {
      return;
    }
    cmd[i] = read;
  }
}
bool AD7190_CheckDataReadyPin(void) {
  return HAL_GPIO_ReadPin(ADC_READY_GPIO_Port, ADC_READY_Pin);
}
void AD7190_Delay_ms(uint32_t ms) { osDelay(ms); }
AD7190_SpiDriver_Typedef h_ad7190 = {
    .AD7190_CS_Low = AD7190_CS_Low,
    .AD7190_CS_High = AD7190_CS_High,
    .AD7190_Spi_WriteByte = AD7190_Spi_WriteByte,
    .AD7190_Spi_ReadByte = AD7190_Spi_ReadByte,
    .AD7190_Spi_Write = AD7190_Spi_Write,
    .AD7190_Spi_Read = AD7190_Spi_Read,
    .AD7190_CheckDataReadyPin = AD7190_CheckDataReadyPin,
    .AD7190_Delay_ms = AD7190_Delay_ms};

void task_adc_init() {
  if (xSemaphoreTake(semaphore_spi1, portMAX_DELAY)) {
    HAL_SPI_DeInit(&hspi1);
    AD7190_SPI1_Init();
    SPI_Send_Nop();

    if (!AD7190_Init(&h_ad7190)) {
      ; // AD7190 init failed
    }
    AD7190_RangeSetup(&h_ad7190, 1,
                      AD7190_CONF_GAIN_1); // ADC In unipolar 单极性0-5V
    AD7190_Calibrate(&h_ad7190, AD7190_MODE_CAL_INT_ZERO,
                     AD7190_CH_AIN1P_AINCOM);
    AD7190_Calibrate(&h_ad7190, AD7190_MODE_CAL_INT_FULL,
                     AD7190_CH_AIN1P_AINCOM);
    AD7190_Calibrate(&h_ad7190, AD7190_MODE_CAL_INT_ZERO,
                     AD7190_CH_AIN2P_AINCOM);
    AD7190_Calibrate(&h_ad7190, AD7190_MODE_CAL_INT_FULL,
                     AD7190_CH_AIN2P_AINCOM);
    AD7190_Calibrate(&h_ad7190, AD7190_MODE_CAL_INT_ZERO,
                     AD7190_CH_AIN3P_AINCOM);
    AD7190_Calibrate(&h_ad7190, AD7190_MODE_CAL_INT_FULL,
                     AD7190_CH_AIN3P_AINCOM);
    AD7190_Calibrate(&h_ad7190, AD7190_MODE_CAL_INT_ZERO,
                     AD7190_CH_AIN4P_AINCOM);
    AD7190_Calibrate(&h_ad7190, AD7190_MODE_CAL_INT_FULL,
                     AD7190_CH_AIN4P_AINCOM);
    AD7190_ChannelSelectAll(&h_ad7190);
    AD7190_ContinueMode_StatusAfterData(&h_ad7190);
    AD7190_AutoContinueMode(&h_ad7190, true);
    xSemaphoreGive(semaphore_spi1);
  }
}

void task_adc_loop(void) {
  static uint8_t channel = 0;
  uint32_t value = 0;
  if (xSemaphoreTake(semaphore_spi1, portMAX_DELAY)) {
    if (hspi1.Init.CLKPolarity != SPI_POLARITY_HIGH) { // 减少重复初始化
      HAL_SPI_DeInit(&hspi1);
      AD7190_SPI1_Init();
      SPI_Send_Nop();
    }
    if (AD7190_ReadDataRegister(&h_ad7190, &channel, &value)) {
      ad7190_val[channel] = value;
    }
    xSemaphoreGive(semaphore_spi1);
  }
}

void getCalibrationFromRange(uint32_t IscaleID, double *k, double *b) {
  switch (IscaleID) {
  case PMU_DAC_SCALEID_5UA:
    *k = 3.0517578125e-8;
    *b = -0.1572864;
    break;
  case PMU_DAC_SCALEID_20UA:
    *k = 1.220703125e-7;
    *b = -0.6291456;
    break;
  case PMU_DAC_SCALEID_200UA:
    *k = 1.220703125e-6;
    *b = -6.291456;
    break;
  case PMU_DAC_SCALEID_2MA:
    *k = 2.991199535114993e-10;
    *b = 7544410;
    break;
  case PMU_DAC_SCALEID_EXT:
    *k = 4.8828125e-5;
    *b = -251.65824;
    break;
  default:
    *k = 2.991199535114993e-10;
    *b = 7544410;
    break;
  }
}

double getMeasuredCurrent(uint8_t ch) {
  if (ch >= 4)
    return 0.0;
  double k, b = 0.0;

  getCalibrationFromRange(getSmuIrange(ch), &k, &b);
  return ((double)ad7190_val[ch] - b) * k;
}

double getMeasuredVoltage(uint8_t ch) {
  if (ch >= 4)
    return 0.0;
  return ((double)ad7190_val[ch] - AD7190_V1_B) * AD7190_V1_K;
}

void getMeasuredString(uint8_t ch, char *buffer, size_t bufferSize) {
  if (ch >= 4) {
    snprintf(buffer, bufferSize, "Invalid channel");
    return;
  }
  getSmuFunction(ch, buffer, bufferSize);
  if (strlen(buffer) == 0) {
    snprintf(buffer, bufferSize, "Unknown mode");
    return;
  }
  if (strstr(buffer, "MV") != NULL) {
    double voltage = getMeasuredVoltage(ch);
    snprintf(buffer, bufferSize, "Vmes: %.6f V", voltage);
  } else if (strstr(buffer, "MI") != NULL) {
    double current = getMeasuredCurrent(ch);
    if (fabs(current) < 1e-3) {
      snprintf(buffer, bufferSize, "Imes: %.3f uA", current * 1.0e6);
    } else {
      snprintf(buffer, bufferSize, "Imes: %.4f mA", current * 1.0e3);
    }
  } else {
    snprintf(buffer, bufferSize, "Unknown mode");
  }
}