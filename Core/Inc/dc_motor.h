/*
 * dc_motor.h
 *
 *  Created on: Feb 8, 2026
 *      Author: seonmin
 */

#ifndef INC_DC_MOTOR_H_
#define INC_DC_MOTOR_H_

#include "stm32f4xx_hal.h"

#define DIR_CW 0      // 시계방향
#define DIR_CCW 1     // 반시계방향

typedef struct
{
  GPIO_TypeDef* dirAPort;
  GPIO_TypeDef* dirBPort;
  uint16_t dirAPin;
  uint16_t dirBPin;
}DCMotor_t;

void rotateMotor(DCMotor_t *DCMotor, uint8_t direction);
// DCMotor_t구조체에 모터드라이버에 연결되는 포트와 핀을 담아 넘기고 방향을 넘기면 해당 모터가 해당 방향으로 회전
void stopMotor(DCMotor_t *DCMotor);
// DCMotor_t구조체에 모터드라이버에 연결되는 포트와 핀을 담아 넘기면 해당 모터 정지



/*
 * 사용예시(만약 2개의 DC모터를 드라이버에 연결해서 사용한다면)
 *
 * DCMotor_t motorA;
 * DCMotor_t motorB; 일단 사용할 DC모터의 수만큼 구조체를 선언해준다
 *
 *
 * 그리고 메인 안에서 아래와 같이 핀과 포트를 구조체에 채운다.
 *
 * motorA.dirAPort = GPIOC;
 * motorA.dirBPort = GPIOC;
 * motorA.dirAPin = GPIO_PIN_8;
 * motorA.dirAPin = GPIO_PIN_6; 모터 A의 핀과 포트를 이렇게 설정해준다
 *
 * motorB.dirAPort = GPIOC;
 * motorB.dirBPort = GPIOC;
 * motorB.dirAPin = GPIO_PIN_1;
 * motorB.dirAPin = GPIO_PIN_2; 모터 B도 구조체에 설정한다.
 *
 *
 * 만약 모터 A를 시계방향으로 돌리고 싶다면
 * void rotateMotor(&motorA, DIR_CW);
 * 이런식으로 돌릴 모터의 구조체와 방향을 넣어 호출하면 된다!
 *
 */

#endif /* INC_DC_MOTOR_H_ */
