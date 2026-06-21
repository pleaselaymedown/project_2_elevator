/*
 * FND_595.c
 *
 *  Created on: 2026. 2. 9.
 *      Author: user22
 */

#include "FND_595.h"
#include "tim.h"

static const uint8_t fndData[10] = {
    0b11000000, //0
    0b11111001, //1
    0b10100100, //2
    0b10110000, //3
    0b10011001, //4
    0b10010010, //5
    0b10000010, //6
    0b11111000, //7
    0b10000000, //8
    0b10011000  //9
};

// 기존 1~2번째 74HC595에 들어갈 16bit (LSB=1번째, MSB=2번째)
static uint16_t data595 = 0;

static uint8_t packByteFromBool(const bool *arr8){//8개짜리 불배열의 각 원소를 각 비트로
    uint8_t ret = 0;
    for (int i = 0; i < 8; i++){
        if (arr8[i])
        	ret |= (uint8_t)(1u << i); //비트마스킹
    }
    return ret;
}

void showFND(uint8_t digit)//1자리 숫자만 넣을것!!!
{
    if (digit > 9) digit = 9;
    data595 = (data595 & 0xFF00u) | (uint16_t)fndData[digit]; //뒷쪽 8비트를 바꾸기
}

void showBar(uint8_t n)//0~8만 넣을것!!!
{
    uint8_t bar = (uint8_t)((1u << n) - 1u); //하위비트 n개가 1인 비트마스크 생성
    data595 = (data595 & 0x00FFu) | ((uint16_t)bar << 8); //앞쪽 8비트를 교체하기
}

void delay_us(uint16_t us)
{
    uint16_t start = __HAL_TIM_GET_COUNTER(&htim11);
    while((uint16_t)(__HAL_TIM_GET_COUNTER(&htim11) - start) < us);
}

void dataOut(bool *arr16){
    // ext16[0..7]  : 3번째 74HC595 QA..QH
    // ext16[8..15] : 4번째 74HC595 QA..QH
    uint8_t reg3 = packByteFromBool(&arr16[0]);
    uint8_t reg4 = packByteFromBool(&arr16[8]);


    //   [31:24] -> 4번째, [23:16] -> 3번째, [15:8] -> 2번째, [7:0] -> 1번째
    uint32_t out32 = ((uint32_t)reg4 << 24) | ((uint32_t)reg3 << 16) | (uint32_t)data595;

    for (int i = 31; i >= 0; i--)
    {
        if (out32 & (1u << i))
            HAL_GPIO_WritePin(GPIO_PORT, SER_PIN, GPIO_PIN_SET);
        else
            HAL_GPIO_WritePin(GPIO_PORT, SER_PIN, GPIO_PIN_RESET);

        // 시프트 클럭
        delay_us(2);
        HAL_GPIO_WritePin(GPIO_PORT, SRCLK_PIN, GPIO_PIN_SET);
        delay_us(2);
        HAL_GPIO_WritePin(GPIO_PORT, SRCLK_PIN, GPIO_PIN_RESET);
        delay_us(2);
    }

    // 래치 클럭
    HAL_GPIO_WritePin(GPIO_PORT, RCLK_PIN, GPIO_PIN_SET);
    delay_us(2);
    HAL_GPIO_WritePin(GPIO_PORT, RCLK_PIN, GPIO_PIN_RESET);
}
