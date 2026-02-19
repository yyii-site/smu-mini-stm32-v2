#include "u8g2.h"
#include "main.h"
#include "cmsis_os.h"
#include <sys/_intsup.h>

extern I2C_HandleTypeDef hi2c1;

#define I2C_ADDRESS 0x3C

u8g2_t u8g2;


uint8_t u8x8_byte_stm32hal_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int,
                                  void *arg_ptr) {
  static uint8_t buffer[32]; /* u8g2/u8x8 will never send more than 32 bytes
                                between START_TRANSFER and END_TRANSFER */
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
    /* only support for software I2C*/
    break;
  case U8X8_MSG_DELAY_NANO:
    /* not required for SW I2C */
    {
      volatile uint32_t i;
      for (i = 1; i <= arg_int * 10; i++)
        ;
    }
    break;
  case U8X8_MSG_DELAY_10MICRO:
    /* not used at the moment */
    break;
  case U8X8_MSG_DELAY_100NANO:
    /* not used at the moment */
    break;
  case U8X8_MSG_DELAY_MILLI:
    osDelay(arg_int);
    break;
  case U8X8_MSG_DELAY_I2C:
    /* arg_int is 1 or 4: 100KHz (5us) or 400KHz (1.25us) */
    // delay_micro_seconds(arg_int<=2?5:1);
    break;
  case U8X8_MSG_GPIO_I2C_CLOCK:
    break;
    /*
        case U8X8_MSG_GPIO_MENU_SELECT:
          u8x8_SetGPIOResult(u8x8, Chip_GPIO_GetPinState(LPC_GPIO,
       KEY_SELECT_PORT, KEY_SELECT_PIN)); break; case U8X8_MSG_GPIO_MENU_NEXT:
          u8x8_SetGPIOResult(u8x8, Chip_GPIO_GetPinState(LPC_GPIO,
       KEY_NEXT_PORT, KEY_NEXT_PIN)); break; case U8X8_MSG_GPIO_MENU_PREV:
          u8x8_SetGPIOResult(u8x8, Chip_GPIO_GetPinState(LPC_GPIO,
       KEY_PREV_PORT, KEY_PREV_PIN)); break;

        case U8X8_MSG_GPIO_MENU_HOME:
          u8x8_SetGPIOResult(u8x8, Chip_GPIO_GetPinState(LPC_GPIO,
       KEY_HOME_PORT, KEY_HOME_PIN)); break;
    */
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

void u8g2_loop() {
  static uint8_t count = 0;
  count++;
  if (count > '9')
    count = '0';
  u8g2_ClearBuffer(&u8g2);

  u8g2_SetFontMode(&u8g2, 1); // Transparent
  u8g2_SetFontDirection(&u8g2, 0);
  u8g2_SetFont(&u8g2, u8g2_font_inb24_mf);
  u8g2_DrawStr(&u8g2, 0, 30, "U");

  u8g2_SetFontDirection(&u8g2, 1);
  u8g2_SetFont(&u8g2, u8g2_font_inb30_mn);
  char str[2] = {count, '\0'};
  u8g2_DrawStr(&u8g2, 21, 8, str);

  u8g2_SetFontDirection(&u8g2, 0);
  u8g2_SetFont(&u8g2, u8g2_font_inb24_mf);
  u8g2_DrawStr(&u8g2, 51, 30, "g");
  u8g2_DrawStr(&u8g2, 67, 30, "\xb2");

  u8g2_DrawHLine(&u8g2, 2, 35, 47);
  u8g2_DrawHLine(&u8g2, 3, 36, 47);
  u8g2_DrawVLine(&u8g2, 45, 32, 12);
  u8g2_DrawVLine(&u8g2, 46, 33, 12);

  u8g2_SetFont(&u8g2, u8g2_font_4x6_tr);
  u8g2_DrawStr(&u8g2, 1, 54, "github.com/olikraus/u8g2");

  u8g2_SendBuffer(&u8g2);
}

