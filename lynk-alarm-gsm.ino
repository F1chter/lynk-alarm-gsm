//TODO CALL WHEN POWER ON


#include <Wire.h>
#include "LynkIP5306.h"
#include "LynkGsm.h"
#include <LittleFS.h>
#include "LynkFile.h"
#include "secrets.h"
#include "LynkTelegramBot.h"

#define DOOR_SENSOR_PIN 34
#define MOTION_SENSOR_PIN 15
#define LED_PIN 13
#define I2C_SDA 21
#define I2C_SCL 22
#define BATT_VOLTAGE_PIN 35
//R62 and R11 is not soldered on board
//#define IP5306_IRQ 39

struct Config {
  uint8_t callTo = 0b00000000;  //call to first, 0b00000011 call to first and second, etc
  bool armStatus = false;
  bool doorSensorEnabled = true;
  bool motionSensorEnabled = true;
  bool gridLostCall = false;
  bool gridAppearCall = false;
} config;
LynkFile configFile(&LittleFS, "/config.cfg", 2, &config, sizeof(config));

bool connectingToWifi = false;
bool alarmStatus = false;
bool doorOpened = false;
bool motionDetected = false;
uint8_t doorOpenedCount = 0;
uint8_t motionDetectedCount = 0;
uint32_t setupStartMillis;
uint32_t lastAlarmMillis;
uint32_t lastDoorOpenedDetected;
uint32_t lastMotionDetected;
uint32_t lastSensorReadMillis;
int targetCallIndex = 8;
uint8_t alarmInitiatedBy = 0;
bool needToRecheckBalance = true;
bool needToParseBalance = false;
int balance = -1;
int dtmfMenu = 0;
bool gridPresent = false;

#define ALARM_DURATION_MS 1000 * 60 * 5  //5min
#define DOOR_SENSOR_DELAY 5000           //5s display as opened after close for minimize jigle
#define MOTION_SENSOR_DELAY 5000         //4s delay on fire detection for minimize false alarm
#define DOOR_OPENED_COUNT_FIRE 3
#define MOTION_DETECTED_COUNT_FIRE 5
#define SENSOR_READ_INTERVAL 100       //100ms
#define WARMUP_PERIOD 60000            //60s
#define CONNECT_TO_WIFI_TIMEOUT 20000  //20s

uint8_t blinkRemain = 0;
bool blinkState = false;
unsigned long lastBlinkMillis;
#define BLINK_DURATION 100
#define BLINK_PAUSE_DURATION 500

void setup() {
  setupStartMillis = millis();
  //Setup I2C bus(IP5306,)
  Wire.begin(I2C_SDA, I2C_SCL);  //begin ASAP, cause IP5306 can go to non i2c mode
  presetupModem();
  delay(1000);

  // Set console baud rate
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("setup start...");

  Serial.println("==========DEFAULT VALUES==========");
  printConfig();  // print default values
  LittleFS.begin(true);
  FileStatus fileStatus = configFile.init();
  Serial.println("==========VALUES FROM FLASH==========");
  printConfig();  // print values from flash

  analogReadResolution(10);
  pinMode(MOTION_SENSOR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);  //indicate esp started
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }

  //enable boost output(if not successfull - restart)
  if (!IP5306_SetBoostOutputEnabled(true)) {
    Serial.println("IP5306 not respond, restart in 10s");
    delay(10000);
    ESP.restart();
  }

  if (IP5306_GetPowerSource()) {
    gridPresent = true;
    connectingToWifi = true;
    setupTelegram();
    if (config.gridAppearCall) {
      targetCallIndex = 0;
    }
  }

  printIP5306Stats();
  delay(1000);

  if (!connectingToWifi) {
    Serial.println("Initializing modem...");
    setupModem();
  }

  lastAlarmMillis = setupStartMillis;
  lastDoorOpenedDetected = setupStartMillis;
  lastMotionDetected = setupStartMillis;
  lastSensorReadMillis = setupStartMillis;
  Serial.println("setup end");
}

void loop() {
  //Serial.println("loop start...");
  bool oldMotionDetected = motionDetected;
  updateAlarmStatus();


  if (connectingToWifi) {
    if (isWifiConnected()) {
      tickTelegram();
      if (!oldMotionDetected && motionDetected) {
        sendToChat("Motion detected");
      } else if (oldMotionDetected && !motionDetected) {
        sendToChat("Motion stopped");
      }
    } else if (millis() - setupStartMillis > CONNECT_TO_WIFI_TIMEOUT) {
      connectingToWifi = false;
      WiFi.mode(WIFI_OFF);
      WiFi.disconnect(true);
      Serial.println("Initializing modem...");
      setupModem();
    }
  } else {
    bool gridPresentNew = IP5306_GetPowerSource();
    if (!gridPresentNew && gridPresent && config.gridLostCall) targetCallIndex = 0;
    if (gridPresentNew && !gridPresent && config.gridAppearCall) targetCallIndex = 0;
    if (gridPresentNew != gridPresent) gridPresent = gridPresentNew;
    //clear all previus responses and unsolicited response codes
    readFromModemUntilOK(100L);
    if (checkModemRegistrationStatus()) {
      tickModem();
    }
  }
  delay(10);

  //int rawAnalogVoltage = analogReadMilliVolts(BATT_VOLTAGE_PIN) * 2;
  //Serial.print("Voltage value: ");
  //Serial.println(rawAnalogVoltage);

  //if(blinkRemain < 1 && millis() - lastBlinkMillis > BLINK_PAUSE_DURATION) {
  //  blinkRemain = IP5306_LEDS2FOUR(IP5306_GetLevelLeds());
  //  lastBlinkMillis = millis();
  //}
  //blinkAsync();

  //Serial.println("loop end");
}

void readSensors() {
  if (millis() - lastSensorReadMillis < SENSOR_READ_INTERVAL)
    return;
  uint32_t now = millis();
  int rawAnalogDoorSensor = analogRead(DOOR_SENSOR_PIN);
  //Serial.print("Door sensor value: ");
  //Serial.print(rawAnalogDoorSensor);
  if (rawAnalogDoorSensor < 512) {
    if (!doorOpened) {
      doorOpenedCount++;
    }
    lastDoorOpenedDetected = now;
  } else {
    doorOpenedCount = 0;
    if (doorOpened && (now - lastDoorOpenedDetected) > DOOR_SENSOR_DELAY) {
      Serial.println("Door closed");
      doorOpened = false;
    }
  }
  if (!doorOpened && doorOpenedCount >= DOOR_OPENED_COUNT_FIRE) {
    Serial.println("Door opened");
    doorOpened = true;
  }

  bool motionDetectedNew = digitalRead(MOTION_SENSOR_PIN);
  if (!motionDetected && motionDetectedNew) {
    lastMotionDetected = now;
    motionDetectedCount++;
    if (motionDetectedCount >= MOTION_DETECTED_COUNT_FIRE) {
      motionDetected = true;
      Serial.println("Motion detected");
    }
  } else if (motionDetected && !motionDetectedNew && (now - lastMotionDetected) > MOTION_SENSOR_DELAY) {
    motionDetected = false;
    Serial.println("Motion stopped");
  } else if (motionDetectedNew) {
    lastMotionDetected = now;
  }
}

void updateAlarmStatus() {
  readSensors();
  if (millis() - setupStartMillis < WARMUP_PERIOD) {
    //ignore sensors states during warmup period
    return;
  }
  if (config.armStatus) {
    if ((config.doorSensorEnabled && doorOpened) || (config.motionSensorEnabled && motionDetected)) {
      if (!alarmStatus) {
        //start alarm
        targetCallIndex = 0;
        alarmInitiatedBy = 0;
        alarmStatus = true;
      }
      if (config.doorSensorEnabled && doorOpened) bitSet(alarmInitiatedBy, 0);
      if (config.motionSensorEnabled && motionDetected) bitSet(alarmInitiatedBy, 1);
      lastAlarmMillis = millis();
    } else if (alarmStatus) {
      if (millis() - lastAlarmMillis > ALARM_DURATION_MS) {
        //alarm timeout, sensors ok, stop alarm
        alarmStatus = false;
      }
    }
  } else if (alarmStatus) {
    //disarm, stop alarm
    alarmStatus = false;
  }
}

void tickModem() {
  Call call = getCall();
  if (call.status == ACTIVE) {
    if (call.direction == IN) {
      Serial.print("Active in call from: ");
      Serial.println(call.number);
      processDTMF(call.number);
    } else {
      Serial.print("Hangup Active out call to: ");
      Serial.println(call.number);
      hangup();
      if (!initiateNextCall()) {
        checkBalance();
      }
    }
  } else if (call.status == INCOMING) {
    Serial.print("Incoming: ");
    Serial.print(call.number);
    Serial.print("indexOf: ");
    Serial.println(ALLOW_INCOME_NUMBERS.indexOf(call.number));
    if (call.number.length() > 0 && ALLOW_INCOME_NUMBERS.indexOf(call.number) >= 0) {
      Serial.println("Incoming allowed, waiting...");
      String number = call.number;
      delay(5000);
      call = getCall();
      if (call.status != 0 && call.number.length() > 0 && number == call.number) {
        Serial.print("still incoming from same number, answering... ");
        sendAT("+DDET=1");
        readFromModemUntilOK();
        answer();
      }
    } else {
      delay(5000);
      hangup();
    }
  } else if (call.status == NO_CALL) {
    if (!initiateNextCall()) {
      checkBalance();
    }
  }

  parseBalance();
}

bool initiateNextCall() {
  if (targetCallIndex >= 8) return false;
  byte bit = bitRead(config.callTo, targetCallIndex);
  if (!bit) {
    targetCallIndex++;
    return true;
  }
  initCall(ALLOW_INCOME_NUMBERS.substring(targetCallIndex * NUMBER_LENGTH, (targetCallIndex + 1) * NUMBER_LENGTH));
  targetCallIndex++;
  needToRecheckBalance = true;
  return true;
}

void checkBalance() {
  if (!needToRecheckBalance) return;
  needToRecheckBalance = false;
  sendUssd("*111#");
  needToParseBalance = true;
}

void parseBalance() {
  if (!needToParseBalance) return;
  String response = getUssdResponse();
  if (response == "") return;
  needToParseBalance = false;
  Serial.print("Raw balance: ");
  Serial.println(response);
  int start = response.indexOf("UAH");  //"Vash osnovnij balans: UAH1.00. Spasib?, shcho Vi z Lycamobile"
  if (start < 0 || start + 4 > response.length()) return;
  start += 3;
  int end = response.indexOf('.', start + 1);
  if (end < 0) return;
  balance = response.substring(start, end).toInt();
  Serial.print("Balance: ");
  Serial.println(balance);
}

void callToThisNumber(String phoneNumber, bool enable) {
  int index = ALLOW_INCOME_NUMBERS.indexOf(phoneNumber);
  index = index / NUMBER_LENGTH;
  if (index < 0 || index >= 8) return;  //Unexpexted behaviour
  bool oldState = bitRead(config.callTo, index);
  if (oldState != enable) {
    if (enable) {
      bitSet(config.callTo, index);
    } else {
      bitClear(config.callTo, index);
    }
    configFile.commit();
  }
}

void processDTMF(String phoneNumber) {
  String dtmfCodes = getDTMFCodes();
  for (int i = 0; i < dtmfCodes.length(); i++) {
    uint8_t code = dtmfCodes[i] - '0';
    if (dtmfMenu == 0) {
      if (code == 1) {
        if (!config.armStatus && (!config.doorSensorEnabled || !doorOpened) && (!config.motionSensorEnabled || !motionDetected)) {
          config.armStatus = true;
          configFile.commit();
          //startWarmupMillis = millis();
          playSound("arm.amr");
        } else {
          playSound("error.amr");
        }
      } else if (code == 2) {
        config.armStatus = false;
        configFile.commit();
        targetCallIndex = 8;
        playSound("disarm.amr");
      } else if (code == 3) {
        if (!config.armStatus) {
          playSound("disarm.amr");  //disarmed
        } else if (!alarmStatus) {
          playSound("arm.amr");  //armed, no alarm
        } else {
          playSound("alarm.amr");  //alarm
          if (alarmInitiatedBy == 1 || alarmInitiatedBy == 3) playSound("dooropened.amr");
          if (alarmInitiatedBy == 2 || alarmInitiatedBy == 3) playSound("motiondetected.amr");
        }
        playSound("feedfrom.amr");
        if (IP5306_GetPowerSource()) {
          playSound("grid.amr");
        } else {
          playSound("batstat.amr");
          uint8_t leds = IP5306_LEDS2FOUR(IP5306_GetLevelLeds());
          playSound(leds == 4 ? "p100.amr"
                              : (leds == 3 ? "p75.amr"
                                           : (leds == 2 ? "p50.amr"
                                                        : (leds == 1 ? "p25.amr"
                                                                     : "discharged.amr"))));
        }
        playSound("balance.amr");
        if (balance > 10) {
          playSound("ok.amr");
        } else if (balance < 0) {
          playSound("error.amr");
        } else {
          playSound("low.amr");
        }
      } else if (code == 4) {
        if (doorOpened) playSound("dooropened.amr");
        if (motionDetected) playSound("motiondetected.amr");
        if (!doorOpened && !motionDetected) playSound("ok.amr");
      } else if (code == 5) {
        sendAT("+CEXTERNTONE=0");
        sendAT("+CMIC=1,0");
        sendAT("+CHFA=1");
        playSound("mic.amr");
        playSound("on.amr");
      } else if (code == 6) {
        //stopListen
        sendAT("+CEXTERNTONE=1");
        playSound("mic.amr");
        playSound("off.amr");
      } else if (code == 9) {
        dtmfMenu = 9;
        playSound("settings.amr");
      } else if (code == 0) {
        dtmfMenu = 0;
        playSound("ok.amr");
      }  //else ignore
    } else if (dtmfMenu == 9) {
      if (code == 1) {
        dtmfMenu = 91;
        playSound("doorsensor.amr");
      } else if (code == 2) {
        dtmfMenu = 92;
        playSound("motionsensor.amr");
      } else if (code == 3) {
        dtmfMenu = 93;
        playSound("call.amr");
      } else if (code == 4) {
        dtmfMenu = 94;
        playSound("gridlost.amr");
      } else if (code == 5) {
        dtmfMenu = 95;
        playSound("gridappear.amr");
      } else if (code == 0) {
        dtmfMenu = 0;
        playSound("ok.amr");
      }  //else ignore
    } else if (dtmfMenu == 91) {
      if (code == 1) {
        if (!config.doorSensorEnabled) {
          config.doorSensorEnabled = true;
          configFile.commit();
        }
        playSound("on.amr");
      } else if (code == 2) {
        if (config.doorSensorEnabled) {
          config.doorSensorEnabled = false;
          configFile.commit();
        }
        playSound("off.amr");
      } else if (code == 0) {
        playSound("ok.amr");
      }  //else ignore
      dtmfMenu = 0;
    } else if (dtmfMenu == 92) {
      if (code == 1) {
        if (!config.motionSensorEnabled) {
          config.motionSensorEnabled = true;
          configFile.commit();
        }
        playSound("on.amr");
      } else if (code == 2) {
        if (config.motionSensorEnabled) {
          config.motionSensorEnabled = false;
          configFile.commit();
        }
        playSound("off.amr");
      } else if (code == 0) {
        playSound("ok.amr");
      }  //else ignore
      dtmfMenu = 0;
    } else if (dtmfMenu == 93) {
      if (code == 1) {
        callToThisNumber(phoneNumber, true);
        playSound("on.amr");
      } else if (code == 2) {
        callToThisNumber(phoneNumber, false);
        playSound("off.amr");
      } else if (code == 0) {
        playSound("ok.amr");
      }  //else ignore
      dtmfMenu = 0;
    } else if (dtmfMenu == 94) {
      if (code == 1) {
        config.gridLostCall = true;
        configFile.commit();
        playSound("on.amr");
      } else if (code == 2) {
        config.gridLostCall = false;
        configFile.commit();
        playSound("off.amr");
      } else if (code == 0) {
        playSound("ok.amr");
      }  //else ignore
      dtmfMenu = 0;
    } else if (dtmfMenu == 95) {
      if (code == 1) {
        config.gridAppearCall = true;
        configFile.commit();
        playSound("on.amr");
      } else if (code == 2) {
        config.gridAppearCall = false;
        configFile.commit();
        playSound("off.amr");
      } else if (code == 0) {
        playSound("ok.amr");
      }  //else ignore
      dtmfMenu = 0;
    }
  }
}

/*
AT+FSLS=C:\User\

alarm.amr
arm.amr
disarm.amr
dooropened.amr
doorsensor.amr
mic.amr
motiondetected.amr
motionsensor.amr
off.amr
on.amr
settings.amr
error.amr
ok.amr
feedfrom.amr
grid.amr
batstat.amr
p100.amr
p75.amr
p50.amr
p25.amr
discharged.amr
call.amr
balance.amr
low.amr
gridlost.amr
gridappear.amr
*/

//blink by builtin blue led
void blinkAsync() {
  if (blinkRemain < 1) return;
  if (blinkState) {
    if (millis() - lastBlinkMillis > BLINK_DURATION) {
      blinkState = 0;
      digitalWrite(LED_PIN, blinkState);
      lastBlinkMillis = millis();
      blinkRemain--;
    }
  } else {
    if (millis() - lastBlinkMillis > BLINK_PAUSE_DURATION) {
      blinkState = 1;
      digitalWrite(LED_PIN, blinkState);
      lastBlinkMillis = millis();
    }
  }
}

void printConfig() {
  Serial.print("Version: ");
  Serial.print(configFile.getVersion());
  Serial.print(" { callTo = ");
  Serial.print(config.callTo, BIN);
  Serial.print(" , armStatus = ");
  Serial.print(config.armStatus);
  Serial.println(" }");
}