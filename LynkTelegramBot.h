
#include <FastBot2.h>


FastBot2 bot;
bool needToSentHello = true;

void updateh(fb::Update& u) {
  if (!u.isMessage()) return;

  if (u.message().hasDocument()) {
    if (u.message().document().name().endsWith(".bin")) {  // .bin == ОТА
    bot.sendMessage(fb::Message("OTA begin", u.message().chat().id()));
    bot.updateFlash(u.message().document(), u.message().chat().id());
  } else {
    fb::Fetcher fetch = bot.downloadFile(u.message().document().id());
    if (fetch) {
      fetch.writeTo(Serial);
    }
  }
    return;
  }
#ifdef DEBUG_ENABLED
  Serial.println("NEW MESSAGE id/username/text");
  Serial.println(u.message().chat().id());
  Serial.println(u.message().from().username());
  Serial.println(u.message().text());
#endif
  if (u.message().chat().id() != CHAT_ID) return;

  bot.sendMessage(fb::Message(u.message().text(), u.message().chat().id()));
}

void setupTelegram() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  bot.attachUpdate(updateh);  // подключить обработчик обновлений
  bot.setToken(BOT_TOKEN);    // установить токен
}

bool isWifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void tickTelegram() {
  if(needToSentHello) {
    needToSentHello = false;
     bot.sendMessage(fb::Message("ESP Started(waiting OTA...)", CHAT_ID));
  }
  bot.tick();
}

void sendToChat(String msg) {
  bot.sendMessage(fb::Message(msg, CHAT_ID));
}