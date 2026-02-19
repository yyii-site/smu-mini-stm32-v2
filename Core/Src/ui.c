#include "task_adc.h"
#include "task_dac.h"
#include "u8g2.h"
#include "main.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <stdio.h>

extern I2C_HandleTypeDef hi2c1;

#define I2C_ADDRESS 0x3C

u8g2_t u8g2;

/* --- u8g2 callbacks (unchanged) --- */
uint8_t u8x8_byte_stm32hal_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int,
                                  void *arg_ptr) {
  static uint8_t buffer[32];
  static uint8_t buf_idx;
  uint8_t *data;

  switch (msg) {
  case U8X8_MSG_BYTE_SEND: {
    data = (uint8_t *)arg_ptr;
    while (arg_int > 0) {
      buffer[buf_idx++] = *data;
      data++;
      arg_int--;
    }
  } break;
  case U8X8_MSG_BYTE_START_TRANSFER: {
    buf_idx = 0;
  } break;
  case U8X8_MSG_BYTE_END_TRANSFER: {
    uint8_t iaddress = I2C_ADDRESS;
    HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)iaddress << 1, &buffer[0],
                            buf_idx, 20u);
  } break;
  default:
    return 0;
  }
  return 1;
}

uint8_t psoc_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int,
                               void *arg_ptr) {
  switch (msg) {
  case U8X8_MSG_GPIO_AND_DELAY_INIT:
    break;
  case U8X8_MSG_DELAY_NANO:
    {
      volatile uint32_t i;
      for (i = 1; i <= arg_int * 10; i++)
        ;
    }
    break;
  case U8X8_MSG_DELAY_MILLI:
    osDelay(arg_int);
    break;
  default:
    u8x8_SetGPIOResult(u8x8, 1);
    break;
  }
  return 1;
}

void u8g2_init(void) {
  u8g2_Setup_ssd1306_i2c_128x64_noname_f(
      &u8g2, U8G2_R0, u8x8_byte_stm32hal_hw_i2c, psoc_gpio_and_delay_cb);
  u8g2_SetI2CAddress(&u8g2, I2C_ADDRESS * 2);
  u8g2_InitDisplay(&u8g2);
  u8g2_SetPowerSave(&u8g2, 0);
  u8g2_ClearDisplay(&u8g2);
  u8g2_SetDrawColor(&u8g2, 1);
  u8g2_SetFontDirection(&u8g2, 0);
}

/* UI loop: one screen per channel; KEY1 short press cycles channels */
/* NOTE: if you later add a hardware rotary encoder, replace the KEY1 reading
   with encoder position (TIM counter) reading and map to channel 0..3. */
void u8g2_loop() {
  static uint8_t channel = 0;
  static uint8_t last_btn = 1;
  static uint32_t last_debounce = 0;
  const uint32_t debounce_ms = 1000;

  /* button: assume active low (adjust if your hardware is different) */
  uint8_t btn_state = HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin);

  /* simple debounce */
  // uint32_t now = HAL_GetTick();
  // if (btn_state != last_btn) {
  //     last_debounce = now;
  //     last_btn = btn_state;
  // } else {
  //     if ((now - last_debounce) > debounce_ms) {
  //         /* on falling edge (pressed) */
  //         if (btn_state == GPIO_PIN_RESET) {
  //             channel = (channel + 1) & 0x03; /* cycle 0..3 */
  //             /* tiny delay to avoid multi-increment if needed */
  //             osDelay(120);
  //         }
  //     }
  // }

  char buff[28] = {0};
  char line[32] = {0};

  getSmuFunction(channel, buff, sizeof(buff));

  /* Draw UI */
  u8g2_ClearBuffer(&u8g2);

  /* Top: channel and mode */
  u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
  snprintf(line, sizeof(line), "CH%u  %s", channel, buff);
  u8g2_DrawStr(&u8g2, 2, 10, line);
  /* Middle: target setting */
  getSmuTargetSetting(channel, buff, sizeof(buff));
  u8g2_DrawStr(&u8g2, 2, 20, buff);
  /* Bottom: measured value */
  getMeasuredString(channel, buff, sizeof(buff));
  u8g2_DrawStr(&u8g2, 2, 30, buff);

  getSmuClampMaxSetting(channel, buff, sizeof(buff));
  u8g2_DrawStr(&u8g2, 2, 40, buff);

  getSmuClampMinSetting(channel, buff, sizeof(buff));
  u8g2_DrawStr(&u8g2, 2, 50, buff);

  u8g2_SendBuffer(&u8g2);
}