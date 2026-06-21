
#include "stepper.h"
#include "tim.h"

static Stepper_t stepper;//스테퍼 구조체, 스테퍼 여러개 사용시 이 정적 변수를 지우고 메인에 필요한 만큼 만들고 포인터로 함수에 넘길 것!!!, 또한 타이머도 구조체에 넣을것!!
//포인터로 함수에 넘길 필요가 있을때는 Stepper와 타이머를 사용하는 함수에 Stepper *stepper 라는 매개변수를 추가할 것!!!(간편하게 확장 가능한 설계)

//static const uint8_t HALF_STEP_SEQ[8][4] = { //step 1~8
//		{1,0,0,0},
//		{1,1,0,0},
//		{0,1,0,0},
//		{0,1,1,0},
//		{0,0,1,0},
//		{0,0,1,1},
//		{0,0,0,1},
//		{1,0,0,1},
//};


//0 ~ 255 PWM Duty
static const uint8_t MICRO_STEP_SEQ_64[64][4] = {
    // --- Phase 1: Pin 0(255->0) & Pin 1(0->255) ---
    {255,   0,   0,   0}, // Step 0
    {253,  24,   0,   0},
    {250,  49,   0,   0},
    {244,  74,   0,   0},
    {235,  97,   0,   0},
    {224, 120,   0,   0},
    {212, 141,   0,   0},
    {197, 161,   0,   0},
    {180, 180,   0,   0}, // Step 8 (45도, 70.7%)
    {161, 197,   0,   0},
    {141, 212,   0,   0},
    {120, 224,   0,   0},
    { 97, 235,   0,   0},
    { 74, 244,   0,   0},
    { 49, 250,   0,   0},
    { 24, 253,   0,   0},

    // --- Phase 2: Pin 1(255->0) & Pin 2(0->255) ---
    {  0, 255,   0,   0}, // Step 16
    {  0, 254,  25,   0},
    {  0, 250,  50,   0},
    {  0, 244,  74,   0},
    {  0, 236,  98,   0},
    {  0, 225, 120,   0},
    {  0, 212, 142,   0},
    {  0, 197, 162,   0},
    {  0, 180, 180,   0}, // Step 24
    {  0, 162, 197,   0},
    {  0, 142, 212,   0},
    {  0, 120, 225,   0},
    {  0,  98, 236,   0},
    {  0,  74, 244,   0},
    {  0,  50, 250,   0},
    {  0,  25, 254,   0},

    // --- Phase 3: Pin 2(255->0) & Pin 3(0->255) ---
    {  0,   0, 255,   0}, // Step 32
    {  0,   0, 254,  25},
    {  0,   0, 250,  50},
    {  0,   0, 244,  74},
    {  0,   0, 236,  98},
    {  0,   0, 225, 120},
    {  0,   0, 212, 142},
    {  0,   0, 197, 162},
    {  0,   0, 180, 180}, // Step 40
    {  0,   0, 162, 197},
    {  0,   0, 142, 212},
    {  0,   0, 120, 225},
    {  0,   0,  98, 236},
    {  0,   0,  74, 244},
    {  0,   0,  50, 250},
    {  0,   0,  25, 254},

    // --- Phase 4: Pin 3(255->0) & Pin 0(0->255) ---
    {  0,   0,   0, 255}, // Step 48
    { 25,   0,   0, 254},
    { 50,   0,   0, 250},
    { 74,   0,   0, 244},
    { 98,   0,   0, 236},
    {120,   0,   0, 225},
    {142,   0,   0, 212},
    {162,   0,   0, 197},
    {180,   0,   0, 180}, // Step 56
    {197,   0,   0, 162},
    {212,   0,   0, 142},
    {225,   0,   0, 120},
    {236,   0,   0,  98},
    {244,   0,   0,  74},
    {250,   0,   0,  50},
    {254,   0,   0,  25},
};


void stepMotor(uint8_t step){//모터에 step번째 스텝 적용
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, MICRO_STEP_SEQ_64[step][0]);//PWM 듀티 설정
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, MICRO_STEP_SEQ_64[step][1]);//PWM 듀티 설정
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, MICRO_STEP_SEQ_64[step][2]);//PWM 듀티 설정
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, MICRO_STEP_SEQ_64[step][3]);//PWM 듀티 설정
}

void rotateSteps(uint32_t steps, uint8_t dir){//steps스텝만큼 dir방향으로 회전
	stepper.step = 0;
	stepper.stepGoal=steps;//스텝 목표설정
	stepper.dir=dir;//회전방향 설정
	HAL_TIM_Base_Start_IT(&htim10);//타이머 10의 인터럽트를 활성화 하여 stepsTick이 호출되게 해 회전을 시작함
}

void stepperTick(){//얘는 타이머 10의 인터럽트 콜백 함수에서 주기적으로 호출되며 스텝을 진행시킴
	uint32_t step=0;
	if(stepper.dir==DIR_CW){//회전 방향 결정
				step=stepper.step%64;//시계방향시 원래 순서대로 스텝 증가
			}
			else{
				step=63-(stepper.step %64);//반시계라면 스텝 순서 거꾸로
			}
			stepMotor(step);
			if(++stepper.step >= stepper.stepGoal && stepper.stepGoal != 0){//스텝 목표가 0이 아니면 목표까지만 회전하고 멈춤, 0이라면 계속 회전
				HAL_TIM_Base_Stop_IT(&htim10);
			}
}

void rotateDegrees(uint16_t degrees, uint8_t dir){
	uint32_t steps = ((uint32_t)(degrees * STEPS_PER_REV)/360)*8;//스텝 수가 8에서 64로 늘어난 만큼 8배 더 회전해야 하니 곱하기 8 추가
	rotateSteps(steps, dir);
}

void rotateStart(uint8_t dir){//정지하기 전까지 계속 회전하는 함수
	rotateSteps(0, dir);
}

void rotateStop(){//스텝 정지
	HAL_TIM_Base_Stop_IT(&htim10);//타이머 10의 인터럽트를 비활성화
}

void setSpeed(uint32_t frequency){//주파수를 입력받아 타이머 10의 주기를 조절하여 stepsTick()이 호출되는 주기를 조절함(각 스텝을 진행하는 주기를 변화함)
	//주의!! 주파수는 16Hz보다 낮출 수 없음!
	__HAL_TIM_SET_AUTORELOAD(&htim10, (1000000 / frequency) - 1 );//타이머 ARR레지스터에 값을 설정
}

void stepperInit(){
	//타이머 시작
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
	//구조체 초기화(추후 여러 객체를 사용하게 확장할경우 아래 내용은 삭제해야함)
	stepper.dir = DIR_CW;
	stepper.step=0;
	stepper.stepGoal=0;
}

