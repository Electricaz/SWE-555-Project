#include "MQ2Sensor.h"

MQ2Sensor::MQ2Sensor(uint8_t pin) : _pin(pin) {
  pinMode(_pin, INPUT);
}

bool MQ2Sensor::read() {
  int value = digitalRead(_pin);
  return value == HIGH;
}
