#pragma once
#include "Arduino.h"

#define DEBUG_ENABLED true

#define LYNK_GSM_VERSION "0.1"

#define modem Serial1
#define MODEM_RST 5
#define MODEM_PWR 4
#define MODEM_PWR_CONVERTER 23
//#define MODEM_DTR 32
//#define MODEM_RI 33 not connected in t-call v1.3

#define LYNK_GSM_YIELD() \
  { delay(2); }

#define NETWORK_UNAVAILABLE_TIMEOUT 5000
#define NETWORK_SEARCHING_TIMEOUT 300000

unsigned long lastNetworkAvailableMillis;
String dtmfCodes = "";
String ussdResponse = "";


enum CallDirection : uint8_t {
  OUT,
  IN
};

enum CallStatus : uint8_t {
  NO_CALL,
  ACTIVE,
  HELD,
  DIALING,
  ALERTING,
  INCOMING,
  WAITING,
  DISCONNECT
};

enum CallMode : uint8_t {
  VOICE,
  DATA,
  FAX
};

enum CallMultiparty : uint8_t {
  NO,
  YES
};

struct Call {
  CallDirection direction;
  CallStatus status;
  CallMode mode;
  CallMultiparty multiparty;
  String number;
};

void checkUnsolicitedResponseCodes(String buffer) {
  //CallReady
  //SMSReady
  //RING
  //+DTMF:1+DTMF:2+DTMF:3
  int index = -7;
  do {
    index = buffer.indexOf("+DTMF:", index + 7);
    if (index < 0) break;
    if (buffer.length() < index + 7) break;
#ifdef DEBUG_ENABLED
    Serial.print("DTMF detected: ");
    Serial.println(buffer[index + 6]);
#endif
    dtmfCodes += buffer[index + 6];
    if (buffer.length() < index + 8) break;  //end of buffer
  } while (true);
  index = 0;
  do {
    index = buffer.indexOf("+CUSD:0,\"", index);
    if (index < 0) break;
    if (buffer.length() < index + 7) break;
    int end = buffer.indexOf('"', index + 9);
    if (end < 0 || index + 9 == end) break;
#ifdef DEBUG_ENABLED
    Serial.print("USSD response detected: ");
    Serial.println(buffer.substring(index + 9, end));
#endif
    ussdResponse = buffer.substring(index + 9, end);
    if (end + 2 > buffer.length()) break; //end of buffer
    index = end + 1;
  } while (true);
}

uint8_t readFromModem(uint32_t timeout_ms, String& buffer,
                      const char* r1 = nullptr, const char* r2 = nullptr,
                      const char* r3 = nullptr, const char* r4 = nullptr) {
  buffer.reserve(2);
  uint8_t index = 0;
  uint32_t startMillis = millis();
  Serial.print("timeout = "); 
  Serial.println(timeout_ms);
  do {
    LYNK_GSM_YIELD();
    while (modem.available()) {
      LYNK_GSM_YIELD();
      int8_t c = modem.read();
      if (!((c >= 'a' && c <= 'z')
            || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9')
            || c == '+' || c == '=' || c == '"' || c == ',' || c == ':' || c == '.')) continue;  //skip non a-zA-Z0-9 +=",:.

      buffer += (char)c;
      if (r1 && buffer.endsWith(r1)) {
        index = 1;
        break;
      } else if (r2 && buffer.endsWith(r2)) {
        index = 2;
        break;
      } else if (r3 && buffer.endsWith(r3)) {
        index = 3;
        break;
      } else if (r4 && buffer.endsWith(r4)) {
        index = 4;
        break;
      }
    }
  } while (millis() - startMillis < timeout_ms && !index);
  Serial.print("passed = "); 
  Serial.println(millis() - startMillis);
#ifdef DEBUG_ENABLED
  if (buffer.length()) {
    Serial.print("GSM Response:");
    Serial.println(buffer);
  } else Serial.println("GSM no response");
#endif
  checkUnsolicitedResponseCodes(buffer);
  return index;
}

uint8_t readFromModemUntilOK(uint32_t timeout_ms = 1000L) {
  String buffer = "";
  return readFromModem(timeout_ms, buffer, "OK");
}

void sendAT(String command) {
#ifdef DEBUG_ENABLED
  Serial.print("Sent to modem: AT");
  Serial.println(command);
#endif
  Serial1.write("AT");
  Serial1.print(command);
  Serial1.write("\r\n");
  Serial1.flush();
  LYNK_GSM_YIELD();
}

bool testAT(uint32_t timeout_ms = 10000L) {
  for (uint32_t start = millis(); millis() - start < timeout_ms;) {
    sendAT("");
    if (readFromModemUntilOK(200L) == 1) { return true; }
    delay(100);
  }
  return false;
}

bool initModem(const char* pinCode = nullptr) {

#if DEBUG_ENABLED
  Serial.print("LynkGSM.ino v");
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
  if (readFromModemUntilOK() != 1) { return false; }

#if DEBUG_ENABLED
  Serial.println("echo off");
#endif

#ifdef DEBUG_ENABLED
  sendAT("+CMEE=2");  // turn on verbose error codes
#else
  sendAT("+CMEE=0");  // turn off error codes
#endif
  readFromModemUntilOK();

#ifdef DEBUG_ENABLED
  String manufacturer = "unknown";
  sendAT("+CGMI");  // 3GPP TS 27.007 standard
  String result;
  if (readFromModem(1000L, result, "OK") != 1) { manufacturer = result; }

  String model = "SIM800";
  sendAT("+CGMM");  // 3GPP TS 27.007 standard
  String result2;
  if (readFromModem(1000L, result2, "OK") != 1) { model = result2; }

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

  //Set ringer sound level
  //SerialAT.print("AT+CRSL=100\r\n");
  //delay(2);

  //Set loud speaker volume level
  //SerialAT.print("AT+CLVL=100\r\n");
  //delay(2);

  //Calling line identification presentation
  //sendAT("+CLIP=1");
  //readFromModemUntilOK();

  return true;
}

void setupModem() {
  // setup modem power pins
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_PWR, OUTPUT);
  pinMode(MODEM_PWR_CONVERTER, OUTPUT);
  digitalWrite(MODEM_RST, HIGH);

  // Turn on the Modem power first
  digitalWrite(MODEM_PWR_CONVERTER, HIGH);

  // Pull down PWR for more than 1 second according to manual requirements
  digitalWrite(MODEM_PWR, HIGH);
  delay(100);
  digitalWrite(MODEM_PWR, LOW);
  delay(1000);
  digitalWrite(MODEM_PWR, HIGH);

  Serial1.begin(57600);

  //TinyGsmAutoBaud(Serial1, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
  if (!initModem()) {
#ifdef DEBUG_ENABLED
    Serial.println("Failed to init modem");
#endif
    //TODO
    //modem.restart();
    //Serial.println("Restarting modem...");
    //delay(10000L);
  }

  lastNetworkAvailableMillis = millis();
}

bool checkModemRegistrationStatus() {
  sendAT("+CREG?");
  String buffer = "";
  uint8_t result = readFromModem(1000L, buffer, "OK");
#ifdef DEBUG_ENABLED
  Serial.println("Modem registration status response:");
  Serial.println(buffer);
#endif
  if (result != 1) return false;
  int start = buffer.indexOf("+CREG:");  //+CREG:0,5OK
  if (start < 0 || start + 9 > buffer.length()) {
    return false;
  }
  int status = buffer[start + 8] - '0';
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

  //TODO
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
  return readFromModemUntilOK(60000L);
}

uint8_t hangup() {
#ifdef DEBUG_ENABLED
  Serial.println("Hangup");
#endif
  sendAT("H");
  return readFromModemUntilOK();
}

uint8_t answer() {
#ifdef DEBUG_ENABLED
  Serial.println("Answer");
#endif
  sendAT("A");
  return readFromModemUntilOK();
}

//0 - no call found, 1- active, 2-held, 3-dialing, 4-alerting, 5-incoming, 6-waiting, 7-disconnect
Call getCall() {
  sendAT("+CLCC");
  Call call;
  call.status = NO_CALL;
  String buffer = "";
  uint8_t result = readFromModem(1000L, buffer, "OK");
  if (result != 1) return call;
  //parse CLCC response +CLCC:<id1>,<dir>,<stat>,<mode>,<mpty>[,<number>,<type>[,<alpha>[,<priority>]]]
  int start = buffer.indexOf("+CLCC:");
  if (start < 0 || start + 30 > buffer.length()) return call;
  call.direction = (CallDirection)(buffer[start + 8] - '0');
  call.status = (CallStatus)(buffer[start + 10] - '0' + 1);
  call.mode = (CallMode)(buffer[start + 12] - '0');
  call.multiparty = (CallMultiparty)(buffer[start + 14] - '0');
  call.number = buffer.substring(start + 17, start + 30);
#ifdef DEBUG_ENABLED
  Serial.print(call.direction ? "Direction=Incoming" : "Direction=Outgoing");
  Serial.print(";Status=");
  Serial.print(call.status == NO_CALL ? "No call" : (call.status == ACTIVE ? "Active" : (call.status == HELD ? "Held" : (call.status == DIALING ? "Dialing" : (call.status == ALERTING ? "Alerting" : (call.status == INCOMING ? "Incoming" : (call.status == WAITING ? "Waiting" : "Disconnect")))))));
  Serial.print(";Mode=");
  Serial.print(call.mode == VOICE ? "Voice" : (call.mode == DATA ? "Data" : "Fax"));
  Serial.print(call.multiparty ? ";Multiparty=Yes" : ";Multiparty=No");
  Serial.print(";Number=");
  Serial.println(call.number);
#endif
  return call;
}

void sendUssd(String number) {
  String command = "+CUSD=1,\"";
  command.concat(number);
  command.concat("\"");
  sendAT(command);
  readFromModemUntilOK();
}


String getDTMFCodes() {
  if(dtmfCodes == "") return dtmfCodes;
  String result = dtmfCodes;
  dtmfCodes.clear();
  return result;
}

String getUssdResponse() {
  if(ussdResponse == "") return ussdResponse;
  String result = ussdResponse;
  ussdResponse.clear();
  return result;
}

//not working good actually
void playBeep(uint8_t count, bool isLong = true) {
  sendAT(isLong ? "+VTD=10" : "+VTD=2");
  readFromModemUntilOK(10000L);

  for (int i = 0; i < count; i++) {
    sendAT("+VTS=A");  // 0-9,*,#,A,B,C,D
    delay(100);
    readFromModemUntilOK(10000L);
  }
}

//play uploaded in SIM800L sounds, see TCallSoundUpdate
bool playSound(String fileName, uint32_t duration = 2000) {
  uint32_t startMillis = millis();
  LYNK_GSM_YIELD();
  String command = String("+CREC=4,\"C:\\User\\");
  command.concat(fileName);
  command.concat("\",0,90");
  sendAT(command);
  bool result = readFromModemUntilOK() == 1;
  delay(duration - (millis() - startMillis));
  return result;
}
