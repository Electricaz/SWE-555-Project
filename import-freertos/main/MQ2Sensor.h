#ifndef MQ2_SENSOR_H
#define MQ2_SENSOR_H

#include <Arduino.h>

class MQ2Sensor {
public:
  explicit MQ2Sensor(uint8_t pin);
  bool read();
  
private:
  uint8_t _pin;
};

#endif // MQ2_SENSOR_H
