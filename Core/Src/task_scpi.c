#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "main.h"
#include "scpi-def.h"
#include "scpi/scpi.h"
#include "semphr.h"
#include "stm32f4xx_hal_uart.h"
#include <stdio.h>
#include "lwrb/lwrb.h"

extern UART_HandleTypeDef huart1;

#define COUNTOF(__BUFFER__)   (sizeof(__BUFFER__) / sizeof(*(__BUFFER__)))

#define RX_BUFFER_SIZE   32
uint8_t aRXBufferUser[RX_BUFFER_SIZE];

/* Declare rb instance & raw data */
lwrb_t buff;
uint8_t buff_data[RX_BUFFER_SIZE*2];

void PrintInfo(UART_HandleTypeDef *huart, uint8_t *String, uint16_t Size);
void StartReception(void);
void UserDataTreatment(UART_HandleTypeDef *huart, uint8_t* pData, uint16_t Size);


size_t SCPI_Write(scpi_t *context, const char *data, size_t len) {
  (void)context;
  if (HAL_OK == HAL_UART_Transmit(&huart1, (uint8_t *)data, len, 1000)) {
    return len;
  } else {
    return 0;
  }
}

scpi_result_t SCPI_Flush(scpi_t *context) {
  (void)context;
  return SCPI_RES_OK;
}

int SCPI_Error(scpi_t *context, int_fast16_t err) {
  (void)context;

  char _err[50];
  snprintf(_err, sizeof(_err), "**ERROR: %d, \"%s\"\r\n", (int16_t) err, SCPI_ErrorTranslate(err));
  HAL_UART_Transmit(&huart1, (uint8_t *)_err, strlen(_err), 1000);
  return 0;
}

scpi_result_t SCPI_Control(scpi_t *context, scpi_ctrl_name_t ctrl,
                           scpi_reg_val_t val) {
  (void)context;

  char _err[50];
  if (SCPI_CTRL_SRQ == ctrl) {
    snprintf(_err, sizeof(_err), "**SRQ: 0x%X (%d)\r\n", val, val);
  } else {
    snprintf(_err, sizeof(_err), "**CTRL %02x: 0x%X (%d)\r\n", ctrl, val, val);
  }
  HAL_UART_Transmit(&huart1, (uint8_t *)_err, strlen(_err), 1000);

  return SCPI_RES_OK;
}

scpi_result_t SCPI_Reset(scpi_t *context) {
  (void)context;

  char _err[50];
  snprintf(_err, sizeof(_err), "**Reset\r\n");
  HAL_UART_Transmit(&huart1, (uint8_t *)_err, strlen(_err), 1000);

  return SCPI_RES_OK;
}

scpi_result_t SCPI_SystemCommTcpipControlQ(scpi_t *context) {
  (void)context;

  return SCPI_RES_ERR;
}

void task_scpi_init(void) {
  // semaphore_scpi = xSemaphoreCreateMutex();

  StartReception();

  SCPI_Init(&scpi_context, scpi_commands, &scpi_interface, scpi_units_def,
            SCPI_IDN1, SCPI_IDN2, SCPI_IDN3, SCPI_IDN4, scpi_input_buffer,
            SCPI_INPUT_BUFFER_LENGTH, scpi_error_queue_data,
            SCPI_ERROR_QUEUE_SIZE);
}

void task_scpi_loop(void) {
  uint32_t flags;
  uint8_t data[RX_BUFFER_SIZE*2]; /* Application working data */
  size_t len;

  flags = osThreadFlagsWait(0x01, osFlagsWaitAny, osWaitForever);
  if (!(flags & 0x80000000)) { // 检查是否是错误码 (最高位为1表示错误)
    // 1. 获取当前缓冲区内已存储的数据长度
    size_t full_size = lwrb_get_full(&buff);
    
    // 2. 确保缓冲区内至少有 1 个字节
    if (full_size >= 1) {
        uint8_t last_byte;
        
        // 使用 peek 预读最后一个字节
        // 偏移量为 (full_size - 1)
        lwrb_peek(&buff, full_size - 1, &last_byte, 1);

        // 3. 判断最后一位是否为 0x0D (\r) 或 0x0A (\n)
        if (last_byte == 0x0A || last_byte == 0x0D) {
            // 匹配成功：说明收到了结束符，现在将数据从缓冲区读出
            len = lwrb_read(&buff, data, sizeof(data));
            if (len > 0) {
                // PrintInfo(&huart1, data, len);
                // HAL_GPIO_WritePin(GPIOB, LED5_Pin, GPIO_PIN_SET);
                // xSemaphoreTake(semaphore_scpi, portMAX_DELAY);
                SCPI_Input(&scpi_context, (const char *)data, len);
                // xSemaphoreGive(semaphore_scpi);
                // HAL_GPIO_WritePin(GPIOB, LED5_Pin, GPIO_PIN_RESET);
            }
        } else {
          return;
        }
    }
  }
}

/**
  * @brief  Send Txt information message on UART Tx line (to PC Com port).
  * @param  huart UART handle.
  * @param  String String to be sent to user display
  * @param  Size   Size of string
  * @retval None
  */
void PrintInfo(UART_HandleTypeDef *huart, uint8_t *String, uint16_t Size)
{
  if (HAL_OK != HAL_UART_Transmit(huart, String, Size, 100))
  {
    Error_Handler();
  }
}

/**
  * @brief  This function prints user info on PC com port and initiates RX transfer
  * @retval None
  */
void StartReception(void)
{
  /* Initialize buffer */
  lwrb_init(&buff, buff_data, sizeof(buff_data));

  /* Initializes Rx sequence using Reception To Idle event API.
     As DMA channel associated to UART Rx is configured as Circular,
     reception is endless.
     If reception has to be stopped, call to HAL_UART_AbortReceive() could be used.

     Use of HAL_UARTEx_ReceiveToIdle_DMA service, will generate calls to
     user defined HAL_UARTEx_RxEventCallback callback for each occurrence of
     following events :
     - DMA RX Half Transfer event (HT)
     - DMA RX Transfer Complete event (TC)
     - IDLE event on UART Rx line (indicating a pause is UART reception flow)
  */
  if (HAL_OK != HAL_UARTEx_ReceiveToIdle_DMA(&huart1, aRXBufferUser, RX_BUFFER_SIZE))
  {
    Error_Handler();
  }
}

/**
  * @brief  This function handles buffer containing received data on PC com port
  * @note   In this example, received data are sent back on UART Tx (loopback)
  *         Any other processing such as copying received data in a larger buffer to make it
  *         available for application, could be implemented here.
  * @note   This routine is executed in Interrupt context.
  * @param  huart UART handle.
  * @param  pData Pointer on received data buffer to be processed
  * @retval Size  Nb of received characters available in buffer
  */
void UserDataTreatment(UART_HandleTypeDef *huart, uint8_t* pData, uint16_t Size)
{
  /*
   * This function might be called in any of the following interrupt contexts :
   *  - DMA TC and HT events
   *  - UART IDLE line event
   *
   * pData and Size defines the buffer where received data have been copied, in order to be processed.
   * During this processing of already received data, reception is still ongoing.
   *
   */
  uint8_t* pBuff = pData;
  uint8_t  i;

  /* Implementation of loopback is on purpose implemented in direct register access,
     in order to be able to echo received characters as fast as they are received.
     Wait for TC flag to be raised at end of transmit is then removed, only TXE is checked */
  for (i = 0; i < Size; i++)
  {
    while (!(__HAL_UART_GET_FLAG(huart, UART_FLAG_TXE))) {}
    huart->Instance->DR = *pBuff;
    pBuff++;
  }

}

/**
  * @brief  User implementation of the Reception Event Callback
  *         (Rx event notification called after use of advanced reception service).
  * @param  huart UART handle
  * @param  Size  Number of data available in application reception buffer (indicates a position in
  *               reception buffer until which, data are available)
  * @retval None
  */
extern osThreadId_t myTask02Handle;
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  static uint8_t old_pos = 0;
  uint16_t len = 0;

  /* Check if number of received data in recpetion buffer has changed */
  if (Size != old_pos)
  {
    /* Check if position of index in reception buffer has simply be increased
       of if end of buffer has been reached */
    if (Size > old_pos)
    {
      len = Size - old_pos;
      lwrb_write(&buff, &aRXBufferUser[old_pos], len);
    }
    else
    {
      len = RX_BUFFER_SIZE - old_pos;
      lwrb_write(&buff, &aRXBufferUser[old_pos], len);
      if (Size > 0) {
        lwrb_write(&buff, &aRXBufferUser[0], Size);
      }
    }    
    old_pos = Size;
    osThreadFlagsSet(myTask02Handle, 0x01);
  }
}
