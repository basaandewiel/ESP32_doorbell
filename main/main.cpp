/* baswi
Streaming webserver code copied en bewerkt from: https://github.com/jameszah/ESP32-CAM-Video-Recorder-junior/blob/master/ESP32-CAM-Video-Recorder-junior-10x.ino

TODO
* wifi provisioning, https://docs.espressif.com/projects/esp-jumpstart/en/latest/index.html
  * complexe (grote code) en additional phone app nodig; ik wil gewoon simpele AP met webinterface
* https://github.com/espressif/esp-idf/issues/1503
* https://arduino-esp8266.readthedocs.io/en/latest/faq/a02-my-esp-crashes.html
* https://github.com/alanesq/CameraWifiMotion/blob/master/CameraWifiMotion/CameraWifiMotion.ino
* init Oled fails sometimes; does not occur if camera_init is removed
* capture=1600x1200; pic in :1280*960
* zoek alle @@@
* check project - low res AND/OR greyscale mov detection IN ESP
  ESP does lores motion detection (see https://eloquentarduino.github.io/2020/05/easier-faster-pure-video-esp32-cam-motion-detection/)
  IF mov detected
    set cam to hires; this will trigger motion@RPI3 to start recording (see motion)
    REPEAT
    UNTIL motion != detected
    set cam to lores
  FI
  IF (PIR or button)
    pause streamer
    set cam to hires
    take and send photo
    resume streamer
  FI
* prevent http disconnecting after each capture
* over the air updates
  * only mark image Valid, as it works OK
* exception handing, https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/fatal-errors.html
* store network credentials in NVS; 
    Store the value of key 'my_key' to NVS 
    nvs_set_u32(nvs_handle, "my_key", chosen_value);

    Read the value of key 'my_key' from NVS
    nvs_get_u32(nvs_handle, "my_key", &chosen_value);

    Register 3 second press callback
    iot_button_add_on_press_cb(btn_handle, 3, button_press_3sec_cb, NULL);
    static void button_press_3sec_cb(void *arg)
    {
      nvs_flash_erase();
      esp_restart();
    }
* custom watchdog - if no internet connection for x seconds ->reboot
* maak code netjes

Kconfig setup
=============
If you have a Kconfig file, copy the content from
https://github.com/espressif/esp32-camera/blob/master/Kconfig into it.
In case you haven't, copy and paste this Kconfig file inside the src directory.
This Kconfig file has definitions that allows more control over the camera and
how it will be initialized.
* enable right type of camera
* set core affinity of camera

sdkconfig
=========
CONFIG_ESP32_SPIRAM_SUPPORT=y  #Enable PSRAM on sdkconfig:
CONFIG_EXAMPLE_WIFI_SSID="ssid"
CONFIG_EXAMPLE_WIFI_PASSWORD="passphrase"
CONFIG_ESP_TLS_USING_MBEDTLS=y
CONFIG_ESP_MAIN_TASK_STACK_SIZE=32000

Enable app rollback support - will roll back when neither mark_valid nor mark_invalid is called.
allows you to track the *first* boot of a new application. In this case, the application must
confirm its operability by calling *esp_ota_mark_app_valid_cancel_rollback()* function, 
otherwise the application will be rolled back upon reboot. 
It allows you to control the operability of the application during the boot phase. 
Thus, a new application has only one attempt to boot successfully.


More info on
https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html#config-esp32-spiram-support
*/

#include <esp_event.h>
#define LOG_LOCAL_LEVEL ESP_LOG_INFO //default log level, must placed before include
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>

#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "nvs.h"
//#define CONFIG_EXAMPLE_FIRMWARE_UPG_URL "http://192.168.1.13:3000/hello-world.bin"
#define CONFIG_EXAMPLE_OTA_RECV_TIMEOUT 5000


/*an ota data write buffer ready to write to the flash*/
#define BUFFSIZE 1024
#define HASH_LEN 32 /* SHA-256 digest length */
//static char ota_write_data[BUFFSIZE + 1] = { 0 };
#define OTA_URL_SIZE 256
#define CONFIG_APP_PROJECT_VER_FROM_CONFIG 1 
#define CONFIG_APP_PROJECT_VER "va0.52"


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_camera.h"
#include <stdio.h>
#include <cstring>
//#include <string> //without .h; standard C++ lib

#include "esp_wifi.h"
#include "freertos/event_groups.h"

#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_http_server.h"
#include "time.h"
#include "esp_sntp.h"

#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
//#include "addr_from_stdin.h"

//OLED
#include "Display.h"
#include "SSD1306.h"
//#include <SPI_PIF.h>
#include "OLED.h"

// I2C Definitions
#include "I2C_PIF.h"
#define scl GPIO_NUM_22 //baswi changed from 19
#define sda GPIO_NUM_21 //baswi changed from 18

#include "privat.h" //include private defines, in .gitignore@@@

//baswi pins for TTGO T-camera
#define CAM_PIN_PWDN    -1 //power down is not used
#define CAM_PIN_RESET   -1 //software reset will be performed
#define CAM_PIN_XCLK    4
#define CAM_PIN_SIOD    18
#define CAM_PIN_SIOC    23

#define CAM_PIN_D7      36
#define CAM_PIN_D6      37
#define CAM_PIN_D5      38
#define CAM_PIN_D4      39
#define CAM_PIN_D3      35
#define CAM_PIN_D2      14
#define CAM_PIN_D1      13
#define CAM_PIN_D0      34
#define CAM_PIN_VSYNC   5
#define CAM_PIN_HREF    27
#define CAM_PIN_PCLK    25

#define BUTTON_GPIO GPIO_NUM_15
#define PIR_PIN GPIO_NUM_19 //PIR cannot wake device, button can
#define BUTTON_PRESSED 1 //value put in gpio_queue when button interrupt occurs
#define PIR_ACTIVATED  2 //value put in gpio_queue when PIR interrupt occurs

//defines forSoftAP
#define ESP_WIFI_SOFTAP_SSID      "ESP32"
#define ESP_WIFI_SOFTAP_PASS      "" //open network
#define ESP_WIFI_SOFTAP_CHANNEL   11
#define ESP_WIFI_SOFTAP_MAX_STA_CONN       4


static const char *TAG = "***";
static void obtain_time(void);
static void initialize_sntp(void);
time_t now;
char localip[20];
wifi_config_t glob_wifi_config; //used to store wifi_config to connect to network
char taglevel[32] = "***";

static void http_cleanup(esp_http_client_handle_t client)
{
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}


void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_UXGA,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG
    //FRAMESIZE_SXGA works OK (motion+telegram)
    //UXGA gives mbed_tls failures; don't know why
//    .frame_size = FRAMESIZE_UXGA,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG
                                     //UXGA Works in separate streamer_stable; 
                                     //NB!!! malloc of framebuffer maybe needs to be adapted
    .jpeg_quality = 12, //0-63 lower number means higher quality, was 12; 10 should be possible if PSRAM is present
    // However, very low numbers for image quality, specially at higher resolution can make the ESP32-CAM to crash or it may not be able to take the photos properly.
    //So, if you notice that the images taken with the ESP32-CAM are cut in half, or with strange colors, that’s probably a sign that you need to lower the quality (select a higher number).
    .fb_count = 1       //if more than one, i2s runs in continuous mode. Use only with JPEG
};

static EventGroupHandle_t wifi_event_group;


bool wifi_has_ip = false;
bool sendAlert2Telegram = false;
bool streamer_paused = false;

#define MAX_HTTP_OUTPUT_BUFFER 2048  
#define POST_DATA_LENGTH 2048

uint8_t* framebuffer;
int framebuffer_len = -1; //to indicate that no photo is retreived yet
float most_recent_fps = 0;
int most_recent_avg_framesize = 0;
static const char devname[] = "videodoorbell"; // name of your camera for mDNS, Router, and filenames
static const char vernum[] = "v10";
int c1_framesize = 7;                //  10 UXGA, 9 SXGA, 7 SVGA, 6 VGA, 5 CIF
int c1_quality = 10;                 //  quality on the 1..63 scale  - lower is better quality and bigger files - must be higher than the jpeg_quality in camera_config
int c1_avi_length = 1800;            // how long a movie in seconds -- 1800 sec = 30 min

int c2_framesize = 10;
int c2_quality = 10;
int c2_avi_length = 1800;

int c1_or_c2 = 1;
int framesize = c1_framesize;
int quality = c1_quality;
int avi_length = c1_avi_length;

// begin deboucing definitions
#define DEBOUNCETIME_BUTTON 400 //msec
#define DEBOUNCETIME_PIR 10000 //10 sec; don't send PIR events within 10 seconds

static xQueueHandle gpio_evt_button_queue = NULL; //used for button interrupts
static xQueueHandle gpio_evt_PIR_queue = NULL; //used for PIR interrupts

DRAM_ATTR const uint32_t button_pressed = (uint32_t) BUTTON_PRESSED; //baswi  value put in queue 201229: inserted DRAM_ATTR const
static void IRAM_ATTR handleButtonInterrupt() {
// Interrupt Service Routine - Keep it short!
//   201212 do not get time and set var's; only put something in queue, otherweise WDG expires
//   201229 ISR may only call functions placed into IRAM or functions present in ROM   
    gpio_intr_disable(BUTTON_GPIO); //disable inter to prevent lot of bouncing inter
    xQueueSendFromISR(gpio_evt_button_queue, &button_pressed, NULL);
}

DRAM_ATTR const uint32_t PIR_activated = (uint32_t) 2; //baswi value put in queue 201229: inserted DRAM_ATTR const
static void IRAM_ATTR handlePIRInterrupt() {
//   201229 ISR may only call functions placed into IRAM or functions present in ROM
    gpio_intr_disable(PIR_PIN); //disable inter to prevent lot of PIR interrupts
    xQueueSendFromISR(gpio_evt_PIR_queue, &PIR_activated, NULL);
}


static esp_err_t init_camera()
{
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }
    return ESP_OK;
}

//baswi: tested; extern C is necessary, to prevent reference not found linker errors
extern "C" {
    #include <remote_log.h>
}

const int CONNECTED_BIT = BIT0;

static void task_sendAlert2Telegram(void *ignore) {
  for (;;) {
//    ESP_LOGD(TAG, "ENTER Loop task_sendAlert2Telegram");
    if (sendAlert2Telegram == true) { //@@@could also be implemented via queue event; is that better?
      char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};   // Buffer to store response of http request
      int content_length = 0;
      esp_http_client_config_t config = {}; // prevent compiler warnings
      config = {}; // prevent compiler warnings
      config = {
        .url = "http://httpbin.org/get",
        //only used in the GET below
      };
      esp_http_client_handle_t client = esp_http_client_init(&config);

      // GET Request
      // baswi: the uncommented lines are really neccesary, otherwise I get no response on my POST message below
      //        I really don't understand why
      //        Behaviour is the same for httpbin.org and api.telegram.org
      esp_http_client_set_method(client, HTTP_METHOD_GET);
      ESP_ERROR_CHECK(esp_http_client_open(client, 0));
      content_length = esp_http_client_fetch_headers(client);
      if (content_length < 0) {
        ESP_LOGE(TAG, "HTTP client fetch headers failed");
      } else {
        int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
        if (data_read >= 0) {
          ESP_LOGD(TAG, "extra GET before POST - HTTP GET Status = %d, content_length = %d",
                   esp_http_client_get_status_code(client),
                   esp_http_client_get_content_length(client));
          } else {
            ESP_LOGE(TAG, "Failed to read response");
          }
      }
      //esp_http_client_close(client);  //6mrt
      http_cleanup(client); //close & cleanup
 
      //only get new photo when there is no recent photo taken by capture_handler
      if (framebuffer_len == -1) {
        ESP_LOGI(TAG, "Fetching NEW photo, because there is no recent photo");
        camera_fb_t * fb = NULL; 
        fb = esp_camera_fb_get(); //get photo
        if(!fb) {
          ESP_LOGE(TAG, "Camera capture failed");
          return;
        }
        framebuffer_len = fb->len;
        memcpy(framebuffer, fb->buf, framebuffer_len);
        esp_camera_fb_return(fb);
        ESP_LOGD(TAG, "framebuffer has size: %d", framebuffer_len);
      }

      // Use @myidbot to find out the chat ID of an individual or a group
      // Also note that you need to click "start" on a bot before it can
      // message you

      char post_data[POST_DATA_LENGTH];
      uint wlen;
      snprintf(post_data, POST_DATA_LENGTH, "----baswiboundary127\r\n"
                                  "Content-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\n"
                                  "Content-Type: image/jpeg\r\n\r\n");
        
      ESP_ERROR_CHECK(esp_http_client_set_url(client, TLG_URL));
      ESP_ERROR_CHECK(esp_http_client_set_method(client, HTTP_METHOD_POST));

      char *boundary = "----baswiboundary127\r\n";
      char *end_boundary = "\r\n----baswiboundary127--\r\n";
      char header[512];
      snprintf(header,512, "multipart/form-data; boundary=--baswiboundary127");    
      
      esp_http_client_set_header(client, "Content-Type", header);

      streamer_paused = true; //pause streamer so fb is not overwritten during sending it to telegram
      ESP_LOGI(TAG, "pause streamer");
      //this is necessary (tested), because stream_handler (now capture_handler, because that is now called
      //by motion@RPI3) is executed 'during' http_client_open, and
      //even during http_write; otherwise framebuffer and framebuffer_len would be changed
      //CONDITON: camera_streamer is paused and has saved last fb in 
      // framebuffer and 
      // framebuffer_len
      ESP_LOGI(TAG, "framebuffer_len = %d", framebuffer_len);

      int total_length = strlen(post_data) + framebuffer_len + strlen(end_boundary); //length of pre_data+content+end_trailer
      ESP_LOGD(TAG, "total length= %d", total_length);
      ESP_ERROR_CHECK(esp_http_client_open(client, total_length)); //2nd param is use in "Content-Length: (wireshark)
      //source: client_open->client_request_send: sends client->request->buffer->data (volgens mij stuurt hij header)
      //source: sets client->request->headers
      //source: open connection and sends header; writes client->request->buffer->data

      ESP_LOGD(TAG, "Connection OPENED succesfully");
      wlen = esp_http_client_write(client, post_data, strlen(post_data));
      // write data to the HTTP connection previously opened by esp_http_client_open
      ESP_LOGD(TAG, "Wrote %d bytes", wlen);
      //do NOT expect HTTP response now, because HTTP message is not yet completely sent
    
      //Not usefull for client_write. ESP_ERROR_CHECK(esp_http_client_set_post_field(client, post_data, strlen(post_data)));
        //source: sets client->post_data
        //source: post_data is only send via client_perform
        //source: client_write doet NIKS met post_data gezet via set_post_field
        //source: Je kan local post_data wel als param aan client_write meegeven

      uint frame_buffer_index = 0; //next byte to copy from framebuffer to post-data
      uint post_data_pos2fil = 0; //next position to write in post_data; reset for each part
      uint number_of_full_parts = (framebuffer_len / 1024);
      for (uint i=1; i<=number_of_full_parts; i++) {
        for (post_data_pos2fil=0; post_data_pos2fil<1024; post_data_pos2fil++) {
          post_data[post_data_pos2fil] = framebuffer[frame_buffer_index];
          frame_buffer_index++;
        }
        wlen = esp_http_client_write(client, post_data, post_data_pos2fil); //not +1, becuase index was already incremented
        ESP_LOGD(TAG, "Wrote %d bytes", wlen);
      }
      //sendlastpart
      size_t remainder = framebuffer_len%1024;
      ESP_LOGI(TAG, "remainder= %d", remainder);
      for (post_data_pos2fil=0; post_data_pos2fil<remainder; post_data_pos2fil++) {
        post_data[post_data_pos2fil] = framebuffer[frame_buffer_index];
        frame_buffer_index++;
      }
      //add last boundary
      strncpy(&post_data[post_data_pos2fil], "\r\n----baswiboundary127--\r\n", (strlen("\r\n----baswiboundary127--\r\n")+1));
      post_data_pos2fil += strlen("\r\n----baswiboundary127--\r\n"); //adapt index due to previous line
          
      ESP_LOGD(TAG, "LAST PART - post_data2fil= %d", post_data_pos2fil);
      wlen = esp_http_client_write(client, post_data, post_data_pos2fil); //post_data2fil=next pos to fil=equal length filled
      ESP_LOGD(TAG, "Wrote %d bytes", wlen);
      streamer_paused = false; //enable streamer again
      ESP_LOGI(TAG, "resume streamer");
      ESP_LOGI(TAG, "picture sent to Telegram");

      int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
      if (data_read >= 0) {
        ESP_LOGD(TAG, "HTTP GET Status = %d, content_length = %d",
        esp_http_client_get_status_code(client),
        esp_http_client_get_content_length(client));      
      } else {
        ESP_LOGE(TAG, "Failed to read response");
      } 
      //esp_http_client_cleanup(client); //6mrt
      http_cleanup(client); //close & cleanup
      sendAlert2Telegram = false;
      framebuffer_len = -1; //set to invalid length, so that this photo is not reused
    } //sentalert==true
    //ESP_LOGD(TAG, "EXIT loop task_sendAlert2Telegram");
    // do not send alerts faster than every x seconds, and yield OS
    vTaskDelay(500/portTICK_PERIOD_MS); //not too long, because PIR and button interrupts are already disabled some time after trigger
  }
}


static void task_handle_button_interrupt(void *pvParameter) {
  ESP_LOGI(TAG, "Debounced ButtonRead Task running on core: %d ", xPortGetCoreID());
  uint32_t io_num;

  for(;;) {
    if(xQueueReceive(gpio_evt_button_queue, &io_num, portMAX_DELAY) == pdTRUE) {
      ESP_LOGI(TAG, "button interrupt received");
      //xTaskCreate((TaskFunction_t)&task_ssd1306_display_text, "ssd1306_display_text",  2048,
      //       		  (void *)"DING        \nDONG        \n", 6, NULL);
      //ring the bell
      //@@@esp_http_client_handle_t http_client = esp_http_client_init(&http_config); //seems that client_init once is not enought; maybe the 
      
      sendAlert2Telegram = true;
      
      vTaskDelay(400/portTICK_PERIOD_MS); //wait 400 msec, then enable interrupts again
      gpio_intr_enable(BUTTON_GPIO);
    }
  }
}

static void task_handle_PIR_interrupt(void *pvParameter) {
  ESP_LOGI(TAG, "Debounced PIR read Task running on core: %d ", xPortGetCoreID());
  uint32_t io_num;

  for(;;) {
    if(xQueueReceive(gpio_evt_PIR_queue, &io_num, portMAX_DELAY) == pdTRUE) {
      ESP_LOGI(TAG, "PIR interrupt received");
      sendAlert2Telegram = true;
      vTaskDelay(10000/portTICK_PERIOD_MS); //wait 10000 msec, then enable interrupts again
      //during this delay, task_sendalert2telegram must be activated; to ensure this latter task is given a higher prio than current task
      gpio_intr_enable(PIR_PIN);
    }
  }
}


httpd_handle_t camera_httpd = NULL;
char the_page[4000];

/*
static esp_err_t index_handler(httpd_req_t *req) {
//  Serial.print("http index, core ");  Serial.print(xPortGetCoreID());
//  Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

  time(&now);
  const char *strdate = ctime(&now);

//baswi PROGMEM removed - arduino specific
  const char msg[] = R"(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>%s ESP32-CAM Video Recorder</title>
</head>
<body>
<h1>Camera streamer <br><font color="red">%s</font></h1><br>
 Framesize %d, Quality %d <br>
 <br>
 <h3><a href="http://%s/stream">Stream at 5 fps </a></h3>
 <h3><a href="http://%s/photos">Photos - 15 saveable photos @ every 2 seconds </a></h3>
</body>
</html>)";

  sprintf(the_page, msg, devname, strdate,
          camera_config.frame_size, camera_config.jpeg_quality,
          localip, localip); 

  httpd_resp_send(req, the_page, strlen(the_page));
  return ESP_OK;
} */

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//
static esp_err_t photos_handler(httpd_req_t *req) {

  //Serial.print("http photos, core ");  Serial.print(xPortGetCoreID());
  //Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

  time(&now);
  const char *strdate = ctime(&now);

  const char msg[] = R"(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>%s ESP32-CAM Video Recorder Junior</title>
</head>
<body>
<h1>%s<br>ESP32-CAM Video Recorder <br><font color="red">%s</font></h1><br>
 <br>
 One photo every 2 seconds for 30 seconds - roll forward or back - refresh for more live photos
 <br>
<br><div id="image-container"></div>
<script>
document.addEventListener('DOMContentLoaded', function() {
  var c = document.location.origin;
  const ic = document.getElementById('image-container');  
  var i = 1;
  
  var timing = 2000; // time between snapshots for multiple shots
  function loop() {
    ic.insertAdjacentHTML('beforeend','<img src="'+`${c}/capture?_cb=${Date.now()}`+'">')
    ic.insertAdjacentHTML('beforeend','<br>')
    ic.insertAdjacentHTML('beforeend',Date())
    ic.insertAdjacentHTML('beforeend','<br>')
    i = i + 1;
    if ( i <= 15 ) {             // 1 frame every 2 seconds for 15 seconds 
      window.setTimeout(loop, timing);
    }
  }
  loop();
  
})
</script><br>
</body>
</html>)";

  sprintf(the_page, msg, devname, devname, strdate );

  httpd_resp_send(req, the_page, strlen(the_page));
  return ESP_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  Streaming stuff based on Random Nerd
//
//

#define PART_BOUNDARY "123456789000000000000987654321"

//static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

/*
static esp_err_t stream_handler(httpd_req_t *req) {
  //NOT USED anymore; motion@RPI3 GETs /capture
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  //Serial.print("stream_handler, core ");  Serial.print(xPortGetCoreID());
  //Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  //while (true) { //commented; PI3 will pull image; and call capture_handler
    if (streamer_paused == false) { //do NOT get new fb when I try to send current framebuffer to Telegram
      ESP_LOGD(TAG, "Stream_handler: Getting camera fb");
      fb = esp_camera_fb_get(); //@@@crashes in this function sometimes; only if OLED display is used??
      framebuffer_len = fb->len;
      memcpy(framebuffer, fb->buf, framebuffer_len);
      esp_camera_fb_return(fb);

      _jpg_buf_len = framebuffer_len;
      _jpg_buf = framebuffer;

      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
      if (res != ESP_OK) {
        return res;
      }

      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
      if (res != ESP_OK) {
        return res;
      }

      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
      if (res != ESP_OK) {
        return res;
      }
    } else {
      ESP_LOGD(TAG, "pause_streamer = TRUE");
    }
    //vTaskDelay(500/portTICK_PERIOD_MS);      // 200 ms = 5 fps !!!
    //no delay, so that RPI can grab next image asap
    //delay blocks also other uri's (denk ik)
    //during this time for instance task_handle_PIR_inter can run, which sets streamer_paused to true
  //}
  return res;
} */


static esp_err_t capture_handler(httpd_req_t *req) {
  //this function is called by URL/capture via motion@RPI3
  camera_fb_t * fb;
  esp_err_t res = ESP_OK;
  char fname[100]; //@@@never set to value; used in set_hdr below; why length 100?

  ESP_LOGD(TAG, "capture, core: %d ", xPortGetCoreID());      
  ESP_LOGD(TAG, "priority = %d", uxTaskPriorityGet(NULL));
                                                                                                                          
  if (streamer_paused == false) { //do NOT get new fb when I try to send current framebuffer to Telegram
    ESP_LOGD(TAG, "Capture_handler: Getting camera fb");
    fb = esp_camera_fb_get();
    framebuffer_len = fb->len;                                            
    memcpy(framebuffer, fb->buf, framebuffer_len);                                           
    esp_camera_fb_return(fb);

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", fname);
                                                                     
    res = httpd_resp_send(req, (const char *)framebuffer, framebuffer_len);
  }
  return res;
}



/*
 * Serve OTA update portal (index.html)
 */
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

esp_err_t index_get_handler(httpd_req_t *req)
{
	httpd_resp_send(req, (const char *) index_html_start, index_html_end - index_html_start);
	return ESP_OK;
}


/*
 * Handle OTA file upload
 * source:https://github.com/Jeija/esp32-softap-ota/tree/master/main
 */
esp_err_t update_post_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "UPDATE POST HANDLER");
	char buf[1000];
	esp_ota_handle_t ota_handle;
	int remaining = req->content_len;

	const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
	ESP_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle));

	while (remaining > 0) {
		int recv_len = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));

		// Timeout Error: Just retry
		if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
			continue;

		// Serious Error: Abort OTA
		} else if (recv_len <= 0) {
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
			return ESP_FAIL;
		}

		// Successful Upload: Flash firmware chunk
		if (esp_ota_write(ota_handle, (const void *)buf, recv_len) != ESP_OK) {
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Error");
			return ESP_FAIL;
		}

		remaining -= recv_len;
	}

	// Validate and switch to new OTA image and reboot
	if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK) {
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error");
			return ESP_FAIL;
	}

	httpd_resp_sendstr(req, "Firmware update complete, rebooting now!");

	vTaskDelay(500 / portTICK_PERIOD_MS);

  udp_client_free();
	esp_restart();

	return ESP_OK;
}



/*
 * Handle send2telegram request
 */
esp_err_t setSend2TelegramTrue (httpd_req_t *req)
{
  ESP_LOGI(TAG, "REQUEST TO SEND TO TELEGRAM");
	sendAlert2Telegram = true;
	return ESP_OK;
}

/*
 * Handle reboot request
 */
esp_err_t rebootESP (httpd_req_t *req)
{
  ESP_LOGI(TAG, "REQUEST TO *REBOOT*");

  udp_client_free();
	esp_restart();
	return ESP_OK;
}

/* An HTTP GET handler */
static esp_err_t setWifiParams (httpd_req_t *req)
{
  ESP_LOGI(TAG, "setWifiParams");
  char*  buf;
  size_t buf_len;

  /* Read URL query string length and allocate memory for length + 1,
   * extra byte for null termination */
  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char *) malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      ESP_LOGI(TAG, "Found URL query => %s", buf);
      char ssid[32]; //%%%
      char passkey[32];
      // Get value of expected key from query string
      if (httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid)) == ESP_OK) {
        ESP_LOGI(TAG, "Found network SSID =%s", ssid);
      }
      if (httpd_query_key_value(buf, "passkey", passkey, sizeof(passkey)) == ESP_OK) {
        ESP_LOGI(TAG, "Found network PASSKEY =%s", passkey);
      }
    }
    free(buf);
  }
  return ESP_OK;
}


/* An HTTP GET handler */
static esp_err_t SetLogLevel (httpd_req_t *req)
{
  esp_log_level_set("*", ESP_LOG_DEBUG);
  ESP_LOGI(TAG, "setloglevel*");
    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = (char *) malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char *) malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
          ESP_LOGI(TAG, "Found URL query => %s", buf);
          char var[32];
          char val[32];
          /* Get value of expected key from query string */
          if (httpd_query_key_value(buf, "var", var, sizeof(var)) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query parameter => var=%s", var);
          }
          if (httpd_query_key_value(buf, "val", val, sizeof(val)) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query parameter => val=%s", val);
          }

          if (strcmp(var, "tag") == 0) {
            //TAG to set loglevel of
            ESP_LOGI(TAG, "TAG *val* to set level of: %s", val);
            strncpy(taglevel, val, (strlen(val)+1)); //+1 to also copy NULL character;
            //otherwise old strings stay present, if they were longer than new string
            ESP_LOGI(TAG, "taglevel: %s", taglevel);
          }
          else if (strcmp(var, "loglevel") == 0) {
            //case '0' does not work; '0' is compiled to char[2] (incl.trailing zero)
            //and matches only if char[] is in exact same memory location as val
            //Switch statements work on int values (or enum), but not on char arrays.
            esp_log_level_t level=ESP_LOG_NONE;
            if (strcmp(val, "0") == 0) {
              level = ESP_LOG_NONE; ESP_LOGI(TAG,"NONE");
            }
              else if (strcmp(val, "1") == 0) {
                level = ESP_LOG_ERROR; ESP_LOGI(TAG,"ERROR");
              }
              else if (strcmp(val, "2") == 0) {
                level = ESP_LOG_WARN; ESP_LOGI(TAG,"WARN"); 
              }
              else if (strcmp(val, "3") == 0) {
                level = ESP_LOG_INFO; ESP_LOGI(TAG,"INFO");
              }
              else if (strcmp(val, "4") == 0) {
                level = ESP_LOG_DEBUG; ESP_LOGI(TAG,"DEBUG");
              }
              else if (strcmp(val, "5") == 0) {
                level = ESP_LOG_VERBOSE; ESP_LOGI(TAG,"VERBOSE");
              }
              else {
                ESP_LOGE(TAG, "no valid loglevel");
              }
              if (taglevel != NULL) {
                ESP_LOGI(TAG, "set taglevel= %s; to level= %d", taglevel, level);
                esp_log_level_set(taglevel, level);
              }
            }
        }
        free(buf);
    }

    /* Set some custom headers
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2"); */

    // Send response with custom headers and body set as the
    //  string passed in user context
    // ***baswi: if nothing is returned then maxfd increases till 64 => crash
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, strlen(resp_str));
    

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. 
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }*/
    return ESP_OK;
}


void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  ESP_LOGI(TAG, "http task prio: %d", config.task_priority);

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
//    .handler   = index_handler,
     .handler = index_get_handler, //handler for OTA update web page
    .user_ctx  = NULL
  };
  httpd_uri_t capture_uri = {
    .uri       = "/capture",
    .method    = HTTP_GET,
    .handler   = capture_handler, //baswi: rpi3 motion does NOT require stream, but separate images
    //.handler   = NULL, //baswi set to NULL, because I only need stream_handler; avoid unnes fb_gets from camera
    .user_ctx  = NULL
  };
//  httpd_uri_t stream_uri = {
//    .uri       = "/stream",
//   .method    = HTTP_GET,
//    .handler   = stream_handler,
//    .user_ctx  = NULL
//  };  
//  httpd_uri_t photos_uri = {
//    .uri       = "/photos",
//    .method    = HTTP_GET,
//    .handler   = photos_handler,
//    .user_ctx  = NULL
//  };

  httpd_uri_t update_post = {
	  .uri	  = "/update",
	  .method   = HTTP_POST,
	  .handler  = update_post_handler,
	  .user_ctx = NULL
  };

  httpd_uri_t sendtelegram_uri = {
	  .uri	  = "/telegram",
	  .method   = HTTP_GET,
	  .handler  = setSend2TelegramTrue,
	  .user_ctx = NULL
  };

  httpd_uri_t reboot_uri = {
	  .uri	  = "/reboot",
	  .method   = HTTP_GET,
	  .handler  = rebootESP,
	  .user_ctx = NULL
  };

  httpd_uri_t setWifiParams_uri = {
	  .uri	  = "/control",
	  .method   = HTTP_GET,
	  .handler  = setWifiParams,
//for POST	  .user_ctx = NULL
    .user_ctx = (void *) "Hello Bassie" //is returned after get; 
    //GET: if nothing is returned then maxfd increases till 64 => crash
  };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
//    httpd_register_uri_handler(camera_httpd, &stream_uri);
//    httpd_register_uri_handler(camera_httpd, &photos_uri);
    httpd_register_uri_handler(camera_httpd, &update_post);
    httpd_register_uri_handler(camera_httpd, &sendtelegram_uri);
    httpd_register_uri_handler(camera_httpd, &reboot_uri);
    httpd_register_uri_handler(camera_httpd, &setWifiParams_uri);
  }
  ESP_LOGI(TAG, "HTTP server started");
}

static void print_sha256 (const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i) {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG, "%s: %s", label, hash_print);
}



static bool diagnostic(void)
{
  #define CONFIG_EXAMPLE_GPIO_DIAGNOSTIC GPIO_NUM_4
    /*
    config EXAMPLE_GPIO_DIAGNOSTIC
        int "Number of the GPIO input for diagnostic"
        range 0 39
        default 4
        help
            Used to demonstrate how a rollback works.
            The selected GPIO will be configured as an input with internal pull-up enabled.
            To trigger a rollback, this GPIO must be pulled low while the message
            `Diagnostics (5 sec)...` which will be on first boot.
If GPIO is not pulled low then the operable of the app will be confirmed.
baswi: snap nog niet, wat je dan ik code moet doen, zodat ie als valid beschouwd wordt
*/

    gpio_config_t io_conf;
    io_conf.intr_type    = (gpio_int_type_t)GPIO_PIN_INTR_DISABLE;
    io_conf.mode         = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << CONFIG_EXAMPLE_GPIO_DIAGNOSTIC);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "Diagnostics (5 sec)...");
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    bool diagnostic_is_ok = gpio_get_level(CONFIG_EXAMPLE_GPIO_DIAGNOSTIC);

    gpio_reset_pin(CONFIG_EXAMPLE_GPIO_DIAGNOSTIC);
    return true;
}

static void init_ota(void)
{
//OTA things
  uint8_t sha_256[HASH_LEN] = { 0 };
  esp_partition_t partition;

  // get sha256 digest for the partition table
  partition.address   = ESP_PARTITION_TABLE_OFFSET;
  partition.size      = ESP_PARTITION_TABLE_MAX_LEN;
  partition.type      = ESP_PARTITION_TYPE_DATA;
  esp_partition_get_sha256(&partition, sha_256);
  print_sha256(sha_256, "SHA-256 for the partition table: ");

  // get sha256 digest for bootloader
  partition.address   = ESP_BOOTLOADER_OFFSET;
  partition.size      = ESP_PARTITION_TABLE_OFFSET;
  partition.type      = ESP_PARTITION_TYPE_APP;
  esp_partition_get_sha256(&partition, sha_256);
  print_sha256(sha_256, "SHA-256 for bootloader: ");

  // get sha256 digest for running partition
  esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
  print_sha256(sha_256, "SHA-256 for current firmware: ");

  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      // run diagnostic function ...
      bool diagnostic_is_ok = diagnostic(); //%%% no message below received
      if (diagnostic_is_ok) {
        ESP_LOGI(TAG, "Diagnostics completed successfully! Continuing execution ...");
        esp_ota_mark_app_valid_cancel_rollback();
      } else {
        ESP_LOGE(TAG, "Diagnostics failed! Start rollback to the previous version ...");
        esp_ota_mark_app_invalid_rollback_and_reboot();
      }
    }
  }
}

static void init_NVS(void)
{
  // Initialize NVS.
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // OTA app partition table has a smaller NVS partition size than the non-OTA
    // partition table. This size mismatch may cause NVS initialization to fail.
    // If this happens, we erase NVS partition and initialize NVS again.
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK( err );
}

static void configure_PIR(void)
{
// PRE: isr_handler must be started: gpio_install_isr_service(0);

  gpio_config_t pir_config;
  
  //configure PIR
  pir_config.mode = GPIO_MODE_INPUT;        	//Set as Input
  pir_config.intr_type = GPIO_INTR_HIGH_LEVEL; 
  pir_config.pin_bit_mask = (1 << PIR_PIN); //Bitmask
  //%%%hier crasht ie soms; reset pin needed?
  //pir_config.pin_bit_mask = ((1 << BUTTON_GPIO) | (1 << PIR_PIN)); //Bitmask
  pir_config.pull_up_en = GPIO_PULLUP_DISABLE; 	
  pir_config.pull_down_en = GPIO_PULLDOWN_ENABLE; 
  gpio_config(&pir_config);

  //create a queue to handle PIR intr from isr //must be separate queue from button interrupt, 
  //because separate task are called; otherwise if button taksk
  //could get PIR-interrupt out of the queue, without re-enabling the PIR interrupt.
  gpio_evt_PIR_queue = xQueueCreate(1, sizeof(uint32_t));
  xTaskCreate(&task_handle_PIR_interrupt, "task_handle_PIR_interrupt", 8000, NULL, 3, NULL);

  esp_err_t err = gpio_isr_handler_add(PIR_PIN, (gpio_isr_t)handlePIRInterrupt, NULL); //debouncing version
  if(err != ESP_OK ){
      ESP_LOGE(TAG,"gpio_isr_handler_add failed:%d",err);  
    }
  ESP_LOGI(TAG, "PIR Interrupt configured\n");
}

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));
    
    wifi_config_t soft_ap_wifi_config = {};
    strcpy((char *) soft_ap_wifi_config.ap.ssid, ESP_WIFI_SOFTAP_SSID); //C++ does not allow conversion from cons string to unin8[32]   
    soft_ap_wifi_config.ap.ssid_len = strlen(ESP_WIFI_SOFTAP_SSID);
    soft_ap_wifi_config.ap.channel = ESP_WIFI_SOFTAP_CHANNEL;
    strcpy((char *) soft_ap_wifi_config.ap.password, ESP_WIFI_SOFTAP_PASS);
    soft_ap_wifi_config.ap.max_connection = ESP_WIFI_SOFTAP_MAX_STA_CONN;
    soft_ap_wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    if (strlen(ESP_WIFI_SOFTAP_PASS) == 0) {
        soft_ap_wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &soft_ap_wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             ESP_WIFI_SOFTAP_SSID, ESP_WIFI_SOFTAP_PASS, ESP_WIFI_SOFTAP_CHANNEL);
}
    
bool wifi_credentials_stored_in_NVS()
{
  //test whether wifi config is stored in NVS
  //when credentials are valid, they are copied in glob_wifi_config
  wifi_interface_t interface;
  // result of esp_wifi_get_config does not mean whether wifi credentials are stored in NVS or not
  // you should check the string length of the data coming from the NVS which is typically filled up with 0xff after erasing the flash.
  esp_err_t ret = esp_wifi_get_config(WIFI_IF_STA, &glob_wifi_config);
  
  //You could check if more values are set but only ssid or password is necessary to check against
  const char * const_ssid = (char *)&glob_wifi_config.sta.ssid; //Convert uint8* to char* to const char * (latest conversion cannot be casted)
  const char * const_password = (char *)&glob_wifi_config.sta.password; //Convert uint8* to char* to const char * (latest conversion cannot be casted)

  if(strlen(const_ssid) == 0 || strlen(const_password) == 0) {
    ESP_LOGI(TAG, "Wifi configuration not found in flash partition called NVS.");
    return false;
  } else {
    ESP_LOGI(TAG, "Wifi configuration stored in NVS will be used: %s, %s", const_ssid, const_password);
    return true;
  }

}

void wifi_init_sta(void)
{
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
  wifi_config_t wifi_config = {}; //will not connect otherwise 

  //    strcpy((char *) wifi_config.sta.ssid, EXAMPLE_ESP_WIFI_SSID); //C++ does not allow conversion from cons string to unin8[32]
  //    strcpy((char *) wifi_config.sta.password, EXAMPLE_ESP_WIFI_PASS);
  ESP_LOGI(TAG, "%s" ,wifi_config.sta.ssid);
  ESP_LOGI(TAG, "%s" ,wifi_config.sta.password);

    //read and use wifi credentials from NVS
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    /* Setting a password implies station will connect to all security modes including WEP/WPA.
     * However these modes are deprecated and not advisable to be used. Incase your Access point
     * doesn't support WPA2, these mode can be enabled by commenting below line */
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}


extern "C" {
   void app_main();
}


void app_main()
{
  esp_log_level_set(TAG, ESP_LOG_DEBUG);
  //@@@esp_ota_mark_app_invalid_rollback_and_reboot(); //set image invalid

  init_ota();  
  init_NVS();

  if (wifi_credentials_stored_in_NVS()) {
    wifi_init_sta(); //start wifi in STA mode, with credentials saved in NVS
    //start_http_server; with normal working uri's
  } else {
    //start softAP; and get ssid en password; they are stored in glob_wifi_config;
    wifi_init_softap();
    //start_http_server; //with one URI /control
  }


//  ESP_ERROR_CHECK(esp_netif_init());
//  ESP_ERROR_CHECK(esp_event_loop_create_default());
  wifi_has_ip = true;
  sprintf(localip, "192.168.1.165"); //@@@should not be hard coded
  //ESP_LOGI(TAG, "Connected to AP");
  /* Ensure to disable any WiFi power save mode, this allows best throughput
   * and hence timings for overall OTA operation.
  */
  esp_wifi_set_ps(WIFI_PS_NONE);

  init_udp_client();

  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  // Is time set? If not, tm_year will be (1970 - 1900).
  if (timeinfo.tm_year < (2022 - 1900)) {
    ESP_LOGD(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
    obtain_time();
    // update 'now' variable with current time
    time(&now);
  }

  char strftime_buf[64];
  // Set timezone 
  setenv("TZ", "GMT-1", 1); //GMT-1 means GMT + 1 hour!
  tzset();
  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  ESP_LOGI(TAG, "The current date/time in Amsterdam is: %s", strftime_buf);

  init_camera();  //must be executed before i2c_master_init; otherwise init_camera fails
  vTaskDelay(500/portTICK_PERIOD_MS);
// init Oled1306 display
  esp_log_level_set("SSD1306", ESP_LOG_VERBOSE);
	//i2c_master_init();
	//ssd1306_init();
  PIF *pif;
  pif = new I2C_PIF{scl, sda, 0x3c}; //BUG:baswi NUM_18 replaced by sda
  pif->info();

  SSD1306 ssd1306(pif, SSD1306_128x64);
  OLED display = OLED(ssd1306);
  display.select_font(1).clear();
  display.draw_string(0, 0, "Fam", WHITE, BLACK); 
  display.draw_string(0, display.font_height(), "aan de Wiel", WHITE, BLACK); 
  display.draw_string(0, 2*display.font_height(), "Zadkinestraat 126", WHITE, BLACK); 
  display.draw_string(90, 4*display.font_height(), CONFIG_APP_PROJECT_VER, WHITE, BLACK); 
  display.refresh();
  //vTaskDelay(100);
  
  xTaskCreate((TaskFunction_t)&task_sendAlert2Telegram, "task_sendAlert2Telegram",  8096,
          		  NULL, 5, NULL);
 
  gpio_install_isr_service(0);						//Start Interrupt Service Routine service

  //Configure button
  gpio_reset_pin(BUTTON_GPIO);
  gpio_config_t btn_config;
  btn_config.mode = GPIO_MODE_INPUT;        	//Set as Input
  btn_config.intr_type = GPIO_INTR_LOW_LEVEL; 
  btn_config.pin_bit_mask = (1 << BUTTON_GPIO); //Bitmask
  //btn_config.pin_bit_mask = ((1 << BUTTON_GPIO) | (1 << PIR_PIN)); //Bitmask
  btn_config.pull_up_en = GPIO_PULLUP_DISABLE; 	
  btn_config.pull_down_en = GPIO_PULLDOWN_ENABLE; //Enable pulldown
  gpio_config(&btn_config);
  ESP_LOGI(TAG, "Button configured\n");
  //create a queue to handle BUTTON intr from isr
  gpio_evt_button_queue = xQueueCreate(1, sizeof(uint32_t));
  xTaskCreate(&task_handle_button_interrupt, "task_handle_button_interrupt", 8000, NULL, 3, NULL);
  //Configure interrupt and add handler   
  esp_err_t err = gpio_isr_handler_add(BUTTON_GPIO, (gpio_isr_t)handleButtonInterrupt, NULL); //debouncing version
  if(err != ESP_OK ){
      ESP_LOGE(TAG,"gpio_isr_handler_add failed:%d",err);  
    }
  ESP_LOGI(TAG, "Button Interrupt configured\n");

  //configure_PIR(); //do not configure PIR interrupt, because it gives false positives
  

  startCameraServer(); //camera streaming server; framebuffer is used in sendalert2telegram, so stream URL MUST be activated (fi via browser)
//baswi replaced ps_malloc by malloc; so probably not in PSRAM, seen https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/external-ram.html
//Because in settings (SDK config editor) " make ram allocatable by malloc" this is OK
//baswi remove type cast from (uint8_t*)malloc
//  framebuffer = (uint8_t*)malloc(size_t(512 * 1024)); // buffer to store a jpg in motion
//  framebuffer = (uint8_t*)malloc(size_t(1200 * 1600)); // changed size to UXVGA, 1600x1200
  framebuffer = (uint8_t*)malloc(size_t(1600 * 1200)); // changed size to UXVGA, 1600x1200 = 1,9 MB; SKD config editor: max 16K placed in
  //internal RAM, if bigger then in SPIRAM (=PSRAM)

//  xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
  ESP_LOGI(TAG, "  End of setup()\n\n");
  //@@@esp_ota_mark_app_valid_cancel_rollback(); //set image valid

  esp_log_level_set("*", ESP_LOG_ERROR);   // set all components to ERROR level
  esp_log_level_set("*", ESP_LOG_INFO);
}


static void obtain_time(void)
{
  initialize_sntp();

  // wait for time to be set
  time_t now = 0;
  struct tm timeinfo = { 0 };
  int retry = 0;
  const int retry_count = 10;
  while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
      ESP_LOGD(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
      vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
  time(&now); //standard C, https://en.cppreference.com/w/cpp/header/ctime
  localtime_r(&now, &timeinfo); //standard C https://en.cppreference.com/w/cpp/header/ctime
}

static void initialize_sntp(void)
{
    ESP_LOGD(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
}
