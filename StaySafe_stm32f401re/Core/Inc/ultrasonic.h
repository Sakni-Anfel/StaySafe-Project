/* ultrasonic.h */

#ifndef ULTRASONIC_H
#define ULTRASONIC_H

#include "stm32f4xx_hal.h"

extern volatile uint32_t IC_Value1;
extern volatile uint32_t IC_Value2;
extern volatile uint8_t  edgeState;        // 0 = rising, 1 = falling, 2 = done
extern volatile uint32_t pulseWidth_us;

void Ultrasonic_Trigger(void);

#endif