
#ifndef CO2_H_
#define CO2_H_
#include "ModbusMaster.h"
#include "ModbusRegister.h"

class Co2 {
public:
	Co2();
	virtual ~Co2();
	int readSensor();
	void increaseSetpoint();
	void decreaseSetpoint();
	int getSetpoint();
private:
	int setpoint;
	ModbusMaster co2Node;
	ModbusRegister co2Bus;
};

#endif
