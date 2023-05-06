#ifndef ALERT_CONTROLLER_H
#define ALERT_CONTROLLER_H

enum AlertMode { Off, Steady, Flashing };

class AlertController {
public:
  AlertController(int ledPin, int buzzerPin);
  void setAlert(bool on);
  void setAlertMode(AlertMode mode);

private:
  int ledPin;
  int buzzerPin;
  AlertMode alertMode;
};

#endif // ALERT_CONTROLLER_H
