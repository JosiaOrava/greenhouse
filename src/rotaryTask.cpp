
#include <rotaryTask.h>
#include "board.h"
#include "chip.h"
#include "FreeRTOS.h"
#include "heap_lock_monitor.h"
#include "semphr.h"
#include "Co2.h"

// Internal semaphore to notify task that interrupt was triggered
SemaphoreHandle_t rotarySemphr = xSemaphoreCreateBinary();
// Semaphores that were meant to communicate rotary events to other tasks, unused
SemaphoreHandle_t rotaryCWSemphr = xSemaphoreCreateBinary();;
SemaphoreHandle_t rotaryCCWSemphr = xSemaphoreCreateBinary();;

extern "C" {
// Interrupt handler
void ROT_SIG_A_PININT_HANDLER(void)
{
	portBASE_TYPE xHigherPriorityWoken = pdFALSE;
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(ROT_SIG_A_PININT_I));

	// Notify task that interrupt was triggered
	xSemaphoreGiveFromISR(rotarySemphr, &xHigherPriorityWoken);

	portEND_SWITCHING_ISR(xHigherPriorityWoken);
}

}

void vRotaryTask(void* pvParameters) {

	// Define IO pins for sigA and sigB of rotary
	static DigitalIoPin sigA = DigitalIoPin(
		ROT_SIG_A_PORT, ROT_SIG_A_PIN,
		DigitalIoPin::input, false
	);
	static DigitalIoPin sigB = DigitalIoPin(
		ROT_SIG_B_PORT, ROT_SIG_B_PIN,
		DigitalIoPin::input, false
	);

	// Enable PININT clock
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_PININT);
	Chip_SYSCTL_PeriphReset(RESET_PININT);
	// Configure interrupt channel for the GPIO pin in INMUX block
	Chip_INMUX_PinIntSel(ROT_SIG_A_PININT_I, ROT_SIG_A_PORT, ROT_SIG_A_PIN);
	// Configure channel interrupt as edge sensitive and falling edge interrupt
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(ROT_SIG_A_PININT_I));
	Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH(ROT_SIG_A_PININT_I));
	Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH(ROT_SIG_A_PININT_I));
	// Enable interrupt in the NVIC
	NVIC_ClearPendingIRQ(ROT_SIG_A_PININT_NVIC);
	NVIC_EnableIRQ(ROT_SIG_A_PININT_NVIC);

	while(1) {
		// Wait for the interrupt to trigger
		if(xSemaphoreTake( rotarySemphr, portMAX_DELAY )) {
			// Determine the direction based on sigA and sigB
			int dir = sigA.read() == sigB.read();
			if(dir == ROT_CW) {
				xSemaphoreGive(rotaryCWSemphr);
				Co2::increaseSetpoint();
			} else if (dir == ROT_CCW) {
				xSemaphoreGive(rotaryCCWSemphr);
				Co2::decreaseSetpoint();
			}
		}
	}
}
