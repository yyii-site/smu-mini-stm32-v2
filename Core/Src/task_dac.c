#include "task_dac.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "main.h"
#include "semphr.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern SPI_HandleTypeDef hspi1;
extern SemaphoreHandle_t semaphore_spi1;

handle_AD5522 h_PMU;

static void AD5522_SPI1_Init(void) {
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
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

void task_dac_init() {
  if (xSemaphoreTake(semaphore_spi1, portMAX_DELAY)) {
  HAL_SPI_DeInit(&hspi1);
  AD5522_SPI1_Init();
  AD5522_init(&h_PMU, &hspi1, 5.0);
  AD5522_Calibrate(&h_PMU);
  AD5522_StartHiZMV(&h_PMU,
                    PMU_CH_1 | PMU_CH_2 |
                        PMU_CH_3); // configure CH2/3 to monitor voltage only
  // AD5522_SetClamp(&h_PMU,PMU_CH_0|PMU_CH_1,32767-30000,32767+30000,0,65535,PMU_DAC_SCALEID_EXT);
  AD5522_SetClamp_float(&h_PMU, PMU_CH_0, -80e-3, 80e-3, -11.5, 11.5,
                        PMU_DAC_SCALEID_2MA);
  AD5522_StartFVMI(&h_PMU, PMU_CH_0, PMU_DAC_SCALEID_2MA);
  // AD5522_StartFIMV(&h_PMU,PMU_CH_0|PMU_CH_1,PMU_DAC_SCALEID_200UA);
  // AD5522_SetOutputVoltage_float(&h_PMU,PMU_CH_0,-1.2);
  xSemaphoreGive(semaphore_spi1);
  }
}

void AD5522_in(void) {
  xSemaphoreTake(semaphore_spi1, portMAX_DELAY);
  HAL_SPI_DeInit(&hspi1);
  AD5522_SPI1_Init();
}
void AD5522_out(void) { xSemaphoreGive(semaphore_spi1); }

void getSmuFunction(uint8_t ch_i, char *mode, uint8_t modeSize) {
  char force[4] = {0};
  char measure[4] = {0};

  if (ch_i >= 4) {
    return;
  }

  uint32_t pmu = h_PMU.reg_pmu[ch_i];

  uint32_t buf = pmu;
  buf &= (0x03 << 19);
  if (buf == PMU_PMUREG_FVCI) {
    snprintf(force, sizeof(force), "FV");
  } else if (buf == PMU_PMUREG_FICV) {
    snprintf(force, sizeof(force), "FI");
  } else if (buf == PMU_PMUREG_HZV) {
    snprintf(force, sizeof(force), "HZV");
  } else if (buf == PMU_PMUREG_HZI) {
    snprintf(force, sizeof(force), "HZI");
  };

  buf = pmu;
  buf &= (0x03 << 13);
  if (buf == PMU_PMUREG_MEAS_I) {
    snprintf(measure, sizeof(measure), "MI");
  } else if (buf == PMU_PMUREG_MEAS_V) {
    snprintf(measure, sizeof(measure), "MV");
  } else if (buf == PMU_PMUREG_MEAS_TEMP) {
    snprintf(measure, sizeof(measure), "MT");
  } else if (buf == PMU_PMUREG_MEAS_HZ) {
    snprintf(measure, sizeof(measure), "MHZ");
  };

  snprintf(mode, modeSize, "%s%s", force, measure);
}

uint32_t getSmuIrange(uint8_t ch_i){
  if (ch_i >= 4) {
    return 0;
  }
  return h_PMU.i_range[ch_i];
}

// void getSmuIrange(uint8_t ch_i, char *range, uint8_t rangeSize) {
//   if (ch_i >= 4) {
//     return;
//   }
//   switch(h_PMU.i_range[ch_i])
// 	{
// 		case PMU_DAC_SCALEID_5UA:
// 			snprintf(range, rangeSize, "5uA");
// 			break;
// 		case PMU_DAC_SCALEID_20UA:
// 			snprintf(range, rangeSize, "20uA");
// 			break;
// 		case PMU_DAC_SCALEID_200UA:
// 			snprintf(range, rangeSize, "200uA");
// 			break;
// 		case PMU_DAC_SCALEID_2MA:
// 			snprintf(range, rangeSize, "2mA");
// 			break;
// 		case PMU_DAC_SCALEID_EXT:
// 			snprintf(range, rangeSize, "80mA");
// 			break;
// 	}
// }

double getSmuTargetVoltage(uint8_t ch_i) {
  if (ch_i >= 4) {
    return 0.0;
  }

  uint32_t v_dac = h_PMU.reg_DAC_FIN_V[ch_i][AD5522_DAC_REG_X1];
  float vref = h_PMU.vref;
  double voltage = (v_dac - 32768.0) / 65536.0 * vref * 4.5;

  return voltage;
}

double getSmuTargetCurrent(uint8_t ch_i) {
    if (ch_i >= 4) {
        return 0.0;
    }

    uint32_t i_dac = h_PMU.reg_DAC_FIN_I[ch_i][h_PMU.i_range[ch_i]][AD5522_DAC_REG_X1];
    float vref = h_PMU.vref;
    float Rsense = h_PMU.Rsense[ch_i];
    float MI_gain = 10.0;
    if (Rsense < 0.1) {
      Rsense = 500;
    }
    double current = (i_dac - 32768.0) / 65536.0 * vref * 4.5 / MI_gain / Rsense;

    return current;
}

void getSmuTargetSetting(uint8_t ch, char *buffer, size_t bufferSize) {
    char mode[5] = {0};
    getSmuFunction(ch, mode, sizeof(mode));
    if (strstr(mode, "FV") != NULL || strstr(mode, "HZV") != NULL) {
        snprintf(buffer, bufferSize, "Vset: %.3f  V", getSmuTargetVoltage(ch));
    } else if (strstr(mode, "FI") != NULL || strstr(mode, "HZI") != NULL) {
        snprintf(buffer, bufferSize, "Iset: %.6f mA", getSmuTargetCurrent(ch)*1e3);
    } else {
        snprintf(buffer, bufferSize, "%f", 0.0);
    }
}

void getSmuClampMaxSetting(uint8_t ch, char *buffer, size_t bufferSize) {
    if (ch >= 4) {
        snprintf(buffer, bufferSize, "Invalid channel");
        return;
    }
    // uint32_t cll_v = h_PMU.reg_DAC_CLL_V[ch][AD5522_DAC_REG_X1];
    uint32_t clh_v = h_PMU.reg_DAC_CLH_V[ch][AD5522_DAC_REG_X1];
    // uint32_t cll_i = h_PMU.reg_DAC_CLL_I[ch][AD5522_DAC_REG_X1];
    uint32_t clh_i = h_PMU.reg_DAC_CLH_I[ch][AD5522_DAC_REG_X1];
    float vref = h_PMU.vref;
    float Rsense = h_PMU.Rsense[ch];
    if (Rsense<0.1) {
      Rsense = 1.0;
    }
    float MI_gain = 10;
    // double v_clamp_low = (cll_v - 32768.0) / 65536.0 * vref * 4.5;
    double v_clamp_high = (clh_v - 32768.0) / 65536.0 * vref * 4.5;
    // double i_clamp_low = (cll_i - 32768.0) / 65536.0 * vref * 4.5 / MI_gain / Rsense;
    double i_clamp_high = (clh_i - 32768.0) / 65536.0 * vref * 4.5 / MI_gain / Rsense;  

    char mode[5] = {0};
    getSmuFunction(ch, mode, sizeof(mode));
    if (strstr(mode, "FV") != NULL || strstr(mode, "HZV") != NULL) {
        snprintf(buffer, bufferSize, "Imax: %.3f mA", i_clamp_high*1e3);
    } else if (strstr(mode, "FI") != NULL || strstr(mode, "HZI") != NULL) {
        snprintf(buffer, bufferSize, "Vmax: %.6f  V", v_clamp_high);
    } else {
        snprintf(buffer, bufferSize, "Unknown mode");
    }
}

void getSmuClampMinSetting(uint8_t ch, char *buffer, size_t bufferSize) {
    if (ch >= 4) {
        snprintf(buffer, bufferSize, "Invalid channel");
        return;
    }
    uint32_t cll_v = h_PMU.reg_DAC_CLL_V[ch][AD5522_DAC_REG_X1];
    // uint32_t clh_v = h_PMU.reg_DAC_CLH_V[ch][AD5522_DAC_REG_X1];
    uint32_t cll_i = h_PMU.reg_DAC_CLL_I[ch][AD5522_DAC_REG_X1];
    // uint32_t clh_i = h_PMU.reg_DAC_CLH_I[ch][AD5522_DAC_REG_X1];
    float vref = h_PMU.vref;
    float Rsense = h_PMU.Rsense[ch];
    float MI_gain = 10;
    double v_clamp_low = (cll_v - 32768.0) / 65536.0 * vref * 4.5;
    // double v_clamp_high = (clh_v - 32768.0) / 65536.0 * vref * 4.5;
    double i_clamp_low = (cll_i - 32768.0) / 65536.0 * vref * 4.5 / MI_gain / Rsense;
    // double i_clamp_high = (clh_i - 32768.0) / 65536.0 * vref * 4.5 / MI_gain / Rsense;  

    char mode[5] = {0};
    getSmuFunction(ch, mode, sizeof(mode));
    if (strstr(mode, "FV") != NULL || strstr(mode, "HZV") != NULL) {
        snprintf(buffer, bufferSize, "Imin: %.3f mA", i_clamp_low*1e3);
    } else if (strstr(mode, "FI") != NULL || strstr(mode, "HZI") != NULL) {
        snprintf(buffer, bufferSize, "Vmin: %.6f  V", v_clamp_low);
    } else {
        snprintf(buffer, bufferSize, "Unknown mode");
    }
}