# ESP32_doorbell

This is a video doorbell running on TTGO T-Camera, an ESP32 based development board.
Software is written with ESP IDF framework version 4.x.

**Features**
* When wifi credentials are not yet stored in NVS memory, ESP32 starts up in SoftAP mode
(open network with SSID ESP32); 
you can give in your SSID and passkey, and ESP32 continues in STA mode, 
and tries to connect to your network with the credentials supplied
 address of ESP32 in SoftAP mode: 192.168.4.1
* video stream is avaliable at <ip_of_esp>/capture - can for instance be used in motion running on raspberry pi to do motion detection
* when the button on TTYGO T-Camera is pressed, a photo is sent to Telegram (see configuration below)
* PIR detection is deactivated, because of too many false positives
* OLED displays your name and adress (see configuration below)
* remote logging is enabled, so you can debug even when not connected to laptop; UPD port 3333; on linux host: nc -l -u -p 3333 
* OTA (Over The Air) updates supported via <ip_of_esp>/update; select the bin file and select upload


**Configuration**
privat.h in included in main, but not present in this repo
So this privat.h must be made by you, and shoud contain:

'''
#define TLG_TOKEN "Telegram token to send photo to"
#define TLG_URL "https://api.telegram.org/bot<secretpart>sendPhoto?chat_id=<chat_id>"
#define EXAMPLE_ESP_WIFI_SSID      "yourssid"
#define EXAMPLE_ESP_WIFI_PASS      "yourpassword"
#define EXAMPLE_ESP_MAXIMUM_RETRY  5
#define FAM_NAME "family name"
#define FAM_ADDRESS "address of family"
'''
 
**Known bugs**
Sometimes booting does not succeed (hangs at Oled initialisation), and device reboots
