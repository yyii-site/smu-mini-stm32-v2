#ifndef __TASK_DAC_H
#define __TASK_DAC_H

#include "ad5522.h"

extern handle_AD5522 h_PMU;

void AD5522_in(void);
void AD5522_out(void);
uint32_t getSmuIrange(uint8_t ch_i);
// void getSmuIrange(uint8_t ch_i, char *range, uint8_t rangeSize);
double getSmuTargetVoltage(uint8_t ch_i);
double getSmuTargetCurrent(uint8_t ch_i);
void getSmuFunction(uint8_t ch_i, char *mode, uint8_t modeSize);
void getSmuTargetSetting(uint8_t ch, char *buffer, size_t bufferSize);
void getSmuClampMaxSetting(uint8_t ch, char *buffer, size_t bufferSize);
void getSmuClampMinSetting(uint8_t ch, char *buffer, size_t bufferSize);

void task_dac_init(void);


#endif