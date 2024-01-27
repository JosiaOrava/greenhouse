
#include "Co2.h"
#include "eeprom.h"
#include "ModbusMaster.h"
#include "ModbusRegister.h"

int Co2::setpoint = 0;
//ModbusMaster object to communicate with Vaisala GMP252 CO2 probe (address 240)
ModbusMaster* Co2::node = new ModbusMaster(240);
ModbusRegister* Co2::bus = new ModbusRegister(Co2::node, 256, true);

void Co2::init(){
	//Enable EEPROM clock and reset EEPROM controller
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_EEPROM);
	Chip_SYSCTL_PeriphReset(RESET_EEPROM);

	//Initialize communication with Vaisala GMP252 CO2 probe at 9600 bps
	node->begin(9600);

	//Suspend scheduler to safely access EEPROM
	vTaskSuspendAll();
	Chip_EEPROM_Read(0, (uint8_t *) &Co2::setpoint, 4);
	xTaskResumeAll();
}

int Co2::readSensor(){
	return bus->read();
}

void Co2::increaseSetpoint(){
	setpoint++;
	vTaskSuspendAll();
	Chip_EEPROM_Write(0, (uint8_t *) &setpoint, 4);
	xTaskResumeAll();
}

void Co2::decreaseSetpoint(){
	if(setpoint > 0){
		setpoint--;
	}
	vTaskSuspendAll();
	Chip_EEPROM_Write(0, (uint8_t *) &setpoint, 4);
	xTaskResumeAll();
}

int Co2::getSetpoint(){	
	return setpoint;
}


