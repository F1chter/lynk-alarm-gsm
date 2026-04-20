#define DEBUG_ENABLED true

#define CALL_TARGET "+380501234567"

//#include "IP5306_I2C.h"
#include <Wire.h>

#define I2C_SDA 21
#define I2C_SCL 22
//IP5306 ip5306(I2C_SDA,I2C_SCL);

unsigned long lastCallMillis;

void setup() {
#if DEBUG_ENABLED
  // Set console baud rate
  Serial.begin(115200);
  delay(1000);
  Serial.println("setup start...");
#endif
  
  //Setup I2C bus(IP5306,)
  Wire.begin(I2C_SDA, I2C_SCL);
  
  //enable boost output
  ip5306_set_bits(0x00, 1, 1, 1);
  //disable eco mode(cause load is too small and boost go to sleep)
  ip5306_set_bits(0x00, 2, 1, 0);
  //set power source from bat
  ip5306_set_bits(0x23, 5, 1, 0);
#if DEBUG_ENABLED
  Serial.println("Boost output enabled and eco mode disabled");
  printIP5306Settings();
  printIP5306Stats();
#endif

#if DEBUG_ENABLED
 Serial.println("Initializing modem...");
#endif
  setupModem();
  lastCallMillis = millis();

#if DEBUG_ENABLED
 Serial.println("setup end");
#endif
}

void loop() {
#if DEBUG_ENABLED
 Serial.println("loop start...");
#endif
  if(checkModemRegistrationStatus()) {
    doModemTest();
  }
  //doModemTest();
  delay(5000);
  printIP5306Stats();
#if DEBUG_ENABLED
 Serial.println("loop end");
#endif
}

void doModemTest() {
  
  //### Unhandled: Call Ready
  //### Unhandled: SMS Ready

  //checkIncomingCall
  String phoneNumber = checkIncomingCall();
  if (phoneNumber == "+380501234567") {
#if DEBUG_ENABLED
    Serial.println("Incoming +380501234567 and wait");
#endif   
    delay(1000);
    phoneNumber = checkIncomingCall();
    if (phoneNumber != "") {
      hangup();
    }
  } else if (phoneNumber != "") {
    hangup();
  }
  if (!getOutgoingCallStatus() && millis() - lastCallMillis > 60000) {
    initCall("+380501234567");
  }
  updateOutgoingCallStatus();
  if (getOutgoingCallStatus()) {
    lastCallMillis = millis();
  }
}
