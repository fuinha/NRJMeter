// **********************************************************************************
// ESP8266 NRJMeter WEB Server
// **********************************************************************************
// Creative Commons Attrib Share-Alike License
// You are free to use/extend this library but please abide with the CC-BY-SA license:
// Attribution-NonCommercial-ShareAlike 4.0 International License
// http://creativecommons.org/licenses/by-nc-sa/4.0/
//
// For any explanation about teleinfo ou use , see my blog
// http://hallard.me/category/tinfo
//
// This program works with the Wifinfo board
// see schematic here https://github.com/hallard/teleinfo/tree/master/Wifinfo
//
// Written by Charles-Henri Hallard (http://hallard.me)
//
// History : V1.00 2015-06-14 - First release
//
// All text above must be included in any redistribution.
//
// **********************************************************************************
// Include Arduino header
#include <Arduino.h>
#include <ESP8266WiFi.h>
//#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
//#include <WebSocketsServer.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <Hash.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <Wire.h>
#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>
#include <LibTeleinfo.h>
#include <FS.h>
#include <Ticker.h>

//#include <GDBStub.h>

// Global project file
#include "NRJMeter.h"
//#include "debug.h"

// Watchdog
Ticker wdt_ticker;
static unsigned long wdt_loop;

//WiFiManager wifi(0);
//ESP8266WebServer server(80);
//WebSocketsServer webSocket = WebSocketsServer(8081);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws

// State Machine for WebSocket Client;
_ws_client ws_client[MAX_WS_CLIENT]; 

// DNS server
//DNSServer dnsServer;

bool ota_blink;
//uint8_t heartbeat;
uint8_t wifi_state;
unsigned long wifi_connect_time;
bool    wifi_end_setup;

// RGB Led
#ifdef RGB_LED_PIN
  //#define RGBW_LED
  //typedef NeoPixelBus<NeoRgbwFeature, NeoEsp8266BitBang800KbpsMethod> MyPixelBus;

  // RGB LED (1 LED)
  //MyPixelBus rgb_led(RGB_LED_COUNT, RGB_LED_PIN);

  //#ifdef RGBW_LED
  //  NeoPixelBus<NeoRgbwFeature, NeoEsp8266BitBang800KbpsMethod> rgb_led(RGB_LED_COUNT, RGB_LED_PIN);
  //#else
  //  NeoPixelBus<NeoRgbFeature, NeoEsp8266BitBang800KbpsMethod> rgb_led(RGB_LED_COUNT, RGB_LED_PIN);
  //#endif

  //NeoPixelBus<NeoRgbFeature, NeoEsp8266BitBang800KbpsMethod>* rgb_led = NULL;
  NeoPixelWrapper rgb_led;

  const uint16_t AnimCount = 17 * 2 / 5 + 1; // we only need enough animations for the tail and one extra
  uint8_t  rgb_led_front_pixel;   // general purpose variable used to store pixel index
  uint8_t  rgb_led_effect_state;  // general purpose variable used to store effect state
  RgbColor rgb_led_color; // RGB Led color

  // what is stored for state is specific to the need, in this case, the colors.
  // basically what ever you need inside the animation update function
  struct MyAnimationState
  {
    RgbwColor RgbwStartingColor;
    RgbwColor RgbwEndingColor;
    RgbColor  RgbStartingColor;
    RgbColor  RgbEndingColor;
    uint8_t IndexPixel;   // general purpose variable used to store pixel index
  };

  NeoPixelAnimator animations(AnimCount); // NeoPixel animation management object
  MyAnimationState animationState[AnimCount];

  //long rgb_led_timer = 0;
#endif

uint32_t tick_emoncms;
uint32_t tick_sensors;
uint32_t tick_jeedom;
uint32_t tick_domoticz;

volatile boolean task_1_sec = false;
boolean task_emoncms  = false;
boolean task_sensors  = false;
boolean task_jeedom   = false;
boolean task_domoticz = false;
unsigned long seconds = 0;


/* ======================================================================
Function: wdt_check
Purpose : call back to check everything is fine, else reset CPU
Input   : -
Output  : - 
Comments: -
====================================================================== */
void ICACHE_RAM_ATTR wdt_check(void) {
  unsigned long t = millis();
  unsigned long last_run = abs(t - wdt_loop);

  os_printf("[osWatch] last_run:%d FreeRam:%d rssi:%d WIFI:%d\n", last_run, ESP.getFreeHeap(), WiFi.RSSI(), WiFi.status());

  if( last_run >= (WDT_RESET_TIME * 1000) ) {
    // save the hit here to eeprom or to rtc memory if needed
    // ESP.restart();  // normal reboot 
    //pinMode(0, INPUT);
    //pinMode(2, INPUT);
    //pinMode(15, INPUT);
    //ESP.reset();  // hard reset
    os_printf("[osWatch] WARNING loop Blocked! check for deadlock!\n");
  }
}

// Light off the RGB LED
#ifdef RGB_LED_PIN

/* ======================================================================
Function: LedRGBFadeAnimUpdate
Purpose : Fade in and out effect for RGB Led
Input   : -
Output  : - 
Comments: grabbed from NeoPixelBus library examples
====================================================================== */
void LedRGBFadeAnimUpdate(const AnimationParam& param)
{
  // this gets called for each animation on every time step
  // progress will start at 0.0 and end at 1.0

  // apply a exponential curve to both front and back
  float progress = NeoEase::QuadraticInOut(param.progress);

  // we use the blend function on the RgbColor to mix
  // color based on the progress given to us in the animation
  if (Is_RGBW(config.led_type)) {
    RgbwColor updatedColor = RgbwColor::LinearBlend(  
                                animationState[param.index].RgbwStartingColor, 
                                animationState[param.index].RgbwEndingColor, 
                                progress);
    // apply the color to the strip
    for (uint8_t pixel=0; pixel<config.led_num ; pixel++) 
      rgb_led.SetPixelColor(pixel, updatedColor);

  } else {
    RgbColor updatedColor = RgbColor::LinearBlend(  
                                animationState[param.index].RgbStartingColor, 
                                animationState[param.index].RgbEndingColor, 
                                progress);

    // apply the color to the strip
    for (uint8_t pixel=0; pixel<config.led_num ; pixel++) 
      rgb_led.SetPixelColor(pixel, updatedColor);
  }

}

/* ======================================================================
Function: LedRGBCyclonFadeAnimUpdate
Purpose : Cyclon Fade effect
Input   : -
Output  : - 
Comments: grabbed from NeoPixelBus library examples
====================================================================== */
void LedRGBCyclonFadeAnimUpdate(const AnimationParam& param)
{
  // this gets called for each animation on every time step, progress will start at 0.0 and end at 1.0
  // we use the blend function on the RgbColor to mix color based on the progress given to us in the animation
  if (Is_RGBW(config.led_type) ) {

    RgbwColor updatedColor = RgbwColor::LinearBlend(
              animationState[param.index].RgbwStartingColor,
              animationState[param.index].RgbwEndingColor,
              param.progress);

    // apply the color to the strip
    rgb_led.SetPixelColor(animationState[param.index].IndexPixel, updatedColor);

  } else {

    RgbColor updatedColor = RgbColor::LinearBlend(
              animationState[param.index].RgbStartingColor,
              animationState[param.index].RgbEndingColor,
              param.progress);

    // apply the color to the strip
    rgb_led.SetPixelColor(animationState[param.index].IndexPixel, updatedColor);
  }
  
}

/* ======================================================================
Function: LedRGBCyclonAnimUpdate
Purpose : Cyclon effect
Input   : -
Output  : - 
Comments: grabbed from NeoPixelBus library examples
====================================================================== */
void LedRGBCyclonAnimUpdate(const AnimationParam& param)
{
  // wait for this animation to complete,
  // we are using it as a timer of sorts
  if (param.state == AnimationState_Completed) {
    // done, time to restart this position tracking animation/timer
    animations.RestartAnimation(param.index);

    // pick the next pixel inline to start animating
    rgb_led_front_pixel = (rgb_led_front_pixel + 1) % config.led_num; // increment and wrap

    uint16_t indexAnim;
    // do we have an animation available to use to animate the next front pixel?
    // if you see skipping, then either you are going to fast or need to increase
    // the number of animation channels
    if (animations.NextAvailableAnimation(&indexAnim, 1)) {
      if (Is_RGBW(config.led_type) ) {
        animationState[indexAnim].RgbwStartingColor = rgb_led_color;
        animationState[indexAnim].RgbwEndingColor = RgbColor(0, 0, 0);
      } else {
        animationState[indexAnim].RgbStartingColor = rgb_led_color;
        animationState[indexAnim].RgbEndingColor = RgbColor(0, 0, 0);
      }
      animationState[indexAnim].IndexPixel = rgb_led_front_pixel;

      animations.StartAnimation(indexAnim, 200, LedRGBCyclonFadeAnimUpdate);
    }
  }
}

/* ======================================================================
Function: LedRGBAnimate
Purpose : Manage RGBLed Animations
Input   : Animation type
          parameter (here animation time in ms)
Output  : - 
Comments: 
====================================================================== */
void LedRGBAnimate(uint8_t anim_type, uint16_t param)
{
  if (config.config & CFG_RGB_LED) {
    if ( animations.IsAnimating() ) {
      // the normal loop just needs these two to run the active animations
      animations.UpdateAnimations();
      rgb_led.Show();
    } else {

      if ( anim_type == RGB_ANIM_FADEINOUT) {
        // no animation runnning, start some fade
        //LedRGBFadeInFadeOut(config.led_hb * 1000);

        if (rgb_led_effect_state == 0) {
          // Fade up to target color
          if (Is_RGBW(config.led_type) ) {
            animationState[0].RgbwStartingColor = rgb_led.GetPixelColor(0);
            animationState[0].RgbwEndingColor = rgb_led_color;
          } else {
            animationState[0].RgbStartingColor = rgb_led.GetPixelColor(0);
            animationState[0].RgbEndingColor = rgb_led_color;
          }
          animations.StartAnimation(0, param, LedRGBFadeAnimUpdate);
        } else if (rgb_led_effect_state == 1) {
          // fade to black
          if (Is_RGBW(config.led_type) ) {
            animationState[0].RgbwStartingColor = rgb_led.GetPixelColor(0);
            animationState[0].RgbwEndingColor = RgbColor(0);
          } else {
            animationState[0].RgbStartingColor = rgb_led.GetPixelColor(0);
            animationState[0].RgbEndingColor = RgbColor(0);
          }
          animations.StartAnimation(0, param, LedRGBFadeAnimUpdate);
        }
        // toggle to the next effect state
        rgb_led_effect_state = (rgb_led_effect_state + 1) % 2;

      } else if ( anim_type == RGB_ANIM_CYCLON) {
        // no animation runnning, start some cyclon
        // start 1st pixel
        rgb_led_front_pixel = 0;
        animations.StartAnimation(0, param / config.led_num, LedRGBCyclonAnimUpdate);
      }
    }
  } else {
    // No RGB in config, paranoïa mode, stop animation
    animations.StopAnimation(0);
  }
}

/* ======================================================================
Function: LedRGBON
Purpose : Set RGB LED strip color, but does not lit it
Input   : Hue (0..360)
Output  : - 
Comments: 
====================================================================== */
//void LedRGBON (uint8_t r, uint8_t g, uint8_t b)
void LedRGBON (uint16_t hue)
{
  // Convert to neoPixel API values
  // H (is color from 0..360) should be between 0.0 and 1.0
  // L (is brightness from 0..100) should be between 0.0 and 0.5
  RgbColor target = HslColor( hue / 360.0f, 1.0f, config.led_bright * 0.005f );

  rgb_led_color = target;
}

/* ======================================================================
Function: LedRGBOFF 
Purpose : light off the RGB LED strip
Input   : -
Output  : - 
Comments: -
====================================================================== */
//void LedOff(int led)
void LedRGBOFF(void)
{
  animationState[0].RgbwStartingColor = RgbColor(0);
  animationState[0].RgbwEndingColor = RgbColor(0);
  animationState[0].RgbStartingColor = RgbColor(0);
  animationState[0].RgbEndingColor = RgbColor(0);
  rgb_led_color = RgbColor(0);

  // clear the strip
  if (config.led_num ) {
    for (uint8_t pixel=0; pixel<config.led_num ; pixel++) {
      rgb_led.SetPixelColor(pixel, RgbColor(0));
    }
    rgb_led.Show();  
  }
}

/* ======================================================================
Function: LedRGBOFF 
Purpose : light off the RGB LED strip
Input   : -
Output  : - 
Comments: -
====================================================================== */
void LedRGBSetup( void)
{
  if (config.led_num ) {
    rgb_led.Begin( (NeoPixelType) config.led_type, config.led_num, config.led_gpio);

    // apply the rainbow color to the strip
    uint16_t step = 360 / config.led_num ;
    for (uint8_t pixel=0; pixel<config.led_num ; pixel++) {
      rgb_led.SetPixelColor(pixel, HslColor((pixel*step)/360.0f, 1.0f, 0.25f) );
    }
    rgb_led.Show();
  }
}

#endif


/* ======================================================================
Function: WiFiEvent
Purpose : callback for Wifi events connection / reconnection 
Input   : event 
Output  : -
Comments: -
====================================================================== */

void WifiEvent(WiFiEvent_t event) {

  DebugF("**** WiFi_event ");
  switch(event) {

    case WIFI_EVENT_STAMODE_CONNECTED:       DebuglnF("sta conected")     ; break;
    case WIFI_EVENT_STAMODE_DISCONNECTED:    DebuglnF("sta disconnected") ; break;
    case WIFI_EVENT_STAMODE_AUTHMODE_CHANGE: DebuglnF("sta auth change")  ; break;
    case WIFI_EVENT_STAMODE_GOT_IP:          DebuglnF("sta got ip")       ; break;
    case WIFI_EVENT_STAMODE_DHCP_TIMEOUT:    DebuglnF("sta dhcp tout")    ; break;

    case WIFI_EVENT_SOFTAPMODE_STACONNECTED: DebuglnF("ap connected")    ; break;
    case WIFI_EVENT_SOFTAPMODE_STADISCONNECTED: DebuglnF("ap disconnected") ; break;
    case WIFI_EVENT_SOFTAPMODE_PROBEREQRECVED: DebuglnF("ap req received")  ; break;
  }
}


/* ======================================================================
Function: WifiConnect
Purpose : Handle Wifi connection with configuration parameters
Input   : if true wait timeout for conection
Output  : connection status  
Comments: -
====================================================================== */
int WifiConnect(boolean with_timeout) 
{
  int ret = WiFi.status();

  // If not already connected
  if (ret != WL_CONNECTED) {
    if (config.config & CFG_WIFI) {
      DebugF("Connecting to: "); 
      Debug(config.ssid);
      Debugflush();

      // Do wa have a PSK ?
      if (*config.psk) {
        // protected network
        Debug(F(" with key '"));
        Debug(config.psk);
        Debug(F("'..."));
        Debugflush();
        WiFi.begin(config.ssid, config.psk);
      } else {
        // Open network
        Debug(F("unsecure AP"));
        Debugflush();
        WiFi.begin(config.ssid);
      }

      if (with_timeout) {
        unsigned long my_start = millis();
        wifi_connect_time = my_start;

        //delay(100);
        while ( ( ret = WiFi.status() )!=WL_CONNECTED &&  wifi_connect_time<10000 ) {
          #ifdef RGB_LED_PIN
          LedRGBON(326);
          LedRGBAnimate(RGB_ANIM_CYCLON, 750);
          #endif
          // Feed the dog
          //delay(1);
          yield(); 
          //ESP.wdtFeed();

          // Save connection duration
          wifi_connect_time = millis() - my_start;
        }

        #ifdef RGB_LED_PIN
        // stop cyclon animation
        animations.StopAnimation(0);
        LedRGBOFF();
        #endif

      }
    } else {
      DebugF("Wifi Disabled in config, not connecting"); 
      Debugflush();
    }
  }

  // connected ? 
  if (ret == WL_CONNECTED)
  {

    DebugF("connected in "); Debug(wifi_connect_time); DebuglnF("ms");
    DebugF("IP address   : "); Debugln(WiFi.localIP());
    DebugF("MAC address  : "); Debugln(WiFi.macAddress());

  // not connected ? start AP
  } else {
    wifi_connect_time = 0;
    DebuglnF("Wifi client unable to connect!");
  }

  return ret;
}

/* ======================================================================
Function: WifiHandleConn
Purpose : Handle Wifi connection / reconnection and OTA updates
Input   : setup true if we're called 1st Time from setup
Output  : c
Comments: -
====================================================================== */
int WifiHandleConn(boolean setup = false) 
{
  int ret = WiFi.status();

  if (setup) {

    //WiFi.onEvent(WifiEvent);
    WiFi.mode(WIFI_AP_STA);

    DebuglnF("========== SDK Saved parameters Start"); 
    #ifdef DEBUG_SERIAL
    WiFi.printDiag(DEBUG_SERIAL);
    #endif
    DebuglnF("========== SDK Saved parameters End"); 
    Debugflush();

    //  correct SSID
    if (*config.ssid) {
      ret = WifiConnect(true);
    } else {
      DebugF("no Wifi SSID in config, set to AP Only!"); 
      WiFi.mode(WIFI_AP);
    }

    // Check what mode we need, do we need to also Access Point ?
    // con_type = WiFi.getMode()
    // status = wifi_station_get_connect_status();
    /*
     STATION_IDLE = 0,
     STATION_CONNECTING,
     STATION_WRONG_PASSWORD,
     STATION_NO_AP_FOUND,
     STATION_CONNECT_FAIL,
     STATION_GOT_IP
     */

    // connected and not AP defined in config, remove AP
    if ( ret == WL_CONNECTED && !(config.config & CFG_AP) ) {
      WiFi.mode(WIFI_STA);
    } else {
      // Need Access point configuration 
      // SSID = hostname
      DebugF("Starting AP  : ");
      Debug(config.ap_ssid);
      Debugflush();

      // STA+AP Mode without connected to STA, autoconnec will search
      // other frequencies while trying to connect, this is causing issue
      // to AP mode, so disconnect will avoid this
      if (ret != WL_CONNECTED) {
        // Disable auto retry search channel
        WiFi.disconnect(); 
      }

      // protected network
      if (*config.ap_psk) {
        DebugF(" with key '");
        Debug(config.ap_psk);
        DebugF("'");
        WiFi.softAP(config.ap_ssid, config.ap_psk);
      // Open network
      } else {
        DebugF(" with no password");
        WiFi.softAP(config.ap_ssid);
      }

      // Setup the DNS server redirecting all the domains to the apIP 
      //dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
      //dnsServer.start(53, "*", WiFi.softAPIP());

      DebugF("\r\nIP address   : "); Debugln(WiFi.softAPIP());
      DebugF("MAC address  : "); Debugln(WiFi.softAPmacAddress());
    } 

    // Set OTA parameters
    ArduinoOTA.setPort(config.ota_port);
    ArduinoOTA.setHostname(config.host);
    ArduinoOTA.setPassword(config.ota_auth);
    ArduinoOTA.begin();

    WiFiMode_t con_type = WiFi.getMode();

    #ifdef RGB_LED_PIN
      if (config.config & CFG_RGB_LED) {
        DebugF("WIFI Mode ");
        if (con_type == WIFI_STA) {
          DebuglnF("WIFI_STA");
          LedRGBON(COLOR_GREEN);
        } else if (con_type == WIFI_AP_STA) {
          DebuglnF("WIFI_AP_STA");
          LedRGBON(COLOR_CYAN);
        } else if (con_type == WIFI_AP) {
          DebuglnF("WIFI_AP");
          LedRGBON(COLOR_MAGENTA);
        } else {
          DebuglnF("UNKNOWN");
          LedRGBON(COLOR_RED);
        }
      }
    #endif

    // just in case your sketch sucks, keep update OTA Available
    // Trust me, when coding and testing it happens, this could save
    // the need to connect FTDI to reflash
    // Usefull just after 1st connexion when called from setup() before
    // launching potentially buggy main()
    unsigned long my_start = millis();

    // 3 seconds time out
    while( millis() - my_start < 3000 ) {
     
      #ifdef RGB_LED_PIN
        // start breathing
        LedRGBAnimate( RGB_ANIM_FADEINOUT, 333); 
      #endif
      
      ArduinoOTA.handle();
      yield(); 
    }

    // Wifi is activated
    wifi_state = true;

  } // if setup

  return WiFi.status();
}

/* ======================================================================
Function: webSocketEvent
Purpose : Manage routing of websocket events
Input   : -
Output  : - 
Comments: -
====================================================================== */
void webSocketEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{
  //Handle WebSocket event
  switch(type) {
    
    case WS_EVT_DISCONNECT:
      //client disconnected
      Debugf("ws[%s][%u] Disconnect ", server->url(), client->id());

      for (uint8_t i=0; i<MAX_WS_CLIENT ; i++) {
        if (ws_client[i].id == client->id() ) {
          ws_client[i].id = 0;
          ws_client[i].state = CLIENT_NONE;
          ws_client[i].refresh = 0;
          ws_client[i].tick = 0;
          Debugf("freed[%d]\n", i);
          i=MAX_WS_CLIENT; // Exit for loop
        }
      }
    break;

    case WS_EVT_CONNECT:
      uint8_t i;
      //client connected
      Debugf("ws[%s][%u] connect ", server->url(), client->id() );
      // Search a free location to store client information
      for (i=0; i<MAX_WS_CLIENT ; i++) {
        if (ws_client[i].id == 0 ) {
          ws_client[i].id = client->id();
          ws_client[i].state = CLIENT_NONE;
          ws_client[i].refresh = 0;
          ws_client[i].tick = 0;
          Debugf("added[%d]\n", i);
          i=MAX_WS_CLIENT+1; // Exit for loop
        }
      }
      if (i>MAX_WS_CLIENT) {
        DebuglnF("not added, table is full");
      }
    break;

    case WS_EVT_ERROR: 
      //error was received from the other end
      Debugf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);    
    break;

    case WS_EVT_PONG: 
      //pong message was received (in response to a ping request maybe)
      Debugf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");  
    break;

    case WS_EVT_DATA: {
      //data packet
      AwsFrameInfo * info = (AwsFrameInfo*) arg;

      if (info->final && info->index == 0 && info->len == len) {
        //the whole message is in a single frame and we got all of it's data
        Debugf("ws[%s][#%u] %s-msg[%lu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", (uint32_t) info->len);

        // Search if it's a known client
        for (uint8_t index=0; index<MAX_WS_CLIENT ; index++) {
          if (ws_client[index].id == client->id() ) {
            String msg = "";
            char buff[3];
            int val;

            // Grab all data
            for(size_t i=0; i < info->len; i++) {
              if (info->opcode == WS_TEXT ) {
                msg += (char) data[i];
              } else {
                sprintf(buff, "%02x ", (uint8_t) data[i]);
                msg += buff ;
              }
            }
            Debugf("known[%d] '%s'\n", index, msg.c_str());
            Debugf("client #%d info state=%d, tick=%u, refresh=%u\n", 
                    client->id(), ws_client[index].state, ws_client[index].tick, ws_client[index].refresh);

            // Received text message
            if (info->opcode == WS_TEXT) {
              // Is it a command ?
              if (msg.startsWith("$")) {
                msg = msg.substring(1);
                if (msg=="system") {
                 ws_client[index].state = CLIENT_SYSTEM;
                } else if (msg=="config") {
                 DebuglnF("config");
                 ws_client[index].state = CLIENT_CONFIG;
                } else if (msg=="log") {
                 DebuglnF("log");
                 ws_client[index].state = CLIENT_LOG;
                } else if (msg=="spiffs") {
                 DebuglnF("spiffs");
                 ws_client[index].state = CLIENT_SPIFFS;
                } else if (msg.startsWith("sensors:") &&  info->len >= 9 ) {
                  val = msg.substring(8).toInt();
                  Debugf("sensor=%d",val);
                  ws_client[index].state = CLIENT_SENSORS;
                  if (val>=2 && val <=300)
                    ws_client[index].refresh = val;
                } else if (msg.startsWith("rgbb:") &&  info->len >= 6 ) {
                  val = msg.substring(5).toInt();
                  Debugf("RGB brightness=%d",val);
                  ws_client[index].state = CLIENT_SENSORS;
                  if (val>=0 && val <=100)
                    config.led_bright = val;
                }
              // It's just text, pass to command interpreter
              } else {
                handle_serial((char*) msg.c_str(), client->id() );
              }
            } else {
              //client->binary("I got your binary message");
            }
          } // if known client

          // Exit for loop
          index = MAX_WS_CLIENT;

        } // for all clients
      }
    }
    break;
  }
}

/* ======================================================================
Function: setup
Purpose : Setup I/O and other one time startup stuff
Input   : -
Output  : - 
Comments: -
====================================================================== */
void setup()
{
  char buff[32];
  boolean reset_config = true;

  #if (F_CPU == 160000000L )
    // Set CPU speed to 160MHz
    system_update_cpu_freq(160);
  #endif

  //WiFi.disconnect(false);

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  //WiFi.mode(WIFI_AP_STA);
  //WiFi.disconnect();
  //delay(1000);

  // Init the serial 1, Our Debug Serial TXD0
  // note this serial can only transmit, just 
  // enough for debugging purpose
  #ifdef DEBUG_SERIAL
    DEBUG_SERIAL.begin(115200);
    config.config = CFG_DEBUG ;
  #else
    config.config = 0;
  #endif
  Debugln(F("\r\n\r\n=============="));
  Debugf("NRJMeter V%d.%d Reset=%s\r\n", NRJMETER_VERSION_MAJOR, NRJMETER_VERSION_MINOR, ESP.getResetReason().c_str());
  Debugflush();

  // Our configuration is stored into EEPROM
  EEPROM.begin(sizeof(_Config));
  //EEPROM.begin(2048);

  DebugF("Config size="); Debug(sizeof(_Config));
  DebugF(" (emoncms=");   Debug(sizeof(_emoncms));
  DebugF("  sensors=");   Debug(sizeof(_sensors));
  DebugF("  jeedom=");    Debug(sizeof(_jeedom));
  DebugF("  domoticz=");  Debug(sizeof(_domoticz));
  DebugF("  counter=");   Debug(sizeof(_counter));
  Debugln(')');
  Debugflush();

  // Check File system init 
  if (!SPIFFS.begin())
  {
    // Serious problem
    DebuglnF("SPIFFS Mount failed");
  } else {
   
    DebuglnF("SPIFFS Mount succesfull");

    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {    
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Debugf("FS File: %s, size: %d\n", fileName.c_str(), fileSize);
    }
    DebuglnF("");
  }
  
  // Read Configuration from EEP
  if (readConfig(true)) {
      DebuglnF("Good CRC, not set!");
  } else {
    // Reset Configuration
    resetConfig();

    // save back
    saveConfig();

    // Indicate the error in global flags
    config.config |= CFG_BAD_CRC;

    DebuglnF("Reset to default");
  }

  // Init the RGB Led, and set it off
  #ifdef RGB_LED_PIN
    LedRGBSetup();
  #endif

  // start Wifi connect or soft AP
  WifiHandleConn(true);

  // OTA callbacks
  ArduinoOTA.onStart([]() { 
    LedRGBOFF();
    DebuglnF("Update Started");
    ota_blink = true;
    digitalWrite(16, 1);
    pinMode(16, OUTPUT);
  });

  ArduinoOTA.onEnd([]() { 
    pinMode(16, INPUT);
    LedRGBOFF();
    DebuglnF("Update finished restarting");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {

    digitalWrite(16, ota_blink);
    if (config.led_num > 1) {
      unsigned int percent = progress / (total / 100);
      for (unsigned int pixel=0; pixel<(100/config.led_num) * percent  ; pixel++) 
        rgb_led.SetPixelColor(pixel, HslColor(COLOR_MAGENTA,1.0f, 0.25f));
      rgb_led.Show();  
    } else {
      if (ota_blink) {
        rgb_led.SetPixelColor(0, HslColor(COLOR_MAGENTA,1.0f, 0.25f));
        rgb_led.Show();  
      } else {
        LedRGBOFF();
      }
    }
    ota_blink = !ota_blink;
    //Debugf("Progress: %u%%\n", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    pinMode(16, INPUT);
    for (unsigned int pixel=0; pixel<config.led_num ; pixel++) 
      rgb_led.SetPixelColor(pixel, HslColor(COLOR_RED, 1.0f, 0.25f));
    rgb_led.Show();  
    Debugf("Update Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) { DebuglnF("Auth Failed");}
    else if (error == OTA_BEGIN_ERROR) { DebuglnF("Begin Failed");}
    else if (error == OTA_CONNECT_ERROR) { DebuglnF("Connect Failed");}
    else if (error == OTA_RECEIVE_ERROR) { DebuglnF("Receive Failed");}
    else if (error == OTA_END_ERROR) { DebuglnF("End Failed");}
    ESP.restart(); 
  });

  //server.on("/", handleRoot);
  server.on("/config_form", handleFormConfig);
  server.on("/counter_form", handleFormCounter);
  server.on("/sensors", sensorsJSONTable);
  server.on("/system", sysJSONTable);
  server.on("/config", confJSONTable);
  server.on("/spiffs", spiffsJSONTable);
  server.on("/wifiscan", wifiScanJSON);
  server.on("/factory_reset", handleFactoryReset);
  server.on("/reset", handleReset);

  // Captive portal
  //server.on("/generate_204", handleRoot);  
  ///server.on("/fwlink", handleRoot);        //Microsoft captive portal. Maybe not needed. Might be handled by notFound handle

  //Android/Chrome OS captive portal check.
  server.on("/generate_204", [&](AsyncWebServerRequest *request) { 
    AsyncWebServerResponse * response = request->beginResponse(204, "text/plain", "");
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");
    request->send(response);
  });


  // handler for the hearbeat
  server.on("/hb", HTTP_GET, [&](AsyncWebServerRequest *request){
    AsyncWebServerResponse * response = request->beginResponse(200, "text/html", R"(OK)");
    response->addHeader("Connection", "close");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
  });


  // handler for the /update form POST (once file upload finishes)
  /*
  server.on("/update", HTTP_POST, 
    // handler once file upload finishes
    [&](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {

      // Upload started
      if(!index) {
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        WiFiUDP::stopAll();
        Debugf("Update: %s\n", filename.c_str());
        LedRGBON(COLOR_MAGENTA);
        ota_blink = true;

        //start with max available size
        if(!Update.begin(maxSketchSpace)) {
          #ifdef DEBUG_SERIAL
          Update.printError(Serial1);
          #endif
        }
      }

      // We're receiving file datas
      if (ota_blink) {
        LedRGBON(COLOR_MAGENTA);
      } else {
        LedRGBOFF();
      }
      ota_blink = !ota_blink;
      Debug(".");

      if( Update.write(data, len) != len ) {
        #ifdef DEBUG_SERIAL
        Update.printError(Serial1);
        #endif
      }


      // File uploaded
      if (final) {
        AsyncWebServerResponse * response;
        //true to set the size to the current progress
        if(Update.end(true) && !Update.hasError() ) {
          response = request->beginResponse(200, "text/plain", "OK");
          Debugf("Update Success: %u\nRebooting...\n", 0 upload.totalSize);
        } else {
          response = request->beginResponse(200, "text/plain", "FAIL");
          #ifdef DEBUG_SERIAL
          Update.printError(Serial1);
          #endif
        }
        response->addHeader("Connection", "close");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);

        LedRGBOFF();

        ESP.restart();
      }
    }
    
  );
*/


  // All other not known 
  server.onNotFound(handleNotFound);
  
  // serves all SPIFFS Web file with 24hr max-age control
  server.serveStatic("/", SPIFFS, "/index.htm", "max-age=86400");
  server.serveStatic("/", SPIFFS, "/",          "max-age=86400"); 

  // Init client state machine
  for (uint8_t i=0; i<MAX_WS_CLIENT ; i++) {
    ws_client[i].id = 0;
    ws_client[i].state = CLIENT_NONE;
    ws_client[i].refresh = 0;
    ws_client[i].tick = 0;
  }

  // attach AsyncWebSocket
  ws.onEvent(webSocketEvent);
  server.addHandler(&ws);

  // Start server
  server.begin();

  // Display configuration
  showConfig();

  Debugln(F("HTTP server started"));

  // Light off the RGB LED
  LedRGBOFF();

  i2c_init();
  i2c_scan();

  sensors_setup();
  sensors_measure(); 

  // start 1st 
  //heartbeat = config.led_hb;

  // Setup call watchdog supervision each WDT Reset time / 3
  wdt_loop = millis();
  wdt_ticker.attach_ms(((WDT_RESET_TIME / 3) * 1000), wdt_check);

  // Emoncms first sent
  if (config.emoncms.freq) 
    task_emoncms = true;

  // Let time out setup AP
  wifi_end_setup = false;
}


/* ======================================================================
Function: handle_key_led 
Purpose : Manage on board Led and Push button of NodeMCU
Input   : true to light on the led, false to light off
Output  : true if button is pressed
Comments: on NodeMCU BUILTIN_LED is GPIO16 and switch (Flash) on GPIO 0
====================================================================== */
uint8_t handle_key_led( uint8_t level)
{
  uint8_t btn=true;
  digitalWrite(BUILTIN_LED, 1);
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, !level);

  //animations.Pause();
  //noInterrupts(); // Need 100% focus on instruction timing
  // Flash is on GPIO0, so RGB LED
  //pinMode(0, INPUT);
  //btn = digitalRead(0);
  //btn = true;
  //pinMode(0, OUTPUT); // set back to output
  //interrupts();
  //animations.Resume();
  return !btn;
}


/* ======================================================================
Function: handle_net
Purpose : handle all network low level tasks
Input   : -
Output  : - 
Comments: -
====================================================================== */
void handle_net()
{
  // Do all related network stuff
  ArduinoOTA.handle();  // OTA
  //yield();

  // DNS Server in case of AP
  /*
  if (config.config & CFG_AP)  {
    dnsServer.processNextRequest();  
    yield();
  }
  */
}

/* ======================================================================
Function: loop
Purpose : infinite loop main code
Input   : -
Output  : - 
Comments: -
====================================================================== */
void loop()
{
  static unsigned long previousMillis = 0;// last time update
  unsigned long currentMillis = millis();
  uint8_t key;

  #ifdef RGB_LED_PIN
    // RGB LED Animation
    LedRGBAnimate( RGB_ANIM_FADEINOUT, config.led_hb * 100);
  #endif

  // Manage our second counter
  if ( millis()-previousMillis > 1000) {
     previousMillis = currentMillis;
     seconds++;

     task_1_sec = true ;
  }

  // Do all related network stuff
  handle_net();

  // Do all related Serial stuff
  handle_serial();

  // Only once task per loop, let system do it own task
  if (task_1_sec) { 
    //Debugf("[%5ld][0x%04X]...", seconds, config.config);
    task_1_sec = false; 
    si7021_last_seen++;
    sht1x_last_seen++;

    if (config.config&CFG_WIFI) {

     // Loop thru all connected clients
      for (uint8_t index=0; index<MAX_WS_CLIENT ; index++) {

        // Client connected ?
        if (ws_client[index].id ){
          uint8_t state = ws_client[index].state;

          if (state == CLIENT_SYSTEM)  {
            String response = "{message:\"system\", data:";
            response += sysJSONTable(NULL);
            response += "}";
            Debugf("%d ws[%u][%u] sending: %s\n", system_get_free_heap_size(), ws_client[index].id, index, response.c_str());    
            // send message to this connected client
            ws.text(ws_client[index].id, response.c_str());

          } else if (state == CLIENT_SENSORS)  {

            // Increment ticks counter
            ws_client[index].tick++;

            // Time to send data (tick ellapsed?
            if (ws_client[index].tick >= ws_client[index].refresh) {
              String response = "{message:\"sensors\", data:";
              response += sensorsJSONTable(NULL);
              response += "}";
              ws_client[index].tick=0;
              Debugf("ws[%u][%u] sending %s\n", ws_client[index].id, index, response.c_str());    

              // send message to this connected client
              ws.text(ws_client[index].id, response.c_str());
            }
          }
        } // Client connected
      } // For clients
   }
    
    // Emoncms ticker check
    if (config.emoncms.freq) 
      if ( ++tick_emoncms >= config.emoncms.freq ) 
        task_emoncms = true;
  
    // DomoticZ ticker check
    if (config.domz.freq) 
      if ( ++tick_domoticz >= config.domz.freq ) 
        task_domoticz = true;

    // Sensors ticker check
    if (config.sensors.freq) 
      if ( ++tick_sensors >= config.sensors.freq ) 
        task_sensors = true;

    // Change LED color according to sensors
    #ifdef RGB_LED_PIN
      if ((config.config & CFG_RGB_LED) && config.led_hb) {
        int16_t temp=-9999;
        int16_t hum=-9999;

        // get SHT10 values
        if (config.sensors.en_sht10 && config.config & CFG_SHT10) {
          hum = sht1x_humidity;
          temp = sht1x_temperature;
        } else  {
          // We got an aeeror, RED LED
          LedRGBON (COLOR_RED);
        }
        // get SI7021 values
        if (config.sensors.en_si7021 && config.config & CFG_SI7021) {
          hum = si7021_humidity ;
          temp = si7021_temperature;
        } else {
          // We got an aeeror, RED LED
          LedRGBON (COLOR_RED);
        }

        // We got a good value, Green else yellow ?
        if (hum!=-9999 && temp!=-9999) {
          if (  temp >= config.sensors.temp_min_warn*100 && temp <= config.sensors.temp_max_warn*100 &&
                hum  >= config.sensors.hum_min_warn*100  && hum  <= config.sensors.hum_max_warn*100) {
            LedRGBON (COLOR_GREEN);
          } else {
            LedRGBON (COLOR_YELLOW);
          }
        }
      } // if Config heatbeat
    #endif

    // Time to Stop Wifi ?
    if (seconds >= MAX_WIFI_SEC && wifi_end_setup==false) {
      DebuglnF(" === Wifi Setup Ending");
      wifi_end_setup=true;
      if ( (config.config&CFG_WIFI)==0 ) {
        if ( wifi_state==true ) {
          wifi_state = false;
          DebuglnF("Stopping Wifi ");
          //ws.disconnect();
          //yield();
          //server.close();
          //yield();
          //wifi_station_disconnect(); 
          WiFi.disconnect(); 
          //wifi_set_opmode(NULL_MODE);
          WiFi.mode(WIFI_OFF);
          WiFi.forceSleepBegin();
          delay(1); //Needed, at least in my tests WiFi doesn't power off without this for some reason
          yield();
        } else {
          DebuglnF("Wifi already disabled");
        }
      } else {
        // Config has Wifi but we're not connected
        if ( WiFi.status() != WL_CONNECTED ) {
          // No AP if config and not connected, enable back auto connect
          if ( config.config&CFG_AP==0) {
            DebuglnF("Enabling back Wifi Autoconnect with no AP");
            WiFi.mode(WIFI_STA);
            WifiConnect(false);
          } else {
            DebuglnF("Wifi not connected leaving AP mode");
          }
        } else {
          DebuglnF("Keeping Wifi ");
        }
      }
      DebuglnF(" === OK!");
    }

    //DebuglnF("OK!");
    //Debugflush();

  } else if (task_sensors) { 
    Debugf("-- [%05ld] task_sensors...", seconds);
    sensors_measure(); 
    task_sensors=false; 
    tick_sensors = 0;
    DebuglnF("-- OK!");

  } else if (task_emoncms) { 
    DebugF("task_emoncms...");
    emoncmsPost(); 
    task_emoncms=false; 
    tick_emoncms = 0;

  } else if (task_domoticz) { 
    DebugF("task_domoticz...");
    domoticzPost(); 
    task_domoticz=false; 
    tick_domoticz = 0;
  }

  // manage Blinking LED
  // No Wifi no LED
  if (!wifi_state) {
    // No Wifi led off
    key = handle_key_led(LOW);
  } else {
    if (WiFi.status()==WL_CONNECTED) {
      // Connected (Client mode OK) normal blink
      key = handle_key_led(((millis() % 1000) < 200) ? HIGH:LOW);
    } else {
      // AP Mode only or client failed quick blink
      key = handle_key_led(((millis() % 333) < 111) ? HIGH:LOW);
    }
  }

  if (key) {
    DebuglnF("Key pressed");
  }

  // Refresh our watchdog
  wdt_loop = millis();
  
}
