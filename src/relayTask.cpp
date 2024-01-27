
#include "relayTask.h"
#include "board.h"
#include "chip.h"
#include "FreeRTOS.h"
#include "heap_lock_monitor.h"
#include "task.h"
#include "Co2.h"
#include "stdlib.h"


void vRelayTask(void* pvParameters) {
	static DigitalIoPin relay = DigitalIoPin(
			RELAY_PORT, RELAY_PIN,
			DigitalIoPin::output, false
	);

	long int timer, timer2;
	bool timer1Active = false, timer2Active = false;
	while(1) {
		// Wait a second
		vTaskDelay(1000);
		// Get CO2 sensor data and setpoint
		int setpoint = Co2::getSetpoint();
		int sensor = Co2::readSensor();

		if(!(sensor <= 0)) {
			// If setpoint higher than value read
			if(setpoint > sensor) {
				// If neither timer is active
				if(!timer1Active && !timer2Active){
					// Start timer one
					timer = xTaskGetTickCount();
					timer1Active = true;
					printf("Timer 1: Activated\n");
				}
				// Check if the valve opening time is exceeded. If not, then keep relay open
				if(timer1Active && (xTaskGetTickCount() - timer) < TIMER_VALVE_OPEN){
					printf("Timer 1: %d\n", timer);
					// Open the relay
					relay.write(true);
				} else {
					printf("Timer 2: %d\n", timer2);
					// Close the relay
					relay.write(false);
					timer1Active = false;
					printf("Timer 1: Deactivated\n");
					// Make sure the relay cannot stay closed forever
					if(!timer2Active){
						timer2Active = true;
						printf("Timer 2: Activated\n");
						timer2 = xTaskGetTickCount();
					}
					if (timer2Active && (xTaskGetTickCount() - timer2) > TIMER_VALVE_CLOSE) {
						timer2Active = false;
						timer1Active = false;
						timer2 = 0;
						timer = 0;
					}
				}
			}
			else {
				// Close the relay
				relay.write(false);
			}
			printf("Setpoint: %d, Sensor: %d, Relay: %d\n", setpoint, sensor, setpoint > sensor);
		}

	}

}
