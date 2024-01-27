# Greenhouse Controller
Greenhouse Product helps you maintain optimal conditions for plant growth by monitoring temperature & humidity with automatic CO2 control. We had greenhouse avaivable in school with sensors, relay, co2 tank, mcu etc. avaivable for us to poke around.

## Key Features

* CO2: Shows real-time CO2 levels in your greenhouse.
* Temperature and Humidity: Monitors environmental conditions.
* Setpoint Control: Adjust the desired CO2 level using the rotary encoder.
* Automatic Valve Control: Regulates the CO2 tank valve to maintain the setpoint.

## Data Transmission

Every 5 minutes, the product sends sensor data to the cloud server via MQTT.This data transmission allows you to track and analyze greenhouse conditions remotely. We used [thingspeak](https://thingspeak.com/) to host our MQTT gathered data and display it.

## CO2 Valve Control

The product automatically controls the CO2 tank valve to maintain the desired setpoint. No manual intervention is needed for CO2 regulation. Because sensor data comes with quite a big latency the relay has maximum time of 5 seconds being open. That way it is insurred that device wont flood the greenhouse with CO2. Relay is being kept close for 15 seconds before it can be activated again. Trough a lot of testing it was noted that the 5 second opening time was always enough for CO2 level to reach setpoint. 

## Credits
* [Josia Orava](https://github.com/JosiaOrava)
* [Miro Tuovinen](https://github.com/1UPNuke)
* [Jussi Enne](https://github.com/dredxster)