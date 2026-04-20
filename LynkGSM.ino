//#define TINY_GSM_MODEM_SIM800
//#define GSM_AUTOBAUD_MIN 9600
//#define GSM_AUTOBAUD_MAX 57600
//#define TINY_GSM_DEBUG Serial
//#include <TinyGsmClient.h>
//TinyGsm modem(Serial1);



#define LYNK_GSM_VERSION "0.1"

#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define MODEM_DTR_PIN 32

#define LYNK_GSM_YIELD() \
  { delay(2); }


unsigned long lastNetworkAvailableMillis;
uint8_t outgoingCallStatus = 0;  //0 - no outgoing call, 1 - initiated,  2- active, 3-held, 4-dialing, 5-alerting, 6-incoming, 7-waiting, 8-disconnect
#define NETWORK_UNAVAILABLE_TIMEOUT 5000
#define NETWORK_SEARCHING_TIMEOUT 300000




uint8_t waitModemResponse(uint32_t timeout_ms, String& data,
                          const char* r1 = nullptr, const char* r2 = nullptr,
                          const char* r3 = nullptr, const char* r4 = nullptr) {
  data.reserve(2);
  uint8_t index = 0;
  uint32_t startMillis = millis();
  do {
    LYNK_GSM_YIELD();
    while (Serial1.available() > 0) {
      LYNK_GSM_YIELD();
      int8_t a = Serial1.read();
      if (a <= 0) continue;  // Skip 0x00 bytes, just in case
      data += (char)a;
      if (r1 && data.endsWith(r1)) {
        index = 1;
        break;
      } else if (r2 && data.endsWith(r2)) {
        index = 2;
        break;
      } else if (r3 && data.endsWith(r3)) {
        index = 3;
        break;
      } else if (r4 && data.endsWith(r4)) {
        index = 4;
        break;
      }
    }
  } while (millis() - startMillis < timeout_ms && !index);
  if (!index) {
    data.trim();

#ifdef DEBUG_ENABLED
    if (data.length()) {
      Serial.print("### Unhandled:");
      Serial.println(data);
    }
#endif

    data = "";
  }
  return index;
}

uint8_t waitModemResponse(uint32_t timeout_ms, const char* r1 = nullptr, const char* r2 = nullptr,
                          const char* r3 = nullptr, const char* r4 = nullptr) {
  String data;
  return waitModemResponse(1000L, data, r1, r2, r3, r4);
}

void sendAT(String command) {
  Serial1.write("AT");
  Serial1.print(command);
  Serial1.write("\r\n");
  Serial1.flush();
  LYNK_GSM_YIELD();
}

bool testAT(uint32_t timeout_ms = 10000L) {
  for (uint32_t start = millis(); millis() - start < timeout_ms;) {
    sendAT("");
    if (waitModemResponse(200L, "OK") == 1) { return true; }
    delay(100);
  }
  return false;
}

bool initModem(const char* pinCode = nullptr) {

#if DEBUG_ENABLED
  Serial.print("### LynkGSM.ino v");
  Serial.print(LYNK_GSM_VERSION);
  Serial.println(" initModem");
#endif

  if (!testAT()) { return false; }

#if DEBUG_ENABLED
  Serial.println("test OK");
#endif
  // sendAT(GF("&FZ"));  // Factory + Reset
  // waitResponse();

  sendAT("E0");  // Echo Off
  if (waitModemResponse(1000L, "OK") != 1) { return false; }

#if DEBUG_ENABLED
  Serial.println("echo off");
#endif

#ifdef DEBUG_ENABLED
  sendAT("+CMEE=2");  // turn on verbose error codes
#else
  sendAT("+CMEE=0");  // turn off error codes
#endif
  waitModemResponse(1000L);

#ifdef DEBUG_ENABLED
  String manufacturer = "unknown";
  sendAT("+CGMI");  // 3GPP TS 27.007 standard
  String result;
  if (waitModemResponse(1000L, result, "OK") != 1) { manufacturer = result; }

  String model = "SIM800";
  sendAT("+CGMM");  // 3GPP TS 27.007 standard
  String result2;
  if (waitModemResponse(1000L, result2, "OK") != 1) { model = result2; }

  //TODO
  //modem.getModemInfo();
  //modem.getModemName();
  //modem.getModemRevision();
  //modem.getModemSerialNumber();
  //modem.getSimCCID();
  //modem.getIMEI();
  //modem.getIMSI();


  Serial.print(manufacturer);
  Serial.print(" ");
  Serial.print(model);
  Serial.println();
#endif

  //TODO Enable Local Time Stamp for getting network time
  //sendAT("+CLTS=1");
  //if (waitModemResponse(10000L, "OK") != 1) { return false; }

  //TODO Enable battery checks
  //sendAT("+CBATCHK=1");
  //waitModemResponse(10000L);

  //TODO sim unlock functionality
  /*
    SimStatus ret = getSimStatus();
    // if the sim isn't ready and a pin has been provided, try to unlock the sim
    if (ret != SIM_READY && pinCode != nullptr && strlen(pinCode) > 0) {
      simUnlock(pinCode);
      return (getSimStatus() == SIM_READY);
    } else {
      // if the sim is ready, or it's locked but no pin has been provided,
      // return true
      return (ret == SIM_READY || ret == SIM_LOCKED);
    }
    */
    return true;
}


void setupModem() {
  // setup modem power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  Serial1.begin(57600);

  //TinyGsmAutoBaud(Serial1, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
  if (!initModem()) {
#ifdef DEBUG_ENABLED
    Serial.println("Failed to init modem, wait 10s and restarting");
#endif
    // restart autobaud in case GSM just rebooted
    // TinyGsmAutoBaud(SerialAT, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
    delay(1000L);
    //TODO
    //modem.restart();
    //DBG("Restarting...");
    //delay(10000L);
  }

  lastNetworkAvailableMillis = millis();
}

bool checkModemRegistrationStatus() {
  sendAT("+CREG?");
  String data;
  uint8_t result = waitModemResponse(1000L, data, "OK");
#ifdef DEBUG_ENABLED
  Serial.println("Modem registration status response:");
  Serial.println(data);
#endif
  if (result != 1) return false;
  int start = data.indexOf("+CREG: ");
  if (start < 0 || start + 10 > data.length()) {
    return false;
  }
  int status = data[start + 9] - '0';
#ifdef DEBUG_ENABLED
  Serial.print("Modem registration status: ");
  Serial.println(status);
#endif
  if (status == 2) {
    if (millis() - lastNetworkAvailableMillis > NETWORK_SEARCHING_TIMEOUT) {
      //TODO
      //modem.restart();
      lastNetworkAvailableMillis = millis();
    }
    return false;
  } else if (status != 1 && status != 5) {
    if (millis() - lastNetworkAvailableMillis > NETWORK_UNAVAILABLE_TIMEOUT) {
      //TODO
      //modem.restart();
      lastNetworkAvailableMillis = millis();
    }
    return false;
  }
  lastNetworkAvailableMillis = millis();

  //String cop = modem.getOperator();
  //DBG("Operator:", cop);

  //int csq = modem.getSignalQuality(); //### Unhandled: +CIEV: 10,"25501","Vodafone UA","Vodafone UA", 0, 0
  //DBG("Signal quality:", csq);

  return true;
}

uint8_t initCall(String phoneNumber) {
#ifdef DEBUG_ENABLED
  Serial.print("Calling: ");
  Serial.println(phoneNumber);
#endif

  String command = "D" + phoneNumber + ";";
  sendAT(command);
  outgoingCallStatus = 1;
  return waitModemResponse(60000L, "OK", "BUSY", "NO ANSWER", "NO CARRIER");
}

uint8_t hangup() {
#ifdef DEBUG_ENABLED
  Serial.println("Hangup");
#endif
  sendAT("H");  //+CHUP
  return waitModemResponse(1000L, "OK");
}

String checkIncomingCall() {
  uint8_t result = waitModemResponse(1000L, "RING");
  if (result != 1) return "";
  sendAT("+CLCC");
  String data;
  result = waitModemResponse(1000L, data, "OK");
  if (result != 1) return "";
  //parse CLCC response +CLCC: <id1>,<dir>,<stat>,<mode>,<mpty>[,<number>,<type>[,<alpha>[,<priority>]]]
  int start = data.indexOf("+CLCC: ");
  if (start < 0 || start + 31 > data.length()) return "";
    //check dir=Mobile terminated(incoming), status=Incoming, mode=Voice, multiparty=no
#ifdef DEBUG_ENABLED
  Serial.print("dir= ");
  Serial.println(data[start + 9]);
#endif
  if (data[start + 9] != '1') return "";
#ifdef DEBUG_ENABLED
  Serial.print("status= ");
  Serial.println(data[start + 11]);
#endif
  if (data[start + 11] != '4') return "";
#ifdef DEBUG_ENABLED
  Serial.print("mode= ");
  Serial.println(data[start + 13]);
#endif
  if (data[start + 13] != '0') return "";
#ifdef DEBUG_ENABLED
  Serial.print("mpty= ");
  Serial.println(data[start + 15]);
#endif
  if (data[start + 15] != '0') return "";
#ifdef DEBUG_ENABLED
  Serial.print("number= ");
  Serial.println(data.substring(start + 18, start + 31));
#endif
  return data.substring(start + 18, start + 31);
}

uint8_t updateOutgoingCallStatus() {
  sendAT("+CLCC");
  String data;
  uint8_t result = waitModemResponse(1000L, data, "OK");
  if (result != 1) return outgoingCallStatus;  //outgoingCallStatus = 0;
  //parse CLCC response +CLCC: <id1>,<dir>,<stat>,<mode>,<mpty>[,<number>,<type>[,<alpha>[,<priority>]]]
  int start = data.indexOf("+CLCC: ");
  if (start < 0 || start + 31 > data.length()) {
    outgoingCallStatus = 0;
    return outgoingCallStatus;
  }
  //check dir=Mobile originated(outcoming), mode=Voice, multiparty=no
  if (data[start + 9] != '0') return outgoingCallStatus;
  if (data[start + 13] != '0') return outgoingCallStatus;
  if (data[start + 15] != '0') return outgoingCallStatus;
  outgoingCallStatus = data[start + 11] - '0' + 2;  //0-no call,1-initiated, 2-active, 3-held, 4-dialing, 5-alerting, 6-incoming, 7-waiting, 8-disconnect
#ifdef DEBUG_ENABLED
  Serial.print("outgoingCallStatus= ");
  Serial.println(outgoingCallStatus);
#endif
  return outgoingCallStatus;
}

uint8_t getOutgoingCallStatus() {
  return outgoingCallStatus;
}
