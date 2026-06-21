/*
 * FND_595.h
 *
 *  Created on: 2026. 2. 9.
 *      Author: user22
 */

#include "stm32f4xx_hal.h"
#include <stdbool.h>

#ifndef INC_FND_595_H_
#define INC_FND_595_H_

#define SER_PIN       GPIO_PIN_8
#define RCLK_PIN      GPIO_PIN_6
#define SRCLK_PIN     GPIO_PIN_5
#define GPIO_PORT     GPIOC

// ext16[0]  -> 3rd 74HC595 QA
// ext16[7]  -> 3rd 74HC595 QH
// ext16[8]  -> 4th 74HC595 QA
// ext16[15] -> 4th 74HC595 QH
// true=1, false=0
void showFND(uint8_t digit);
void showBar(uint8_t n);

void delay_us(uint16_t us);

void dataOut(bool *arr16);

#endif /* INC_FND_595_H_ */
