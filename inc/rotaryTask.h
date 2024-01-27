
#ifndef ROTARYTASK_H_
#define ROTARYTASK_H_

#include "DigitalIoPin.h"
#include "FreeRTOS.h"
#include "semphr.h"

// Rotary sigA interrupt definitions, numbers must match
#define ROT_SIG_A_PININT_I 0
#define ROT_SIG_A_PININT_HANDLER PIN_INT0_IRQHandler
#define ROT_SIG_A_PININT_NVIC PIN_INT0_IRQn

// Rotary button interrupt definitions, unused
#define ROT_BTN_PININT_I 1
#define ROT_BTN_PININT_HANDLER PIN_INT1_IRQHandler
#define ROT_BTN_PININT_NVIC PIN_INT1_IRQn


#define ROT_CW 1
#define ROT_CCW 0

// IO pin ports and numbers
#define ROT_SIG_A_PORT 0
#define ROT_SIG_A_PIN 5
#define ROT_SIG_B_PORT 0
#define ROT_SIG_B_PIN 6

// Priority for the rotary task, high since it uses interrupts
#define ROT_TASK_PRIORITY 5UL


extern SemaphoreHandle_t rotaryCWSemphr;
extern SemaphoreHandle_t rotaryCCWSemphr;

void vRotaryTask(void* pvParameters);

#endif
