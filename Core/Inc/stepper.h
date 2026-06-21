
#ifndef INC_STEPPER_H_
#define INC_STEPPER_H_


#include "stm32f4xx_hal.h"


#define STEPS_PER_REV 4096
#define DIR_CW 0
#define DIR_CCW 1

//하프스텝 사용시 사용하는 핀
//#define IN1_PIN GPIO_PIN_1
//#define IN2_PIN GPIO_PIN_15
//#define IN3_PIN GPIO_PIN_14
//#define IN4_PIN GPIO_PIN_13
//#define IN_PORT GPIOB


void stepMotor(uint8_t step);
void rotateSteps(uint32_t steps, uint8_t dir);
void rotateDegrees(uint16_t degrees, uint8_t dir);
void stepperInit();
void rotateStart(uint8_t dir);
void rotateStop();
void setSpeed(uint32_t);
void stepperTick();



typedef struct{
	uint32_t step; //지금 몇스텝까지 진행했나
	uint32_t stepGoal;//몇스텝까지 진행 해야하나
	uint8_t dir;//방향
	//추후 여러 객체를 사용하게 확장할경우 이곳에 타이머 핸들러를 추가해야함
} Stepper_t;

#endif /* INC_STEPPER_H_ */
