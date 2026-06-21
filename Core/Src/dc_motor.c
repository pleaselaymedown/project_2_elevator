/*
 * dc_motor.c
 *
 *  Created on: 2026. 2. 7.
 *      Author: balls
 */

#include "dc_motor.h"

void rotateMotor(DCMotor_t *DCMotor, uint8_t direction){
	if(direction == DIR_CW){//시계방향이라면 dirA핀을 켜고 dirB핀 끄기
		HAL_GPIO_WritePin(DCMotor->dirAPort, DCMotor->dirAPin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(DCMotor->dirBPort, DCMotor->dirBPin, GPIO_PIN_RESET);
	}else{//반시계방향이라면 dirB핀을 켜고 dirA핀 끄기
		HAL_GPIO_WritePin(DCMotor->dirAPort, DCMotor->dirAPin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(DCMotor->dirBPort, DCMotor->dirBPin, GPIO_PIN_SET);
	}
}

void stopMotor(DCMotor_t *DCMotor){//멈출땐 다끄기
	HAL_GPIO_WritePin(DCMotor->dirAPort, DCMotor->dirAPin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(DCMotor->dirBPort, DCMotor->dirBPin, GPIO_PIN_RESET);
}


