#include "AlertController.h"
#include <Arduino.h>

AlertController::AlertController(int ledPin, int buzzerPin)
    : ledPin(ledPin), buzzerPin(buzzerPin), alertMode(Off) {
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
}

void AlertController::setAlert(bool on) {
  if (alertMode == Off || alertMode == Steady) {
    digitalWrite(ledPin, on ? HIGH : LOW);
    digitalWrite(buzzerPin, on ? HIGH : LOW);
  } else if (alertMode == Flashing) {
    digitalWrite(ledPin, !digitalRead(ledPin));
    digitalWrite(buzzerPin, !digitalRead(buzzerPin));
  }
}

void AlertController::setAlertMode(AlertMode mode) {
  alertMode = mode;
}
