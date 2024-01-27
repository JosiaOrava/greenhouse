#include "HumTemp.h"
//ModbusMaster object to communicate with Vaisala HMP60 temperature and humidity sensor (address 241)
ModbusMaster* HumTemp::node = new ModbusMaster(241);
ModbusRegister* HumTemp::humBus = new ModbusRegister(HumTemp::node, 256, true);
ModbusRegister* HumTemp::tempBus = new ModbusRegister(HumTemp::node, 257, true);

void HumTemp::init(){
	node->begin(9600);
}

int HumTemp::readHumidity(){
	return humBus->read();
}
int HumTemp::readTemperature(){
	return tempBus->read();
}



