/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "button.h"
#include "stepper.h"
#include "stdbool.h"
#include "FND_595.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum{
	NONE,
	UP,
	DOWN
}DirectionState;

typedef enum{
	STOP,
	DECEL,
	ACCEL,
}MotorState;

typedef enum{
	DOOR_MOTOR_STOP,
	DOOR_MOTOR_CLOSE,
	DOOR_MOTOR_OPEN,
}DoorMotorState;

typedef enum{
	CLOSING,
	CLOSED,
	OPENING,
	OPEN,
}DoorState;


typedef enum{
	INIT,
	IDLE,
	MOVING,
	DOOR,
	HALT
}SystemState;

typedef struct{
	GPIO_PinState sensorPinState;
	GPIO_TypeDef* sensorPort;
	uint16_t sensorPin;
}Sensor;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */


#define TOTAL_FLOORS 4 //층갯수
#define DOOR_TIMEOUT_MS 4000 //4초 이상 걸리면 끼임/고장 판단
#define DOOR_OPEN_TIME_MS 10000 //문 열려있는 시간
#define DOOR_FAIL_LIMIT 4//이 횟수이상 문 동작 실패시 시스템 정지
#define BUZZER_PERIOD_MS 200
#define LED_BAR_PERIOD_MS 150
#define CHECK_INTERVAL  10   // 10ms마다 센서 확인
#define SCORE_UP        2    // 감지될 때 올라가는 점수
#define SCORE_DOWN      1    // 감지 안 될 때 내려가는 점수
#define TRIGGER_SCORE   20   // 경보가 울릴 임계점 (최대 100 기준 50점)

#define UPPER_SENSOR_PORT GPIOC
#define LOWER_SENSOR_PORT GPIOC
#define LOW_LIMIT_SENSOR_PORT GPIOC
#define DOOR_CLOSED_SENSOR_PORT GPIOC
#define DOOR_OPEN_SENSOR_PORT GPIOC
#define PIR_SENSOR_PORT GPIOC
#define FLAME_SENSOR_PORT GPIOC
#define DOOR_MOTOR_CW_PORT GPIOC
#define DOOR_MOTOR_CCW_PORT GPIOD
#define BUZZER_PORT GPIOA
#define LED_BAR_PORT GPIOB

#define UPPER_SENSOR_PIN GPIO_PIN_0
#define LOWER_SENSOR_PIN GPIO_PIN_1
#define LOW_LIMIT_SENSOR_PIN GPIO_PIN_2
#define DOOR_CLOSED_SENSOR_PIN GPIO_PIN_3
#define DOOR_OPEN_SENSOR_PIN GPIO_PIN_4
#define PIR_SENSOR_PIN GPIO_PIN_11
#define FLAME_SENSOR_PIN GPIO_PIN_10
#define DOOR_MOTOR_CW_PIN GPIO_PIN_12
#define DOOR_MOTOR_CCW_PIN GPIO_PIN_2
#define BUZZER_PIN GPIO_PIN_4
#define LED_BAR_PIN1 GPIO_PIN_2
#define LED_BAR_PIN2 GPIO_PIN_3
#define LED_BAR_PIN3 GPIO_PIN_4
#define LED_BAR_PIN4 GPIO_PIN_5
#define LED_BAR_PIN5 GPIO_PIN_6
#define LED_BAR_PIN6 GPIO_PIN_7
#define LED_BAR_PIN7 GPIO_PIN_8
#define LED_BAR_PIN8 GPIO_PIN_9

// 스텝모터(속도 프로파일) 관련
#define PROFILE_DT_MS 10              // 속도 프로파일 업데이트 주기(ms)
#define MAX_FREQUENCY 8000
#define MIN_FREQUENCY 2000
#define A_MAX_HZ_PER_S 4000.0f        // 최대 가속도(Hz/s)
#define J_MAX_HZ_PER_S2 50000.0f      // 최대 저크(Hz/s^2)

//moving timeout 추가예정(아마도)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

//예약 관리 배열, 배열의 각 인덱스가 해당 층의 버튼을 나타냄
bool hallCallUp[TOTAL_FLOORS] = {0,}; //외부에서 위로 버튼을 눌렀는가
bool hallCallDown[TOTAL_FLOORS] = {0,}; //외부에서 아래로 버튼을 눌렀는가
bool carCall[TOTAL_FLOORS] = {0,}; //내부에서 어떤 층 버튼을 눌렀는가(예약/취소)

//층 관련
uint8_t currentFloor = 1; //현재 층
uint8_t nextStopFloor = 0; //목적지 층

//현재 상태
DirectionState currentDirection=NONE;
DirectionState preferredDirection = UP; // IDLE에서 방향 유지/우선순위에 사용
MotorState currentMotorState=STOP;
DoorState currentDoorState=CLOSED;
SystemState currentSystemState=INIT;

//센서 상태
volatile GPIO_PinState upperSensor; //카 상부 센서
volatile GPIO_PinState lowerSensor; //카 하부 센서
volatile GPIO_PinState lowLimitSensor; //초기화시 사용하는 센서
volatile GPIO_PinState doorClosedSensor; //문 닫힘 감지 센서
volatile GPIO_PinState doorOpenSensor; //문 열림 감지 센서
volatile GPIO_PinState PIRSensor; //인체 감지센서
volatile GPIO_PinState flameSensor; //불 감지센서

//기타 전역 변수
uint32_t lastStateTick = 0; // 마지막 상태 변경 시각 기록용
bool isEmergency = false; // 비상 상황 플래그
bool systemHalted = false; // 시스템 완전 정지 플래그
uint8_t doorFailCount = 0; //문 여닫힘 실패 카운트
volatile uint8_t doorOpenCounter = 8;
bool clearBothAtStop = false; // 회차(runLimit) 정지면 홀콜 양방향을 같이 클리어
uint8_t buzzerCounter = 0;
uint32_t lastBuzzed = 0;
uint32_t lastLedSeqTick = 0;
int8_t ledSequence = 0;

//버튼 선언
Button_t btn1FUp;
Button_t btn2FUp;
Button_t btn2FDown;
Button_t btn3FUp;
Button_t btn3FDown;
Button_t btn4FDown;
Button_t btnCar1F;
Button_t btnCar2F;
Button_t btnCar3F;
Button_t btnCar4F;
Button_t btnEmergency;
Button_t btnOpen;
Button_t btnClose;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	// TIM10에서 인터럽트가 발생했는지 확인
	if (htim->Instance == TIM10) {
		stepperTick(); // 스테퍼 모터 스텝 진행 함수 호출
	}
	if (htim->Instance == TIM9){
		if(doorOpenCounter>0 && HAL_GPIO_ReadPin(btnOpen.port, btnOpen.pin)!=btnOpen.onState)doorOpenCounter--;
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){//포토인터럽터에 인터럽트 발생시 상태 업데이트, 지금 인터럽트는 사실상 폴링 트리거, 추후 플래그 새우고 소모하는 방식으로 개선하면 더 좋을듯?
	if(GPIO_Pin == UPPER_SENSOR_PIN) upperSensor = HAL_GPIO_ReadPin(UPPER_SENSOR_PORT, UPPER_SENSOR_PIN);
	if(GPIO_Pin == LOWER_SENSOR_PIN) lowerSensor = HAL_GPIO_ReadPin(LOWER_SENSOR_PORT, LOWER_SENSOR_PIN);
	if(GPIO_Pin == LOW_LIMIT_SENSOR_PIN) lowLimitSensor = HAL_GPIO_ReadPin(LOW_LIMIT_SENSOR_PORT, LOW_LIMIT_SENSOR_PIN);
	if(GPIO_Pin == DOOR_CLOSED_SENSOR_PIN) doorClosedSensor = HAL_GPIO_ReadPin(DOOR_CLOSED_SENSOR_PORT, DOOR_CLOSED_SENSOR_PIN);
	if(GPIO_Pin == DOOR_OPEN_SENSOR_PIN) doorOpenSensor = HAL_GPIO_ReadPin(DOOR_OPEN_SENSOR_PORT, DOOR_OPEN_SENSOR_PIN);
}

// ===== 다음 정지층(nextStopFloor) 계산 =====
// - IDLE: 선호 방향(preferredDirection)을 우선으로 스캔하여 방향을 고른다.
// - MOVING: 진행 방향으로 '서비스 런(run)'의 끝(runLimit)까지 간다.
//           중간에는 (carCall + 같은 방향 홀콜)을 우선 정지하고,
//           runLimit에서는 방향과 무관한 홀콜도 정지(회차/방향전환을 위해).

void registerRequest(int floor, char type) {//층버튼 처리
	if (floor < 1 || floor > TOTAL_FLOORS) return; // 예외 처리

	int idx = floor - 1; //1층 -> 0번 인덱스

	switch (type) {
	case 'U': // 외부 윗버튼
		hallCallUp[idx] = true;
		break;
	case 'D': // 외부 아래버튼
		hallCallDown[idx] = true;
		break;
	case 'C': // 내부 버튼
		carCall[idx] = !carCall[idx]; // 토글 방식(취소 기능 이용시)
		//carCall[idx] = true; // 단순 등록 방식
		break;
	}
}

void clearRequest(int floor) {//해당 층의 내외부 버튼 끄기(방향 기반, 회차 정지(runLimit)면 양방향 홀콜도 처리)
	if (floor < 1 || floor > TOTAL_FLOORS) return;

	int idx = floor - 1; //1층 -> 0번 인덱스

	// 내부 호출은 정지하면 무조건 처리됨
	carCall[idx] = false;

	// 회차 정지(runLimit)에서는 문을 열어 양방향 승객을 태우는 걸로 보고 홀콜도 양방향 처리
	// (실제도 terminal/reversal stop에서는 반대방향 홀콜을 처리하는 경우가 많음)
	if (clearBothAtStop || currentDirection == NONE) {
		hallCallUp[idx] = false;
		hallCallDown[idx] = false;
		clearBothAtStop = false;
		return;
	}

	// 방향별 홀콜 처리(일반 정지)
	if (currentDirection == UP) {
		hallCallUp[idx] = false;
		// 최상층이면 하행 버튼도 같이 꺼줌
		if (floor == TOTAL_FLOORS) hallCallDown[idx] = false;
	}
	else if (currentDirection == DOWN) {
		hallCallDown[idx] = false;
		// 최하층이면 상행 버튼도 같이 꺼줌
		if (floor == 1) hallCallUp[idx] = false;
	}
}


void motorDrive(DirectionState dir, MotorState mode) { // 모터 제어코드(저크 제한 S-커브 비슷하게)

	// v(Hz): 속도(=스텝 주파수), a(Hz/s): 가속도
	static float v = MIN_FREQUENCY;
	static float a = 0.0f;

	static bool isRotating = false;
	static DirectionState rotatingDir = NONE;
	static uint32_t previousTick = 0;

	uint32_t now = HAL_GetTick();
	if (previousTick == 0) previousTick = now;

	// STOP은 즉시 정지
	if (mode == STOP) {
		if (isRotating) {
			v = MIN_FREQUENCY;
			a = 0.0f;
			setSpeed((uint32_t)v);
			rotateStop();
			isRotating = false;
			rotatingDir = NONE;
		}
		previousTick = now;
		return;
	}

	// ACCEL/DECEL에서 회전 시작(또는 방향 변경)
	if (!isRotating || rotatingDir != dir) {
		if (isRotating) {
			rotateStop();
			isRotating = false;
		}
		v = MIN_FREQUENCY;
		a = 0.0f;
		setSpeed((uint32_t)v);

		rotateStart(dir == UP ? DIR_CW : DIR_CCW);
		isRotating = true;
		rotatingDir = dir;
		previousTick = now;
		return; // 다음 주기부터 프로파일 갱신
	}

	// 프로파일 업데이트 주기 체크
	if (now - previousTick < PROFILE_DT_MS) return;

	float dt = (now - previousTick) / 1000.0f; // sec
	if (dt > 0.1f) dt = 0.1f;                  // 너무 큰 dt는 클램프(디버깅/중단 보호)
	previousTick = now;

	// 저크 제한으로 가속도를 목표값까지 서서히 이동
	float targetA = (mode == ACCEL) ? A_MAX_HZ_PER_S : -A_MAX_HZ_PER_S;
	float maxDa = J_MAX_HZ_PER_S2 * dt;

	float da = targetA - a;
	if (da >  maxDa) da =  maxDa;
	if (da < -maxDa) da = -maxDa;
	a += da;

	// 속도 적분
	v += a * dt;

	// 속도 제한
	if (v > MAX_FREQUENCY) { v = MAX_FREQUENCY; a = 0.0f; }
	if (v < MIN_FREQUENCY) { v = MIN_FREQUENCY; a = 0.0f; }

	setSpeed((uint32_t)v);
}



void doorMotorDrive(DoorMotorState mode) {
	switch(mode){
	case DOOR_MOTOR_STOP:
		HAL_GPIO_WritePin(DOOR_MOTOR_CW_PORT, DOOR_MOTOR_CW_PIN, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(DOOR_MOTOR_CCW_PORT, DOOR_MOTOR_CCW_PIN, GPIO_PIN_RESET);
		break;
	case DOOR_MOTOR_OPEN:
		HAL_GPIO_WritePin(DOOR_MOTOR_CW_PORT, DOOR_MOTOR_CW_PIN, GPIO_PIN_SET);
		HAL_GPIO_WritePin(DOOR_MOTOR_CCW_PORT, DOOR_MOTOR_CCW_PIN, GPIO_PIN_RESET);
		break;
	case DOOR_MOTOR_CLOSE:
		HAL_GPIO_WritePin(DOOR_MOTOR_CW_PORT, DOOR_MOTOR_CW_PIN, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(DOOR_MOTOR_CCW_PORT, DOOR_MOTOR_CCW_PIN, GPIO_PIN_SET);
		break;
	}
}



void triggerEmergency() {// 비상 상황 발생 시 호출할 함수
	isEmergency = true;
	// 모든 층 예약 (가장 가까운 층에 서기 위함)
	for(int i=0; i<TOTAL_FLOORS; i++) {
		carCall[i] = true;
	}
}

static uint8_t initStep = 0;
void elevatorFSMUpdate() {

	if ((lowLimitSensor==GPIO_PIN_SET && currentSystemState != INIT) || currentFloor > TOTAL_FLOORS || currentFloor == 0) { //오류로 층 범위 이탈시 시스템 즉시 정지(상단으로 이탈시 즉시 정지하는 센서?)
		currentSystemState = HALT;
		systemHalted = true;
	}

	// 시스템 완전 정지
	if (systemHalted) {
		motorDrive(NONE, STOP); // 모터 정지
		doorMotorDrive(DOOR_MOTOR_STOP);
		return;
	}

	// 현재 진행 방향에 따라 센서 방향 맵핑
	GPIO_PinState EntrySensing = (currentDirection == UP) ? upperSensor : lowerSensor;
	GPIO_PinState stopSensing  = (currentDirection == UP) ? lowerSensor : upperSensor;
	static GPIO_PinState prevEntrySensing = GPIO_PIN_RESET;
	static GPIO_PinState prevStopSensing  = GPIO_PIN_RESET;



	uint32_t currentTick = HAL_GetTick(); // 현재 틱

	// 메인 FSM
	switch (currentSystemState){

	// [상태 0: 초기화 (INIT)]
	case INIT:
		// 하부 리미트까지 하강 시작
		if (initStep == 0) {
			motorDrive(DOWN, ACCEL); // ACCEL로 한 번만 호출하면 가속 루틴을 타지 않고 최저 속도로 회전 시작
			initStep++;
		}

		// 최하단 리미트 센서 감지
		if (lowLimitSensor == GPIO_PIN_SET && initStep == 1) {
			motorDrive(NONE, STOP); // 정지
			HAL_Delay(100); //초기화 단계라 딜레이 한번쯤은 ㄱㅊㄱㅊ
			initStep++; // 다음 단계로
		}

		// 상승 시작 (1층 정렬 찾기)
		if (initStep == 2) {
			motorDrive(UP, ACCEL); // 최저 속도로 상승 시작
			initStep++;
		}

		// 카 하부 센서가 1층 위치에 도달함
		if (lowerSensor == GPIO_PIN_SET && initStep == 3) {
			motorDrive(NONE, STOP); // 정지


			// 상태 초기화 후 혹시 문 열려있을 수 있으니 문닫기, 닫고나면 IDLE로 전이됨
			currentMotorState = STOP;
			currentDirection = NONE;
			currentDoorState = CLOSING;
			currentSystemState = DOOR;
			lastStateTick = currentTick;
		}
		break;

		// [상태 1: 대기 (IDLE)]
	case IDLE:
		if (isEmergency) {// 비상인데 대기중이면 -> 즉시 문열고 시스템 정지
			currentSystemState = DOOR;
			currentDoorState = OPENING;
			lastStateTick = currentTick;
			break;
		}

		if(Button_GetPressed(&btnOpen) && currentDoorState == CLOSED){
			currentSystemState = DOOR;
			currentDoorState = OPENING;
			lastStateTick = currentTick;
			break;
		}

		// 0) 현재 층에 요청이 있으면 즉시 처리(문 열기)
		{
			int idx = (int)currentFloor - 1;
			if (idx >= 0 && idx < TOTAL_FLOORS) {
				if (carCall[idx] || hallCallUp[idx] || hallCallDown[idx]) {
					clearRequest(currentFloor);
					currentSystemState = DOOR;
					currentDoorState = OPENING;
					lastStateTick = currentTick;
					break;
				}
			}
		}

		// 1) 위/아래에서 가장 가까운 '요청 있는 층' 찾기
		uint8_t nearestUp = 0;
		for (int f = (int)currentFloor + 1; f <= TOTAL_FLOORS; f++) {
			int idx = f - 1;
			if (carCall[idx] || hallCallUp[idx] || hallCallDown[idx]) { nearestUp = (uint8_t)f; break; }
		}

		uint8_t nearestDown = 0;
		for (int f = (int)currentFloor - 1; f >= 1; f--) {
			int idx = f - 1;
			if (carCall[idx] || hallCallUp[idx] || hallCallDown[idx]) { nearestDown = (uint8_t)f; break; }
		}

		if (!nearestUp && !nearestDown) {
			motorDrive(NONE, STOP); // 요청 없음
			break;
		}

		// 2) 방향 유지(preferredDirection) 우선, 없으면 반대 방향 선택
		DirectionState dirChoice = (preferredDirection == DOWN) ? DOWN : UP; // 기본 UP

		if (dirChoice == UP) {
			if (nearestUp) { currentDirection = UP;   nextStopFloor = nearestUp; }
			else          { currentDirection = DOWN; nextStopFloor = nearestDown; }
		} else { // DOWN
			if (nearestDown) { currentDirection = DOWN; nextStopFloor = nearestDown; }
			else            { currentDirection = UP;   nextStopFloor = nearestUp; }
		}

		prevEntrySensing = (currentDirection == UP) ? upperSensor : lowerSensor;
		prevStopSensing  = (currentDirection == UP) ? lowerSensor : upperSensor;
		currentSystemState = MOVING;
		currentMotorState = ACCEL;
		break;

		// [상태 2: 이동 (MOVING)]

	case MOVING:
		// 이동 중에는 nextStopFloor 하나만 유지:
		// - 진행 방향 앞쪽에 정지 요청이 있으면 그 층
		// - 앞쪽 요청이 전부 사라지면(=취소) 다음 층에서 안전 정지

		if(doorClosedSensor != GPIO_PIN_SET){//만약 이동중 문이 완전히 안닫혔으면 즉시 정지
			currentSystemState = HALT;
			systemHalted = true;
			break;
		}

		switch (currentMotorState) {
		case ACCEL: {
			// === 고도화된 정지층 선택(collective control 비슷하게) ===
			// 1) 같은 방향 서비스: carCall + (UP면 hallUp / DOWN면 hallDown)
			// 2) runLimit(해당 방향으로 남아있는 요청의 끝)에서는 방향 무관 홀콜도 정지(회차/방향전환)
			// 3) 앞쪽에 요청이 0이면(전부 취소 or 반대쪽에만 존재) 다음 층에서 안전 정지

			uint8_t runLimit = 0;
			nextStopFloor = 0;

			if (currentDirection == UP) {
				// runLimit: 위쪽에서 요청이 존재하는 가장 높은 층
				for (int f = TOTAL_FLOORS; f >= (int)currentFloor + 1; f--) {
					int idx = f - 1;
					if (carCall[idx] || hallCallUp[idx] || hallCallDown[idx]) { runLimit = (uint8_t)f; break; }
				}

				if (runLimit) {
					for (int f = (int)currentFloor + 1; f <= TOTAL_FLOORS; f++) {
						int idx = f - 1;
						bool normalStop  = (carCall[idx] || hallCallUp[idx]); // 상행 서비스
						bool turningStop = ((uint8_t)f == runLimit);         // 런 끝은 방향 무관
						if (normalStop || turningStop) { nextStopFloor = (uint8_t)f; break; }
					}
				} else {
					// 앞쪽 요청 없음 -> 다음 층에서 정지
					nextStopFloor = (currentFloor < TOTAL_FLOORS) ? (currentFloor + 1) : 0;
				}
			}
			else if (currentDirection == DOWN) {
				// runLimit: 아래쪽에서 요청이 존재하는 가장 낮은 층
				for (int f = 1; f <= (int)currentFloor - 1; f++) {
					int idx = f - 1;
					if (carCall[idx] || hallCallUp[idx] || hallCallDown[idx]) { runLimit = (uint8_t)f; break; }
				}

				if (runLimit) {
					for (int f = (int)currentFloor - 1; f >= 1; f--) {
						int idx = f - 1;
						bool normalStop  = (carCall[idx] || hallCallDown[idx]); // 하행 서비스
						bool turningStop = ((uint8_t)f == runLimit);            // 런 끝은 방향 무관
						if (normalStop || turningStop) { nextStopFloor = (uint8_t)f; break; }
					}
				} else {
					// 앞쪽 요청 없음 -> 다음 층에서 안전 정지
					nextStopFloor = (currentFloor > 1) ? (currentFloor - 1) : 0;
				}
			}
			else {
				nextStopFloor = 0;
			}

			if (nextStopFloor == 0) { // 범위 밖(이상 상태)
				currentSystemState = HALT;
				systemHalted = true;
				break;
			}

			motorDrive(currentDirection, ACCEL); // 저크 제한 프로파일로 가속

			// 1차 센서(진입) 감지 (Rising Edge)
			if (EntrySensing == GPIO_PIN_SET && prevEntrySensing == GPIO_PIN_RESET) {
				// 현재 층 업데이트
				if (currentDirection == UP) currentFloor++;
				else currentFloor--;

				// 계획된 다음 정지층이면 감속 시작
				if (currentFloor == nextStopFloor) {
					// 런 끝(runLimit)에서 정지하는 경우: 문 열고 방향 전환을 할 수 있으므로 홀콜을 양방향 처리
					clearBothAtStop = (runLimit != 0 && nextStopFloor == runLimit);
					currentMotorState = DECEL;
				}
			}
			break;
		}

		case DECEL:
			motorDrive(currentDirection, DECEL); // 층 정렬및 정지를 위해 감속

			// 2차 센서(정지) 감지 (Rising Edge)
			// 층 간격이 좁아 "진입 센서"가 다음 층을 먼저 건드릴 때,
			// "정지 센서"가 아직 이전 층 마커에 걸려 GPIO_PIN_SET 상태일 수 있음.
			// 이 경우 레벨 감지(stopSensing==SET)로는 너무 빨리 정지하므로
			// LOW->HIGH 전이(라이징 엣지)에서만 정지한다.
			if (stopSensing == GPIO_PIN_SET && prevStopSensing == GPIO_PIN_RESET) {
				currentMotorState = STOP;
			}
			break;

		case STOP:
			motorDrive(NONE, STOP); // 모터 정지

			// 도착 처리(정지하면 그 층의 요청은 처리했다고 가정)
			clearRequest(currentFloor);

			//버저를 울리자
			buzzerCounter = currentFloor;

			// 방금 이동한 방향을 선호 방향으로 저장(방향 유지)
			if (currentDirection == UP || currentDirection == DOWN) {
				preferredDirection = currentDirection;
			}
			currentDirection = NONE;

			currentSystemState = DOOR;
			currentDoorState = OPENING; // 도착하면 문 염
			lastStateTick = currentTick;
			break;

		}
		prevEntrySensing = EntrySensing;
		prevStopSensing  = stopSensing;
		break;

		// [상태 3: 문 제어 (DOOR)]
		case DOOR:
			switch (currentDoorState) {

			// 3-1. 문 여는 중
			case OPENING:
				if(doorFailCount >= DOOR_FAIL_LIMIT){//문 동작 실패 횟수가 임계값을 넘으면 엘리베이터 시스템 정지
					currentSystemState = HALT;
					break;
				}

				doorMotorDrive(DOOR_MOTOR_OPEN); // 열기 동작

				// A. 문이 완전히 열림 (센서 감지)
				if (doorOpenSensor == GPIO_PIN_SET) {
					doorFailCount = 0; // 성공했으면 누적 실패 카운트 리셋
					currentDoorState = OPEN;
					lastStateTick = currentTick; // 타이머 리셋 (열려있는 시간 잼)
				}
				// B. 타임아웃 (여는 중 끼임/고장) -> 다시 닫아버리고 대기
				else if (currentTick - lastStateTick > DOOR_TIMEOUT_MS) {
					currentDoorState = CLOSING;
					lastStateTick = currentTick;
					doorFailCount++;
				}

				break;

				// 3-2. 문 열림 (대기)
			case OPEN:
				doorMotorDrive(DOOR_MOTOR_STOP); // 모터 정지

				// 비상 상황이면 문 열어두고 시스템 종료
				if (isEmergency) {
					HAL_TIM_Base_Stop_IT(&htim9);
					showBar(0);
					currentSystemState = HALT; // 상태 전이
					return;
				}

				if(Button_GetPressed(&btnClose)){//닫힘버튼 눌리면 바로 닫기
					currentDoorState = CLOSING;
					lastStateTick = currentTick;

					HAL_TIM_Base_Stop_IT(&htim9);
					__HAL_TIM_SET_COUNTER(&htim9,0);
					doorOpenCounter = 8;
					showBar(0);
					break;
				}

				// 일정 시간 경과 후 닫기
				HAL_TIM_Base_Start_IT(&htim9);
				showBar(doorOpenCounter);

				if(PIRSensor == GPIO_PIN_SET) __HAL_TIM_SET_PRESCALER(&htim9,10000);
				else __HAL_TIM_SET_PRESCALER(&htim9,5000);

				if (doorOpenCounter == 0) {
					showBar(0);
					currentDoorState = CLOSING;
					lastStateTick = currentTick;

					HAL_TIM_Base_Stop_IT(&htim9);
					__HAL_TIM_SET_COUNTER(&htim9,0);
					doorOpenCounter = 8;

					break;
				}
				//닫힘 버튼 눌리면 즉시 CLOSING하려면 여기 수정!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
				break;

				// 3-3. 문 닫는 중
			case CLOSING:
				if(doorFailCount >= DOOR_FAIL_LIMIT){//문 동작 실패 횟수가 임계값을 넘으면 엘리베이터 시스템 정지
					currentSystemState = HALT;
					break;
				}

				doorMotorDrive(DOOR_MOTOR_CLOSE); // 닫기 동작

				// A. 문이 완전히 닫힘 (센서 감지)
				if (doorClosedSensor == GPIO_PIN_SET) {
					doorFailCount = 0; // 성공했으면 누적 실패 카운트 리셋
					currentDoorState = CLOSED;
				}
				// B. 타임아웃 (닫는 중 끼임) -> 다시 열기 (Reversal)
				else if (currentTick - lastStateTick > DOOR_TIMEOUT_MS) {
					currentDoorState = OPENING;
					lastStateTick = currentTick;
					doorFailCount++;
				}

				if(Button_GetPressed(&btnOpen)){//열림버튼 누르면 열기
					currentDoorState = OPENING;
					lastStateTick = currentTick;
				}

				break;

			case CLOSED:
				doorMotorDrive(DOOR_MOTOR_STOP);
				currentSystemState = IDLE;
				break;
			}
			break;


			// [상태 4: 비상 정지 (HALT)]
			case HALT:
				clearBothAtStop = false;
				systemHalted = true;
				motorDrive(NONE, STOP);
				doorMotorDrive(DOOR_MOTOR_STOP);
				break;
	}
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_USART2_UART_Init();
	MX_TIM10_Init();
	MX_TIM3_Init();
	MX_TIM11_Init();
	MX_TIM9_Init();
	/* USER CODE BEGIN 2 */


	//타이머 시작
	HAL_TIM_Base_Start(&htim11);


	//버튼 초기화
	Button_Init(&btn1FUp,   	GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
	Button_Init(&btn2FUp,   	GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
	Button_Init(&btn2FDown, 	GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
	Button_Init(&btn3FUp,		GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
	Button_Init(&btn3FDown, 	GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
	Button_Init(&btn4FDown,		GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
	Button_Init(&btnCar1F,   	GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
	Button_Init(&btnCar2F,   	GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
	Button_Init(&btnCar3F,   	GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
	Button_Init(&btnCar4F,   	GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
	Button_Init(&btnOpen,   	GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
	Button_Init(&btnClose,   	GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
	Button_Init(&btnEmergency,	GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);

	//스테퍼 초기화
	stepperInit();

	//센서값 한번 읽어서 초기화
	upperSensor = HAL_GPIO_ReadPin(UPPER_SENSOR_PORT, UPPER_SENSOR_PIN);
	lowerSensor = HAL_GPIO_ReadPin(LOWER_SENSOR_PORT, LOWER_SENSOR_PIN);
	lowLimitSensor = HAL_GPIO_ReadPin(LOW_LIMIT_SENSOR_PORT, LOW_LIMIT_SENSOR_PIN);
	doorClosedSensor = HAL_GPIO_ReadPin(DOOR_CLOSED_SENSOR_PORT, DOOR_CLOSED_SENSOR_PIN);
	doorOpenSensor = HAL_GPIO_ReadPin(DOOR_OPEN_SENSOR_PORT, DOOR_OPEN_SENSOR_PIN);

	static uint32_t lastCheckTick = 0;
	static int8_t flameScore = false;
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{
		uint32_t currentMainTick = HAL_GetTick();

		if (Button_GetPressed(&btn1FUp))   registerRequest(1, 'U');
		if (Button_GetPressed(&btn2FUp))   registerRequest(2, 'U');
		if (Button_GetPressed(&btn2FDown)) registerRequest(2, 'D');
		if (Button_GetPressed(&btn3FUp)) registerRequest(3, 'U');
		if (Button_GetPressed(&btn3FDown)) registerRequest(3, 'D');
		if (Button_GetPressed(&btn4FDown)) registerRequest(4, 'D');
		if (Button_GetPressed(&btnCar1F))   registerRequest(1, 'C');
		if (Button_GetPressed(&btnCar2F))   registerRequest(2, 'C'); //실내 버튼 아직
		if (Button_GetPressed(&btnCar3F))   registerRequest(3, 'C');
		if (Button_GetPressed(&btnCar4F))   registerRequest(4, 'C');
		PIRSensor = HAL_GPIO_ReadPin(PIR_SENSOR_PORT, PIR_SENSOR_PIN);

		//화재시 비상모드
		if (HAL_GetTick() - lastCheckTick >= CHECK_INTERVAL) {
		    lastCheckTick = HAL_GetTick(); // 시간 갱신

		    // 센서 값 읽기 (GPIO_PIN_RESET이 감지 상태라고 가정)
		    if (HAL_GPIO_ReadPin(FLAME_SENSOR_PORT, FLAME_SENSOR_PIN) == GPIO_PIN_RESET) {
		        // 불꽃이 감지되면 점수 증가
		        flameScore += SCORE_UP;
		        if (flameScore > 100) flameScore = 100; // 최대 점수 제한
		    } else {
		        // 감지가 안 되면 점수 차감 (바로 0이 되지 않음!)
		        flameScore -= SCORE_DOWN;
		        if (flameScore < 0) flameScore = 0; // 0 미만 방지
		    }

		    // [판단] 점수가 임계치를 넘으면 화재 발생으로 간주
		    if (flameScore >= TRIGGER_SCORE) {
		        triggerEmergency();
		        // 한 번 트리거 후 점수를 유지할지, 초기화할지는 정책에 따라 결정
		        // flameScore = 0; // 반복 트리거를 원하면 주석 유지, 원치 않으면 주석 해제
		    }
		}

		//비상버튼 누르면 비상모드
		if (Button_GetPressed(&btnEmergency)) triggerEmergency();

		//엘리베이터 상태머신 구동
		elevatorFSMUpdate();

		//층표시
		showFND(currentFloor);


		bool leds[16] = {0};
		leds[0]=hallCallUp[0];
		leds[1]=hallCallDown[1];
		leds[2]=hallCallUp[1];
		leds[3]=hallCallDown[2];
		leds[4]=hallCallUp[2];
		leds[5]=hallCallDown[3];
		leds[6]=currentDirection==UP;
		leds[7]=currentDirection==DOWN;

		leds[8]=carCall[0];
		leds[9]=carCall[1];
		leds[10]=carCall[2];
		leds[11]=carCall[3];
		leds[12]=(HAL_GPIO_ReadPin(btnOpen.port, btnOpen.pin)==btnOpen.onState && (currentDoorState == CLOSING || (currentSystemState == DOOR && (currentDoorState == CLOSED || currentDoorState == OPENING)))); //열림버튼이 눌려있고 문이 닫히는 중이거나 엘리베이터 대기중에 문이 닫혀있다면 버튼에 불켜기
		leds[13]=(HAL_GPIO_ReadPin(btnClose.port, btnClose.pin)==btnClose.onState && (currentDoorState == OPEN || currentDoorState == CLOSING));
		leds[14]=isEmergency;
		dataOut(leds);


		if (currentMainTick - lastBuzzed > BUZZER_PERIOD_MS){ //층 도착시 버저를 해당 횟수만큼 울림(STOP에 횟수 지정되어 있음)
			if(HAL_GPIO_ReadPin(BUZZER_PORT, BUZZER_PIN)==GPIO_PIN_SET){
				HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
			}
			else if(buzzerCounter>0){
				HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
				buzzerCounter--;
			}
			lastBuzzed = currentMainTick;
		}


		if(currentMainTick-lastLedSeqTick>LED_BAR_PERIOD_MS){//led흐르기
			if(currentDirection == NONE) ledSequence = 0;
			else if(currentDirection == DOWN) ledSequence=(ledSequence+1)%9;
			else if(currentDirection == UP){
				ledSequence--;
				if(ledSequence<0)ledSequence=8;
			}

			switch(ledSequence){
			case 0:
				HAL_GPIO_WritePin(LED_BAR_PORT, LED_BAR_PIN1, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(LED_BAR_PORT, LED_BAR_PIN2, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(LED_BAR_PORT, LED_BAR_PIN3, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(LED_BAR_PORT, LED_BAR_PIN4, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(LED_BAR_PORT, LED_BAR_PIN5, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(LED_BAR_PORT, LED_BAR_PIN6, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(LED_BAR_PORT, LED_BAR_PIN7, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(LED_BAR_PORT, LED_BAR_PIN8, GPIO_PIN_RESET);
				break;
			case 1:
				HAL_GPIO_WritePin(LED_BAR_PORT, LED_BAR_PIN1, GPIO_PIN_SET);
				break;
			case 2:
				HAL_GPIO_WritePin(LED_BAR_PORT, LED_BAR_PIN2, GPIO_PIN_SET);
				break;
			case 3:
				HAL_GPIO_WritePin(LED_BAR_PORT, LED_BAR_PIN3, GPIO_PIN_SET);
				break;
			case 4:
				HAL_GPIO_WritePin(LED_BAR_PORT, LED_BAR_PIN4, GPIO_PIN_SET);
				break;
			case 5:
				HAL_GPIO_WritePin(LED_BAR_PORT, LED_BAR_PIN5, GPIO_PIN_SET);
				break;
			case 6:
				HAL_GPIO_WritePin(LED_BAR_PORT, LED_BAR_PIN6, GPIO_PIN_SET);
				break;
			case 7:
				HAL_GPIO_WritePin(LED_BAR_PORT, LED_BAR_PIN7, GPIO_PIN_SET);
				break;
			case 8:
				HAL_GPIO_WritePin(LED_BAR_PORT, LED_BAR_PIN8, GPIO_PIN_SET);
				break;
			}

			lastLedSeqTick = currentMainTick;
		}







		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 100;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 4;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
			|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
	{
		Error_Handler();
	}
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
	/* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
