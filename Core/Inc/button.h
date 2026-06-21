/* button.h */
#ifndef INC_BUTTON_H_
#define INC_BUTTON_H_

#include "stm32f4xx_hal.h"
#include <stdbool.h>

typedef struct {//각 버튼들을 관리하는 구조체 입니다.
    GPIO_TypeDef* port;
    uint16_t      pin;
    GPIO_PinState onState;//버튼을 눌렀을때 low인지 high인지 정의합니다.
    uint32_t      prevTime;//디바운싱을 위한 이전 누름시간 기록
    bool          isPressed;//눌렸다고 이미 반환 했니?
} Button_t;

void Button_Init(Button_t* pBtn, GPIO_TypeDef* port, uint16_t pin, GPIO_PinState onState);
bool Button_GetPressed(Button_t* pBtn);

#endif /* INC_BUTTON_H_ */
