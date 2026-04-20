#define TINY_GSM_MODEM_SIM800

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG Serial

#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 57600

#define TINY_GSM_YIELD() { delay(2); }

#define CALL_TARGET "+38051234567"

#include <TinyGsmClient.h>
//#include "IP5306_I2C.h"
#include <Wire.h>
#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define I2C_SDA   21
#define I2C_SCL   22

//IP5306 ip5306(I2C_SDA,I2C_SCL);
TinyGsm modem(Serial1);

void setup() {
  // Set console baud rate
  Serial.begin(115200);
  delay(10);

  Serial.write("Start setup...");

  //set battery voltage
  //ip5306.set_battery_voltage(BATT_VOLTAGE_0);   //4.2V

  //enable boost mode
  //ip5306.boost_mode(ENABLE);
   // Ensure power boost keep on (IP5306) so module stays powered from battery
  Wire.begin(I2C_SDA, I2C_SCL);
  // write to IP5306 register 0x00 to set keep-on bit
  Wire.beginTransmission(0x75);
  Wire.write(0x00);
  Wire.write(0x37);
  Wire.endTransmission();
  Serial.write("Enable boost mode");

  // setup modem power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  Serial.write("Auto baud...");
  TinyGsmAutoBaud(Serial1, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);

  //modem.setBaud(57600);
  
  DBG("Initializing modem...");
  //if (!modem.restart()) {
  if (!modem.init()) {
    DBG("Failed to init modem, delaying 10s and retrying");
    // restart autobaud in case GSM just rebooted
    // TinyGsmAutoBaud(SerialAT, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
    delay(10000L);
    modem.restart();
    DBG("Restarting...");
    delay(10000L);
  }
}

void loop() {
  String modemInfo = modem.getModemInfo();
  DBG("Modem Info:", modemInfo);

  String name = modem.getModemName();
  DBG("Modem Name:", name);

  String manufacturer = modem.getModemManufacturer();
  DBG("Modem Manufacturer:", manufacturer);

  String hw_ver = modem.getModemModel();
  DBG("Modem Hardware Version:", hw_ver);

  String fv_ver = modem.getModemRevision();
  DBG("Modem Firware Version:", fv_ver);

  String mod_sn = modem.getModemSerialNumber();
  DBG("Modem Serial Number (may be SIM CCID):", mod_sn);

  //DBG("Waiting for network...");
  //if (!modem.waitForNetwork(600000L, true)) {
  //  DBG(".");
  //  delay(10000); 
  //}

  int status = modem.getRegistrationStatus();
  DBG("status:", status);
  while(status == 2) {
    DBG("Searching...");
    delay(10000);
    status = modem.getRegistrationStatus();
  }
  DBG("status:", status);

  if(status == -1) {
    modem.restart();
    delay(30000);
    waitForOK();
  }

  String ccid = modem.getSimCCID();
  DBG("CCID:", ccid);

  String imei = modem.getIMEI();
  DBG("IMEI:", imei);

  String imsi = modem.getIMSI();
  DBG("IMSI:", imsi);

  String cop = modem.getOperator();
  DBG("Operator:", cop);

  int csq = modem.getSignalQuality();
  DBG("Signal quality:", csq);

  DBG("Calling:", CALL_TARGET);

    // This is NOT supported on M590
  bool res = modem.callNumber(CALL_TARGET);
  DBG("Call:", res ? "OK" : "fail");

  if (res) {
    delay(10000L);
    // Play DTMF A, duration 1000ms
    modem.dtmfSend('A', 1000);

    // Play DTMF 0..4, default duration (100ms)
    for (char tone = '0'; tone <= '4'; tone++) { modem.dtmfSend(tone); }

    delay(5000);

    res = modem.callHangup();
    DBG("Hang up:", res ? "OK" : "fail");
  }
  while(true) {
    makeCall();
  }
}

void waitForOK(){
  int status = modem.getRegistrationStatus();
  DBG("status:", status);
  while(status != 1 && status != 5) {
    DBG("Searching...");
    delay(10000);
    status = modem.getRegistrationStatus();
    DBG("status:", status);
  }
}

void makeCall() {
    DBG("Calling:", CALL_TARGET);

    // This is NOT supported on M590
  bool res = modem.callNumber(CALL_TARGET);
  //FIXME res before answer
  DBG("Call:", res ? "OK" : "fail");

  if (res) {
    delay(10000L);
    // Play DTMF A, duration 1000ms
    modem.dtmfSend('A', 1000);

    // Play DTMF 0..4, default duration (100ms)
    for (char tone = '0'; tone <= '4'; tone++) { modem.dtmfSend(tone); }

    delay(5000);

    res = modem.callHangup();
    DBG("Hang up:", res ? "OK" : "fail");
  }
}