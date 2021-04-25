# ESP32_doorbell

This is a videodoorbell running on TTGO T-Camera a ESP32 based development board.
Software is written with ESP IDF framework version 4.x.

**Features**
* When wifi credentials are not yet stored in NVS memory, ESP32 starts up in SoftAP mode (open network with SSID ESP); you can give in your SSID and passkey, and ESP32 continues in STA mode, and tries to connect to your network
* video stream is avaliable at <ip_of_esp>/capture - can for instance be used in motion running on raspberry pi
* when buttn is pressed, a photo is sent to Telegram
* PIR detection is deactivated, because of false positives
* OLED displays your name and adress

**Configuration**
privat.h in included in main, but not present in this repo
So this privat.h must be made by you, and shoud contain:
#define TLG_TOKEN "Telegram token to send photo to"
#define TLG_URL "https://api.telegram.org/bot<secretpart>sendPhoto?chat_id=<chat_id>"

#define EXAMPLE_ESP_WIFI_SSID      "yourssid"

#define EXAMPLE_ESP_WIFI_PASS      "yourpassword"

#define EXAMPLE_ESP_MAXIMUM_RETRY  5
