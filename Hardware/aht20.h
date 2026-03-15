#ifndef __AHT20_H
#define __AHT20_H

#include <stdint.h>

void AHT20_Init_Device(void);
uint8_t AHT20_Read_Values(float *temp, float *hum);

#endif
