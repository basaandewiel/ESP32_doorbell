# ESP32_doorbell

This is a videodoorbell running on TTGO T-Camera a ESP32 based development board.
Software is written with ESP IDF framework version 4.x.

**Configuration**
privat.h in included in main, but not present in this repo
So this privat.h must be made by you, and shoud contain:
#define TLG_TOKEN "Telegram token to send photo to"
#define TLG_URL "https://api.telegram.org/bot<secretpart>sendPhoto?chat_id=<chat_id>"

#define EXAMPLE_ESP_WIFI_SSID      "yourssid"

#define EXAMPLE_ESP_WIFI_PASS      "yourpassword"

#define EXAMPLE_ESP_MAXIMUM_RETRY  5
