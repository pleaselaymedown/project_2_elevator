/*
 * led.c
 *
 *  Created on: Jan 22, 2026
 *      Author: seonmin
 */

#include "led.h"



LED_CONTROL led[8]=
    {
        {GPIOC, GPIO_PIN_11, GPIO_PIN_SET, GPIO_PIN_RESET},      // 0
        {GPIOC, GPIO_PIN_10, GPIO_PIN_SET, GPIO_PIN_RESET},      // 1
        {GPIOC, GPIO_PIN_12, GPIO_PIN_SET, GPIO_PIN_RESET},      // 2
        {GPIOB, GPIO_PIN_7, GPIO_PIN_SET, GPIO_PIN_RESET},
        {GPIOC, GPIO_PIN_13, GPIO_PIN_SET, GPIO_PIN_RESET},
        {GPIOC, GPIO_PIN_14, GPIO_PIN_SET, GPIO_PIN_RESET},
        {GPIOC, GPIO_PIN_15, GPIO_PIN_SET, GPIO_PIN_RESET},
        {GPIOC, GPIO_PIN_3, GPIO_PIN_SET, GPIO_PIN_RESET}       // 7
    };




void ledOn(uint8_t num)
{
  for(uint8_t i = 0; i < num; i++)    // led를 num개 만큼 켜는거
  {
    HAL_GPIO_WritePin(led[i].port, led[i].number, led[i].onState);
  }
}

void ledOff(uint8_t num)
{
  for(uint8_t i = 0; i < num; i++)    // led를 num개 만큼 끄는거
    {
      HAL_GPIO_WritePin(led[i].port, led[i].number, led[i].offState);
    }
}

void ledToggle(uint8_t num)   // 지정된 핀만 토글
{
  HAL_GPIO_TogglePin(led[num].port, led[num].number);
}

void ledLeftShift(uint8_t num)
{
  for(uint8_t i = 0; i < num; i++)
    {
      ledOn(i);
      HAL_Delay(100);
    }
  HAL_Delay(500);
  for(uint8_t i = 0; i < num; i++)
    {
      ledOff(i);
      HAL_Delay(100);
    }
  HAL_Delay(500);
}
