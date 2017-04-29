/*  =========Smart Mini Blinds ESP8266 Code==============
 *  Programmed by the Handsome and Brilliant Cooper Laone
 *  Special Thanks to pfodDesigner and Forward Computing and Control Pty. Ltd.
 */

// This code also has a connection idle timeout.  If there is no incoming data from the connected client, this code will close the connection after
//  the connection timeout time set in webpage config (default 15 secs).  The prevents half-closed connections from locking out new connections.


//Libraries that help connect to ESP8266 from App
// Download pfodESP8266BufferedClient library from http://www.forward.com.au/pfod/pfodParserLibraries/index.html
// Download pfodParser library from http://www.forward.com.au/pfod/pfodParserLibraries/index.html

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <pfodESP8266Utils.h>
#include <pfodESP8266BufferedClient.h>

#include <pfodSecurity.h>  // V2.31 or higher
int swap01(int); // method prototype for slider end swaps
pfodSecurity parser("V2"); // create a parser to handle the pfod messages
pfodESP8266BufferedClient bufferedClient;

#define DEBUG

const int portNo = 4989; // default TCP port to listen on for pfodApp connections.

// =============== start of pfodWifiWebConfig settings ==============
// update this define with the password from your QR code
//  http://www.forward.com.au/pfod/secureChallengeResponse/keyGenerator/index.html
// leave pfodWifiWebConfigPASSWORD if you want an OPEN config access point NOT RECOMMENDED as anyone can see what you set for your network's password
#define WifiWebConfigPASSWORD "Password"
// this sets the name of the Access Point you will see to configure your WiFi Network details
#define WifiWebConfigAP "WirelessBlindsAP"

// note pfodSecurity uses 19 bytes of eeprom usually starting from 0 so
// start the eeprom address from 20 for configureWifiConfig
const uint8_t webConfigEEPROMStartAddress = 20;

// add your pfod Password here for 128bit security
// eg "b0Ux9akSiwKkwCtcnjTnpWp" but generate your own key, "" means no pfod password
#define pfodSecurityCode ""
// leave pfodSecurityCode empty for no pfod security.  If No security the anyone with pfodApp can connect to the LightRemote IP and turn the light on or off.
// see http://www.forward.com.au/pfod/ArduinoWiFi_simple_pfodDevice/index.html for more information and an example
// and QR image key generator.


// the default to show in the config web page
const int DEFAULT_CONNECTION_TIMEOUT_SEC = 150;
const byte DNS_PORT = 53;
const char ROOT_IP[] = "10.1.1.1";
IPAddress apIP(pfodESP8266Utils::ipStrToNum(ROOT_IP));
DNSServer dnsServer;
ESP8266WebServer webserver(80);
bool configWebserverStarted = false; // if true process config web server
bool stopAP = false; // set to true if timed out or config complete or light switch operated
uint32_t timeout = 0;
const char *aps;
// set the EEPROM structure

struct EEPROM_storage {
  uint16_t portNo;
  char ssid[pfodESP8266Utils::MAX_SSID_LEN + 1]; // WIFI ssid + null
  char password[pfodESP8266Utils::MAX_PASSWORD_LEN + 1]; // WiFi password,  if empyt use OPEN, else use AUTO (WEP/WPA/WPA2) + null
  char staticIP[pfodESP8266Utils::MAX_STATICIP_LEN + 1]; // staticIP, if empty use DHCP + null
} storage;

const int EEPROM_storageSize = sizeof(EEPROM_storage);

char ssid_found[pfodESP8266Utils::MAX_SSID_LEN + 1]; // max 32 chars + null
void readStorage(struct EEPROM_storage * storagePtr);
void terminateStorage(struct EEPROM_storage * storagePtr);
void writeStorage(struct EEPROM_storage * storagePtr);

bool pfodStartConnection = false; // if true started connecting to WiFi network
bool wifiConnected = false; // if true connected to WiFi network and  pfod server starte

WiFiServer server(portNo);
WiFiClient client;
boolean alreadyConnected = false; // whether or not the client was connected previously

// give the board pins names, if you change the pin number here you will change the pin controlled
int cmd_A_var; // name the variable for 'Light is'  0=Off 1=On

void  processSwitch();
void processSwitchChange();
void checkConnected();

unsigned long accessPointTimerStart = 0;
unsigned const long ACCESS_POINT_TIME = 10 * 60 * 1000; // 10mins after power on

unsigned long connectionTimerStart;
unsigned long CONNECTION_TIMER_PERIOD = 500; // half sec

unsigned long CONNECTION_TIMEOUT = 45000; // 45sec
unsigned long timeoutTimerStart = 0;

void handleNotFound();
void handleRoot();
void handleConfig();
void setupAP(const char* ssid_wifi, const char* password_wifi);
void closeAP();

const int cmd_C_pin = 3; //Rx 
const int cmd_B_pin = 2; //GPIO2
const int cmd_A_pin = 0; //GPIO0

boolean waitingForOK = false;
const int MOTOR_SIGNAL_LENGTH= 11;
char MotorControl[MOTOR_SIGNAL_LENGTH];


// the setup routine runs once on reset:
void setup() {
#ifdef DEBUG
  //Serial.begin(115200, Serial_8N1, Serial_TX_ONLY); // does not work with Arduino ESP board library V2.2.0 needs V2.3.0+
  // BUT V2.3.0 has WiFi connection problems so..
  Serial.begin(115200); // works with Arduino ESP board library V2.2.0
  Serial.println();
#endif
  EEPROM.begin(512);  // only use 20bytes for pfodSecurity but reserve 512 (pfodWifiConfig uses more)

  for (int i = 10; i > 0; i--) { // wait 2.5sec because it looks good on debug
#ifdef DEBUG
    Serial.print(i);
    Serial.print(' ');
#endif
    delay(250);
  }
#ifdef DEBUG
  Serial.println();
#endif

// Assign our output pins
  pinMode(cmd_A_pin,OUTPUT);
  pinMode(cmd_B_pin,OUTPUT);
  pinMode(cmd_C_pin,OUTPUT);

  // start 10min access point timer
  accessPointTimerStart = millis();
  setupAP(WifiWebConfigAP, WifiWebConfigPASSWORD); //Starts the webserver at SSID above for user to input new SSID and Password
}


// the loop routine runs over and over again forever:
void loop() {

  if (configWebserverStarted) {     //we wait for our little webserver to get password n stuff here
    dnsServer.processNextRequest();
    webserver.handleClient();
    if ((millis() - accessPointTimerStart) > ACCESS_POINT_TIME) {
      // close access point
      stopAP = true;
    }
    if (stopAP) {
      closeAP();
    }
    return;

  } else if (!pfodStartConnection) { //We connect to WIFI here
    // start pfodServer to start
    startConnectingToWiFi();
    connectionTimerStart = millis();
    return;
  } else if (!wifiConnected) {
    checkConnected();
    if ((millis() - connectionTimerStart) > CONNECTION_TIMER_PERIOD) {
      connectionTimerStart = millis(); //reset timer
#ifdef DEBUG
      Serial.print("."); // print connection progress
#endif
    }
    return;
  }

  //else normal processing
  if (!client) { // see if a client is available
    client = server.available(); // evaluates to false if no connection
  } else {
    // have client
    if (!client.connected()) {
      if (alreadyConnected) {

        #ifdef DEBUG
                Serial.println("Client Disconnected");
        #endif

        
        // client closed so clean up
        closeConnection(parser.getPfodAppStream());
      }
    } else {
      // have connected client

             
      
      if (!alreadyConnected) {
        #ifdef DEBUG
                Serial.println("Got a Client");
        #endif
              
        parser.connect(bufferedClient.connect(&client), F(pfodSecurityCode)); // sets new io stream to read from and write to
        EEPROM.commit(); // does nothing if nothing to do
        alreadyConnected = true;
        timeoutTimerStart = millis(); // restart timeout timer on each connection
      }

      uint8_t cmd = parser.parse(); // parse incoming data from connection
      // parser returns non-zero when a pfod command is fully parsed
      if (cmd != 0) { // have parsed a complete msg { to }

             #ifdef DEBUG
                Serial.println("First Command: ");
                Serial.println(cmd);
              #endif

     
        timeoutTimerStart = millis();  // reset timer if we have incoming data
        //Get the first argument
        uint8_t* pfodArg = parser.getFirstArg(); // may point to \0 if no arguments in this msg.
        
        if(!waitingForOK){

          //Iterate through command and save each request
          for(int i = 0; i<MOTOR_SIGNAL_LENGTH; i++){
            MotorControl[i] = '\0';
          }
          
          MotorControl[0] = '{';
          MotorControl[1] = cmd;
          int lastChar = 2;
          for(int itr = 2; itr <10; itr+= 2){
            if(*pfodArg != 0){
              MotorControl[itr] = '|';
              MotorControl[itr+1] = *pfodArg;
              lastChar = itr+2;
              pfodArg = parser.getNextArg(pfodArg);
            }
          }
          MotorControl[lastChar] = '}';

         #ifdef DEBUG
                Serial.println("We Got: ");
                Serial.print(MotorControl);
         #endif

          //TESTING FOREVER LOOP
//          while(1){
//                parser.print(MotorControl);
//          }
          waitingForOK = true;
              
            //Maybe do something else
           
//          } else if ('!' == cmd) {
//            // CloseConnection command
//            //maybe sleep mode after?
//            closeConnection(parser.getPfodAppStream());
//          } else {
//            // unknown command
//            parser.print(F("{}")); // always send back a pfod msg otherwise pfodApp will disconnect.
//            
//            );
//            
//            #ifdef DEBUG
//              Serial.println("received unknown command");
//            #endif
//           }
        } else { //we need an ok
          if( '$' == cmd){ // got it right
              //Do whatever we were supposed to
              parser.print(F("$"));

              #ifdef DEBUG
                Serial.println("Showing Complete Received Command: ");
                Serial.print(MotorControl);
              #endif
                          
              sendMotorCommand();
          }
          else { //didn't get OK
              parser.print(F("!"));
          }
          waitingForOK = false;
        }
      }
    }  
  }
  // see if we should drop the connection
  //if (alreadyConnected && ((millis() - timeoutTimerStart) > CONNECTION_TIMEOUT)) {
  //  closeConnection(parser.getPfodAppStream());
  //}

}

void sendMotorCommand(){

  char cmd = MotorControl[1];
  if(MotorControl[2] != '}'){
    int firstNum = (MotorControl[3] - '0') * 10 + (MotorControl[5] - '0');
  
    if(MotorControl[6] != '}'){
      int secNum = (MotorControl[7] - '0') * 10 + (MotorControl[9] - '0');
    }
  }

  switch(cmd) {
    case 'P':
      #ifdef DEBUG
      Serial.println("Moving P motor");
      #endif
      writeMotor('P','F',5000);
      break;
    case 'T':
      #ifdef DEBUG
      Serial.println("Moving T motor");
      #endif
      
      writeMotor('T','F',5000);//Move Tilt Motor...
      break;
    case 'L':
      //SendLocation...
      break;
  } 
}

void writeMotor(char MotorSelect, char Direction, int Duration) { 
  if(MotorSelect == 'P'){
    if(Direction == 'F'){
      digitalWrite(cmd_A_pin,1);
      digitalWrite(cmd_B_pin,1);
      digitalWrite(cmd_C_pin,0);
    }
    else{
      digitalWrite(cmd_A_pin,1);
      digitalWrite(cmd_B_pin,0);
      digitalWrite(cmd_C_pin,1);
    }
  }
  else {
    if(Direction == 'F'){
      digitalWrite(cmd_A_pin,0);
      digitalWrite(cmd_B_pin,1);
      digitalWrite(cmd_C_pin,0);
    }
    else{
      digitalWrite(cmd_A_pin,0);
      digitalWrite(cmd_B_pin,0);
      digitalWrite(cmd_C_pin,1);
    }
  } 
  delay(Duration);
  
  digitalWrite(cmd_A_pin,0);
  digitalWrite(cmd_B_pin,0);
  digitalWrite(cmd_C_pin,0);
  
}

void checkConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
#ifdef DEBUG
  Serial.println();
  Serial.println("WiFi connected");
#endif
  // Start the server on configured portNo
  server = WiFiServer(storage.portNo);
  server.begin();
#ifdef DEBUG
  Serial.println("Server started");
#endif
  // Print the IP address
#ifdef DEBUG
  Serial.println(WiFi.localIP());
#endif
  // initialize client
  client = server.available(); // evaluates to false if no connection
  wifiConnected = true;
}

void startConnectingToWiFi() {
  readStorage(&storage);
#ifdef DEBUG
  Serial.println(F("Start pfodServer"));
#endif
  wifiConnected = false;
  // load the stored config
  WiFi.mode(WIFI_STA); //
  /* Initialise wifi module */
  if (*(storage.staticIP) != '\0') {
    IPAddress ip(pfodESP8266Utils::ipStrToNum(storage.staticIP));
    IPAddress gateway(ip[0], ip[1], ip[2], 1); // set gatway to ... 1
#ifdef DEBUG
    Serial.print(F("Setting ip to: "));
    Serial.println(ip);
    Serial.print(F("Setting gateway to: "));
    Serial.println(gateway);
#endif
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(ip, gateway, subnet);
  }
  WiFi.begin(storage.ssid, storage.password);
  // return here
  pfodStartConnection = true;
}


void readStorage(struct EEPROM_storage * storagePtr) {
  uint8_t * byteStorageRead = (uint8_t *)storagePtr;
  for (size_t i = 0; i < EEPROM_storageSize; i++) {
    byteStorageRead[i] = EEPROM.read(webConfigEEPROMStartAddress + i);
  }
  terminateStorage(storagePtr);
}


void terminateStorage(struct EEPROM_storage* storagePtr) {
  storagePtr->ssid[pfodESP8266Utils::MAX_SSID_LEN] = '\0';
  storagePtr->password[pfodESP8266Utils::MAX_PASSWORD_LEN] = '\0';
  storagePtr->staticIP[pfodESP8266Utils::MAX_STATICIP_LEN] = '\0';
}

void writeStorage(struct EEPROM_storage * storagePtr) {
  terminateStorage(storagePtr);
  uint8_t * byteStorage = (uint8_t *)storagePtr;
  for (size_t i = 0; i < EEPROM_storageSize; i++) {
    EEPROM.write(webConfigEEPROMStartAddress + i, byteStorage[i]);
  }
  delay(0);
  bool committed =  EEPROM.commit();
#ifdef DEBUG
  Serial.print(F("EEPROM.commit() = "));
  Serial.println(committed ? "true" : "false");
#endif
}

void closeConnection(Stream *io) {
  // add any special code here to force connection to be dropped
  parser.closeConnection(); // nulls io stream
  alreadyConnected = false;
  bufferedClient.stop(); // clears client reference
  WiFiClient::stopAll(); // cleans up memory
  client = server.available(); // evaluates to false if no connection
}

int swap01(int in) {
  return (in == 0) ? 1 : 0;
}


void closeAP() {
  dnsServer.stop();
  webserver.stop();
  for (int i = 0; i < 10; i++) { // give ESP 10 sec to completely close down AP
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    delay(1000);
  }
  
  configWebserverStarted = false;
}

void setupAP(const char* ssid_wifi, const char* password_wifi) {
  WiFi.mode(WIFI_AP); // access point wifi mode
  aps = pfodESP8266Utils::scanForStrongestAP((char*)&ssid_found, pfodESP8266Utils::MAX_SSID_LEN + 1);
  delay(0);
  IPAddress local_ip = IPAddress(10, 1, 1, 1);
  IPAddress gateway_ip = IPAddress(10, 1, 1, 1);
  IPAddress subnet_ip = IPAddress(255, 255, 255, 0);
#ifdef DEBUG
  Serial.println(F("configure WifiWebConfig"));
#endif
  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);
#ifdef DEBUG
  Serial.println(F("Access Point setup"));
#endif
  WiFi.softAPConfig(local_ip, gateway_ip, subnet_ip);

  WiFi.softAP(ssid_wifi, password_wifi);


#ifdef DEBUG
  Serial.println("done");
  IPAddress myIP = WiFi.softAPIP();
  Serial.print(F("AP IP address: "));
  Serial.println(myIP);
#endif
  delay(10);
  webserver.on ( "/", handleRoot );
  webserver.on ( "/config", handleConfig );
  webserver.onNotFound ( handleNotFound );
  webserver.begin();
#ifdef DEBUG
  Serial.println ( "HTTP webserver started" );
#endif
  configWebserverStarted = true;
}


void handleConfig() {
  // set defaults
  uint16_t portNo = 80;
  uint32_t timeout = DEFAULT_CONNECTION_TIMEOUT_SEC * 10000; // time out in 150 sec cause timeout is annoying for testing
  char tempStr[20]; // for rate,port,timeout

  if (webserver.args() > 0) {
#ifdef DEBUG
    String message = "Config results\n\n";
    message += "URI: ";
    message += webserver.uri();
    message += "\nMethod: ";
    message += ( webserver.method() == HTTP_GET ) ? "GET" : "POST";
    message += "\nArguments: ";
    message += webserver.args();
    message += "\n";
    for ( uint8_t i = 0; i < webserver.args(); i++ ) {
      message += " " + webserver.argName ( i ) + ": " + webserver.arg ( i ) + "\n";
    }
    Serial.println(message);
    Serial.println();
#endif

    uint8_t numOfArgs = webserver.args();
    const char *strPtr;
    uint8_t i = 0;
    for (; (i < numOfArgs); i++ ) {
      // check field numbers
      if (webserver.argName(i)[0] == '1') {
        pfodESP8266Utils::strncpy_safe(storage.ssid, (webserver.arg(i)).c_str(), pfodESP8266Utils::MAX_SSID_LEN);
        pfodESP8266Utils::urldecode2(storage.ssid, storage.ssid); // result is always <= source so just copy over
      } else if (webserver.argName(i)[0] == '2') {
        pfodESP8266Utils::strncpy_safe(storage.password, (webserver.arg(i)).c_str(), pfodESP8266Utils::MAX_PASSWORD_LEN);
        pfodESP8266Utils::urldecode2(storage.password, storage.password); // result is always <= source so just copy over
        // if password all blanks make it empty
        if (pfodESP8266Utils::isEmpty(storage.password)) {
          storage.password[0] = '\0';
        }
      } else if (webserver.argName(i)[0] == '3') {
        pfodESP8266Utils::strncpy_safe(storage.staticIP, (webserver.arg(i)).c_str(), pfodESP8266Utils::MAX_STATICIP_LEN);
        pfodESP8266Utils::urldecode2(storage.staticIP, storage.staticIP); // result is always <= source so just copy over
        if (pfodESP8266Utils::isEmpty(storage.staticIP)) {
          // use dhcp
          storage.staticIP[0] = '\0';
        }
      } else if (webserver.argName(i)[0] == '4') {
        // convert portNo to uint16_6
        pfodESP8266Utils::strncpy_safe(tempStr, (webserver.arg(i)).c_str(), sizeof(tempStr));
#ifdef DEBUG
        Serial.print(F("portNoStr:"));
        Serial.println(tempStr);
#endif
        long longPort = 0;
        pfodESP8266Utils::parseLong((byte*)tempStr, &longPort);
        storage.portNo = (uint16_t)longPort;
      }
    }
#ifdef DEBUG
    Serial.println();
    printStorage(&storage);
#endif
    writeStorage(&storage);
  } // else if no args just return current settings

  delay(0);
  struct EEPROM_storage storageRead;
  readStorage(&storageRead);
#ifdef DEBUG
  Serial.println();
  printStorage(&storageRead);
#endif
  String rtnMsg = "<html>"
                  "<head>"
                  "<title>"
                  WifiWebConfigAP
                  "</title>"
                  "<meta charset=\"utf-8\" />"
                  "<meta name=viewport content=\"width=device-width, initial-scale=1\">"
                  "</head>"
                  "<body>"
                  "<h2>"
                  WifiWebConfigAP
                  "<br>WiFi Settings saved.</h2><br>The WiFi ";
  if (storageRead.password[0] == '\0') {
    rtnMsg += "open ";
  }
  rtnMsg += "network <b>";
  rtnMsg += storageRead.ssid;
  rtnMsg += "</b> will be joined in a few seconds";

  if (storageRead.staticIP[0] == '\0') {
    rtnMsg += "<br> using DCHP to get the IP address";
  } else { // staticIP
    rtnMsg += "<br> using IP addess ";
    rtnMsg += "<b>";
    rtnMsg += storageRead.staticIP;
    rtnMsg += "</b>";
  }
  rtnMsg += "<br><br>This device will be listening for connections on port ";
  rtnMsg += storageRead.portNo;
  "</body>"
  "</html>";

  webserver.send ( 200, "text/html", rtnMsg );
  delay(2000); // give the websever time to send the result
  stopAP = true;
}

void handleRoot() {
  String msg;
  msg = "<html>"
        "<head>"
        "<title>"
        WifiWebConfigAP
        "</title>"
        "<meta charset=\"utf-8\" />"
        "<meta name=viewport content=\"width=device-width, initial-scale=1\">"
        "</head>"
        "<body>"
        "<h2>"
        WifiWebConfigAP
        "</h2>"
        "<p>Use this form to configure your Wifi network setting and the ip and port you want Wireless Window Blinds to use when connecting.</p>"
        "<form class=\"form\" method=\"post\" action=\"/config\" >"
        "<p class=\"name\">"
        "<label for=\"name\">Your Wifi network SSID</label><br>"
        "<input type=\"text\" name=\"1\" id=\"ssid\" placeholder=\"wifi network name\"  required "; // field 1

  if (*aps != '\0') {
    msg += " value=\"";
    msg += aps;
    msg += "\" ";
  }
  msg += " />"
         "<p class=\"password\">"
         "<label for=\"password\">Password for your WiFi network <br>(enter a space if there is no password)</label><br>"
         "<input type=\"text\" name=\"2\" id=\"password\" placeholder=\"wifi network password\" autocomplete=\"off\" required " // field 2
         "</p>"
    //Won't Need from here
         "<p class=\"static_ip\">"
         "<label for=\"static_ip\">Set the Static IP for this device, recommended<br>"
         "(If this field is empty, DHCP will be used to get an IP address)</label><br>"
         "<input type=\"text\" name=\"3\" id=\"static_ip\" placeholder=\"10.1.1.99\" "  // field 3
         " pattern=\"\\b(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\b\"/>"
         "</p>"
         "<p class=\"portNo\">"
         "<label for=\"portNo\">Set the port number.</label><br>"
         "<input type=\"text\" name=\"4\" id=\"portNo\" placeholder=\"80\" required"  // field 4
         " value=\"4989\""
         " pattern=\"\\b([0-9]{1,4}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])\\b\" />"
         "</p>"
    //To here
         "<p class=\"submit\">"
         "<input type=\"submit\" value=\"Configure\"  />"
         "</p>"
         "<h1><p>&nbsp;</p><p>&nbsp;</p><p>&nbsp;</p><p>&nbsp;</p><p>&nbsp;</p><p>&nbsp;</p><p>&nbsp;</p><p>&nbsp;</p><p>&nbsp;</p></h1>"
         "</form>"
         "</body>"
         "</html>";

  webserver.send ( 200, "text/html", msg );
}


void handleNotFound() {
  handleRoot();
}

void printStorage(struct EEPROM_storage* storagePtr) {
#ifdef DEBUG
  Serial.print("portNo:");
  Serial.println(storagePtr->portNo);
  Serial.print("ssid:");
  Serial.println(storagePtr->ssid);
  Serial.print("password:");
  Serial.println(storagePtr->password);
  Serial.print("staticIP:");
  Serial.println(storagePtr->staticIP);
#endif
}


  
