# lynk-alarm-gsm
GSM Alarm System based on TCall module

## Setup:
1. Install libs:
*  [FastBot2](https://github.com/GyverLibs/FastBot2/)

2. Create secrets.h and define next variables:
```
#define NUMBER_LENGTH 13
//predefine admin numbers that can control alarm(up to 8 numbers)
const String ALLOW_INCOME_NUMBERS = "+381234567891+381234567892+381234567893+381234567894+381234567895+381234567896+381234567897+381234567898"; 
#define WIFI_SSID "MY_WIFI";
#define WIFI_PASS "MY_PASSWORD";
#define BOT_TOKEN "1234567890:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
#define CHAT_ID "123456789";
```
