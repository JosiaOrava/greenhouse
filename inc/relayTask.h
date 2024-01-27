
#ifndef RELAYTASK_H_
#define RELAYTASK_H_

#include "DigitalIoPin.h"

#define RELAY_PORT 0
#define RELAY_PIN 27

// Task priority, since it polls we make it fairly low
#define REL_TASK_PRIORITY 1UL

// Unused hysteresis to make reactions slower, switched to timers
#define RELAY_HYSTERESIS 10

// Lengths of the timers
#define TIMER_VALVE_OPEN 4000
#define TIMER_VALVE_CLOSE 13000

void vRelayTask(void* pvParameters);

#endif
