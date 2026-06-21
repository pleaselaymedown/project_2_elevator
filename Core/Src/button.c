/* button.c */
#include "button.h"

void Button_Init(Button_t* pBtn, GPIO_TypeDef* port, uint16_t pin, GPIO_PinState onState) {
    pBtn->port = port;
    pBtn->pin = pin;
    pBtn->onState = onState;
    pBtn->prevTime = 0;
    pBtn->isPressed = false;
}

bool Button_GetPressed(Button_t* pBtn) {
    // 1. 현재 핀 상태 읽기
    GPIO_PinState currentState = HAL_GPIO_ReadPin(pBtn->port, pBtn->pin);
    uint32_t currTime = HAL_GetTick();

    // 2. 버튼이 눌려있는가? (물리적 상태 확인)
    if (currentState == pBtn->onState) {
        // 3. 눌린 상태가 50ms 이상 유지되었는가? (디바운싱)
        if (currTime - pBtn->prevTime >= 50) {
            // 4. 이전에 처리된 적이 없는가? (Rising Edge)
            if (pBtn->isPressed == false) {
                pBtn->isPressed = true; // 처리됨 플래그
                return true;            // 입력 감지 성공!
            }
        }
    }
    else {
        // 5. 버튼을 떼면 타이머 리셋
        // 버튼이 안 눌린 상태일 때만 시간을 현재 시간으로 계속 갱신함
        // 그래야 버튼을 딱 누르는 순간부터 시간이 흐르기 시작함
        pBtn->prevTime = currTime;
        pBtn->isPressed = false;
    }

    return false;
}
