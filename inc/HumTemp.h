
#ifndef HUMTEMP_H_
#define HUMTEMP_H_
#include "ModbusMaster.h"
#include "ModbusRegister.h"

class HumTemp {
public:
	HumTemp();
	virtual ~HumTemp();
	int readHumidity();
	int readTemperature();
private:
	ModbusMaster humTempNode;
	ModbusRegister humBus;
	ModbusRegister tempBus;
};



#endif /* HUMTEMP_H_ */
