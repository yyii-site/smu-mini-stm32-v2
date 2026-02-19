#ifndef __TASK_ADC_H
#define __TASK_ADC_H

#include <stdint.h>
#include <stddef.h>

extern uint32_t ad7190_val[4];

void getMeasuredString(uint8_t ch, char *buffer, size_t bufferSize);

void task_adc_init(void);
void task_adc_loop(void);


#endif