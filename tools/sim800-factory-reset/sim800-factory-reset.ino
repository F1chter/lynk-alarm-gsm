

#include <TinyGsmClient.h>

// Set serial for debug console (to the Serial Monitor, speed 115200)
#define SerialMon Serial

#define SerialAT Serial1

#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);

void setup() {
  // Set console baud rate
  SerialMon.begin(115200);
  delay(10);

  // Set GSM module baud rate
  SerialAT.begin(9600);
  delay(6000);

  if (!modem.init()) {
    SerialMon.println(
        F("***********************************************************"));
    SerialMon.println(F(" Cannot initialize modem!"));
    SerialMon.println(
        F("   Use File -> Examples -> TinyGSM -> tools -> AT_Debug"));
    SerialMon.println(F("   to find correct configuration"));
    SerialMon.println(
        F("***********************************************************"));
    return;
  }

  bool ret = modem.factoryDefault();

  SerialMon.println(
      F("***********************************************************"));
  SerialMon.print(F(" Return settings to Factory Defaults: "));
  SerialMon.println((ret) ? "OK" : "FAIL");
  SerialMon.println(
      F("***********************************************************"));
}

void loop() {}