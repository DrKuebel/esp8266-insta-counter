
/*

  ------------------------
  NodeMCU 1.0 pinout:

  D8 - DataIn
  D7 - LOAD/CS
  D6 - CLK
*/

#include <FS.h>                    //this needs to be first, or it all crashes and burns...
#include "Arduino.h"
#include <ESP8266WiFi.h>
#include "InstagramStats.h"        // https://github.com/witnessmenow/arduino-instagram-stats
#include "JsonStreamingParser.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>


#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>  

const long interval = 3000*1000;  // alle 30 Minuten prüfen
unsigned long previousMillis = millis() - 2980*1000; 
unsigned long lastPressed = millis();

WiFiClientSecure client;

InstagramStats instaStats(client);

int follower;
int modules = 4;

// Variables will change:
int buttonPushCounter = 0;   // counter for the number of button presses
int buttonState = 1;         // current state of the button
int lastButtonState = 1;     // previous state of the button

#define ROTATE 90
#define USE_SERIAL Serial

// for NodeMCU 1.0
#define DIN_PIN 15  // D8
#define CS_PIN  13 // D7
#define CLK_PIN 12  // D6


#define TOGGLE_PIN 0 // D3

#include "max7219.h"
#include "fonts.h"


//define your default values here, if there are different values in config.json, they are overwritten.
char instagramName[40];
char matrixIntensity[5];
char maxModules[5];

// =======================================================================

//flag for saving data
bool shouldSaveConfig = true;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


void setup() {

  // Serial debugging
  Serial.begin(115200);

  // Required for instagram api
  client.setInsecure();

  Serial.println("mounting FS...");

  // Set Reset-Pin to Input Mode
  pinMode(TOGGLE_PIN, INPUT);
  

  if (SPIFFS.begin()) {

 
    
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument json(1024);
        deserializeJson(json, buf.get());
        serializeJson(json,Serial);
       
          Serial.println("\nparsed json");
          strcpy(instagramName, json["instagramName"]);
          strcpy(matrixIntensity, json["matrixIntensity"]);
          strcpy(maxModules, json["maxModules"]);
      }
    } else {
      
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  WiFiManager wifiManager;

  // Requesting Instagram and Intensity for Display
  WiFiManagerParameter custom_instagram("Instagram", "Instagram", instagramName, 40);
  WiFiManagerParameter custom_intensity("Helligkeit", "Helligkeit 1-255", matrixIntensity, 5);
  WiFiManagerParameter custom_modules("Elemente", "Anzahl Elemente 4-8", maxModules, 5);
  
  // Add params to wifiManager
  wifiManager.addParameter(&custom_instagram);
  wifiManager.addParameter(&custom_intensity);
  wifiManager.addParameter(&custom_modules);

  
  initMAX7219();
  sendCmdAll(CMD_SHUTDOWN,1);

   
  printStringWithShift("     Config",5);
  
  Serial.print("Connecting WiFi ");

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  Serial.println("Running WifiManager");
  
  wifiManager.autoConnect("FollowerCounter");
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(instagramName, custom_instagram.getValue());
  strcpy(matrixIntensity, custom_intensity.getValue());
  strcpy(maxModules,custom_modules.getValue());

  // modules = String(maxModules).toInt();

  String matrixIntensityString = matrixIntensity;
  sendCmdAll(CMD_INTENSITY,matrixIntensityString.toInt());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonDocument json(1024);
    json["instagramName"] = instagramName;
    json["matrixIntensity"] = matrixIntensity;
    json["maxModules"] = maxModules;
    
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    serializeJson(json, Serial);
    serializeJson(json, configFile);
     configFile.close();
    //end save
  }

 
  
  printStringWithShift("      Starte ",5);
}


void infoWlan() {

  if (WiFi.status() ==  WL_CONNECTED ) {

    // WLAN Ok
    printStringWithShift(" WIFI OK",5);

  } else {

    // Wlan Probleme
    printStringWithShift(" WIFI ER",5);
  }
}

void infoIP() {
  String localIP = WiFi.localIP().toString();
  printStringWithShift(localIP.c_str(),100);
}

void infoReset() {

     Serial.println("Format System");
    printStringWithShift("    Format",5);
    
    // Reset Wifi-Setting
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    
    // Format Flash
    SPIFFS.format();

    // Restart
    ESP.reset();
  
}

void restartX() {
   
    printStringWithShift("    Restarte ...",5);
    ESP.reset();
}

void update_started() {

  printStringWithShift("    Update ...",5);
  USE_SERIAL.println("CALLBACK:  HTTP update process started");
}

void update_finished() {
  printStringWithShift("    Done ...",5);
  USE_SERIAL.println("CALLBACK:  HTTP update process finished");
}

void update_progress(int cur, int total) {
  printStringWithShift( cur + " of " + total,5);
  USE_SERIAL.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
  printStringWithShift( " Err " + err,5);
  USE_SERIAL.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

void updateFirmware() {

     ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

    // Add optional callback notifiers
    ESPhttpUpdate.onStart(update_started);
    ESPhttpUpdate.onEnd(update_finished);
    ESPhttpUpdate.onProgress(update_progress);
    ESPhttpUpdate.onError(update_error);

    t_httpUpdate_return ret = ESPhttpUpdate.update(client, "http://server/file.bin");
    // Or:
    //t_httpUpdate_return ret = ESPhttpUpdate.update(client, "server", 80, "file.bin");

    switch (ret) {
      case HTTP_UPDATE_FAILED:
        USE_SERIAL.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        break;

      case HTTP_UPDATE_NO_UPDATES:
        USE_SERIAL.println("HTTP_UPDATE_NO_UPDATES");
        break;

      case HTTP_UPDATE_OK:
        USE_SERIAL.println("HTTP_UPDATE_OK");
        break;
    }
  }
}

//  
void loop() {

  buttonState = digitalRead(TOGGLE_PIN);
  unsigned long currentMillis = millis();

  if (buttonState != lastButtonState && currentMillis > lastPressed + 50 ) {
    
    // if the state has changed, increment the counter
    if (buttonState == LOW) {
      // if the current state is HIGH then the button went from off to on:
      buttonPushCounter++;
      lastPressed = currentMillis;
      Serial.println("push");
      printStringWithShift(".",5);
      Serial.println(buttonPushCounter);
    } else {
      // if the current state is LOW then the button went from on to off:
      Serial.println("off");
    }
  }

  // Warte 1sec nach dem letzten Tastendruck 
  if (currentMillis > lastPressed + 1000) {

      if (buttonPushCounter > 0 ) {

            Serial.print("number of button pushes: ");
            Serial.println(buttonPushCounter);

            switch (buttonPushCounter) {

                case 1: 
                  // Einmal gedrückt
                  printCurrentFollower();
                  break;
                
                case 2:
                  // Zweimal gedrückt
                  infoWlan();
                  break;

                case 3:
                  infoIP();
                break;

                case 4:
                  restartX();
                break;

                case 10:
                  infoReset();
                  break;

                default:

                  printStringWithShift("TO MANY",5);
                  break;
            }

            

      }

      buttonPushCounter = 0;
  }
  
  // save the current state as the last state, for next time through the loop
  lastButtonState = buttonState;   


  if (currentMillis - previousMillis >= interval) {
    
    previousMillis = currentMillis;
  
    Serial.println(instagramName);

    InstagramUserStats response = instaStats.getUserStats(instagramName);
    Serial.print("Number of followers: ");
    Serial.println(response.followedByCount);

    int currentCount = response.followedByCount;

    if (currentCount > 0 ) {
        follower = currentCount;
    }
    
    printCurrentFollower();
  }

}

void printCurrentFollower() {

     String instacount = String(follower);

    clr();
    refreshAll();
  
    String insta2 = "$% " + instacount ;
    printStringWithShift(insta2.c_str(),5);
}

// =======================================================================

int showChar(char ch, const uint8_t *data)
{
  int len = pgm_read_byte(data);
  int i,w = pgm_read_byte(data + 1 + ch * len);
  for (i = 0; i < w; i++)
    scr[modules*8 + i] = pgm_read_byte(data + 1 + ch * len + 1 + i);
  scr[modules*8 + i] = 0;
  return w;
}

// =======================================================================
int dualChar = 0;

unsigned char convertPolish(unsigned char _c)
{
  unsigned char c = _c;
  if(c==196 || c==197 || c==195) {
    dualChar = c;
    return 0;
  }
  if(dualChar) {
    switch(_c) {
      case 133: c = 1+'~'; break; // 'ą'
      case 135: c = 2+'~'; break; // 'ć'
      case 153: c = 3+'~'; break; // 'ę'
      case 130: c = 4+'~'; break; // 'ł'
      case 132: c = dualChar==197 ? 5+'~' : 10+'~'; break; // 'ń' and 'Ą'
      case 179: c = 6+'~'; break; // 'ó'
      case 155: c = 7+'~'; break; // 'ś'
      case 186: c = 8+'~'; break; // 'ź'
      case 188: c = 9+'~'; break; // 'ż'
      //case 132: c = 10+'~'; break; // 'Ą'
      case 134: c = 11+'~'; break; // 'Ć'
      case 152: c = 12+'~'; break; // 'Ę'
      case 129: c = 13+'~'; break; // 'Ł'
      case 131: c = 14+'~'; break; // 'Ń'
      case 147: c = 15+'~'; break; // 'Ó'
      case 154: c = 16+'~'; break; // 'Ś'
      case 185: c = 17+'~'; break; // 'Ź'
      case 187: c = 18+'~'; break; // 'Ż'
      default:  break;
    }
    dualChar = 0;
    return c;
  }    
  switch(_c) {
    case 185: c = 1+'~'; break;
    case 230: c = 2+'~'; break;
    case 234: c = 3+'~'; break;
    case 179: c = 4+'~'; break;
    case 241: c = 5+'~'; break;
    case 243: c = 6+'~'; break;
    case 156: c = 7+'~'; break;
    case 159: c = 8+'~'; break;
    case 191: c = 9+'~'; break;
    case 165: c = 10+'~'; break;
    case 198: c = 11+'~'; break;
    case 202: c = 12+'~'; break;
    case 163: c = 13+'~'; break;
    case 209: c = 14+'~'; break;
    case 211: c = 15+'~'; break;
    case 140: c = 16+'~'; break;
    case 143: c = 17+'~'; break;
    case 175: c = 18+'~'; break;
    default:  break;
  }
  return c;
}

// =======================================================================

void printCharWithShift(unsigned char c, int shiftDelay) {
  c = convertPolish(c);
  if (c < ' ' || c > MAX_CHAR) return;
  c -= 32;
  int w = showChar(c, font);

  // Insta-Char-Hack
  int offset=1;
  if (c == 4 || c == 5 ) {
    offset = 0;
 }
  
  for (int i=0; i<w+offset; i++) {
    delay(shiftDelay);
    scrollLeft();
    refreshAll();
  }
}

// =======================================================================

void printStringWithShift(const char* s, int shiftDelay){
  while (*s) {
    printCharWithShift(*s++, shiftDelay);
  }
}

// =======================================================================
unsigned int convToInt(const char *txt)
{
  unsigned int val = 0;
  for(int i=0; i<strlen(txt); i++)
    if(isdigit(txt[i])) val=val*10+(txt[i]&0xf);
  return val;
}
// =======================================================================

/////