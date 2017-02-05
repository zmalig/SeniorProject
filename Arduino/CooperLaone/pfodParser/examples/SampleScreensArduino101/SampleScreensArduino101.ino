/*
    Sample Pfod Screens for Arduino/Genuino 101 BLE Board
    using pfodApp V2 menus
*/
// Using Arduino/Genunio 101 BLE Board
// Use Arduino V1.6.8 IDE
// using Intel Curie Board library V1.0.7
/* Code (c)2016 Forward Computing and Control Pty. Ltd.
   NSW Australia, www.forward.com.au
   This generated code may be freely used for both private and commercial use
*/

#define DEBUG
#include "CurieIMU.h"
#include <CurieBLE.h>
// download the pfodParser library V2.31+ from http://www.forward.com.au/pfod/pfodParserLibraries/index.html
#include <pfodParser.h>
int swap01(int); // method prototype for slider end swaps

// =========== pfodBLESerial definitions
const char* localName = "101 BLE";  // <<<<<<  change this string to customize the adverised name of your board (max 8 chars)
class pfodBLESerial : public BLEPeripheral, public Stream {
  public:
    pfodBLESerial(); void begin(); void poll(); size_t write(uint8_t); size_t write(const uint8_t*, size_t); int read();
    int available(); void flush(); int peek(); void close(); bool isConnected();
  private:
    const static uint8_t pfodEOF[1]; const static char* pfodCloseConnection;  static const int BLE_MAX_LENGTH = 20;
    static const int BLE_RX_MAX_LENGTH = 256; static volatile size_t rxHead; static volatile size_t rxTail;
    volatile static uint8_t rxBuffer[BLE_RX_MAX_LENGTH];  volatile static bool connected;
    size_t txIdx;  uint8_t txBuffer[BLE_MAX_LENGTH]; static void connectHandler(BLECentral& central);
    static void disconnectHandler(BLECentral& central); static void receiveHandler(BLECentral& central, BLECharacteristic& rxCharacteristic);
    static void addReceiveBytes(const uint8_t* bytes, size_t len); BLEService uartService = BLEService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
    BLEDescriptor uartNameDescriptor = BLEDescriptor("2901", localName);
    BLECharacteristic rxCharacteristic = BLECharacteristic("6E400002-B5A3-F393-E0A9-E50E24DCCA9E", BLEWrite, BLE_MAX_LENGTH);
    BLEDescriptor rxNameDescriptor = BLEDescriptor("2901", "RX - (Write)");
    BLECharacteristic txCharacteristic = BLECharacteristic("6E400003-B5A3-F393-E0A9-E50E24DCCA9E", BLEIndicate, BLE_MAX_LENGTH);
    BLEDescriptor txNameDescriptor = BLEDescriptor("2901", "TX - (Indicate)");
};
volatile size_t pfodBLESerial::rxHead = 0; volatile size_t pfodBLESerial::rxTail = 0;
volatile uint8_t pfodBLESerial::rxBuffer[BLE_RX_MAX_LENGTH]; const uint8_t pfodBLESerial::pfodEOF[1] = {(uint8_t) - 1};
const char* pfodBLESerial::pfodCloseConnection = "{!}"; volatile bool pfodBLESerial::connected = false;
// =========== end pfodBLESerial definitions

pfodParser parser("101v9"); // create a parser to handle the pfod messages
pfodBLESerial bleSerial; // create a BLE serial connection

const int maxTextChars = 11;
byte currentText[maxTextChars + 1]  = "Hello"; // allow max 11 chars + null == 12, note  msg {'x`11~Example Text Input screen.\n"  enforces max 11 bytes in return value

const int imagesize = 50;  // this set the image size
bool sendCleanImage = false; // this is set when next image load/update should send blank image

int currentSingleSelection = 1;

const int MAX_MULTI_SELECTIONS = 5; //max 5 possible selections out of all possible,
long multiSelections[MAX_MULTI_SELECTIONS]; // -1 means not selected

int ax, ay, az; // acceleration divided by 16,  1024 ~ 1G
int axRaw, ayRaw, azRaw;         // raw accelerometer values

int redValue = 127;
int greenValue = 127;
int blueValue = 127;
int fanPosition = 0;
const unsigned long RAW_DATA_INTERVAL = 2000; // 2 sec
unsigned long rawDataTimer;
int counter = 0;

// the setup routine runs once on reset:
void setup() {
#ifdef DEBUG
  Serial.begin(115200);
#endif
  for (int i = 3; i > 0; i--) {
    // wait a few secs to see if we are being programmed
    delay(1000);
#ifdef DEBUG
    Serial.print(i);
    Serial.print(' ');
#endif
  }
#ifdef DEBUG
  Serial.println();
#endif
  // initialize device
  Serial.println("Initializing IMU device...");
  CurieIMU.begin();

  // Set the accelerometer range to 2G
  CurieIMU.setAccelerometerRange(2);

  // set advertised local name and service UUID
  // begin initialization
  bleSerial.begin();
  parser.connect(&bleSerial);

  // <<<<<<<<< Your extra setup code goes here
  // setup multiselections
  // just clear for now
  for (int i = 0; i < MAX_MULTI_SELECTIONS; i++) {
    multiSelections[i] = -1; // empty
  }
  rawDataTimer = millis();
}

// the loop routine runs over and over again forever:
void loop() {
  byte cmd = parser.parse(); // parse incoming data from connection
  // parser returns non-zero when a pfod command is fully parsed
  if (cmd != 0) { // have parsed a complete msg { to }
    byte* pfodFirstArg = parser.getFirstArg(); // may point to \0 if no arguments in this msg.
    long pfodLongRtn; // used for parsing long return arguments, if any
    if ('.' == cmd) {
      // pfodApp has connected and sent {.} , it is asking for the main menu
      // send back the menu designed
      if (!parser.isRefresh()) {
        sendMainMenu();
      } else {
        sendMainMenuUpdate();
      }
    } else if ('p' == cmd) {
      sendPlottingScreen(); // plots the raw data being logged
    } else if ('l' == cmd) {
      if (!parser.isRefresh()) {
        sendListMenu();
      } else {
        sendListMenuUpdate();
      }
    } else if ('s' == cmd) {
      if (!parser.isRefresh()) {
        sendSliderMenu();
      } else {
        sendSliderMenuUpdate();
      }
    } else  if ('x' == cmd) {
      // return from text input,  pickup the first arg which is the number
      strncpy((char*)currentText, (const char*)pfodFirstArg, sizeof(currentText) - 1); // keep null at end
      parser.print(F("{}")); // nothing to update pfodApp will request previous menu
    } else  if ('y' == cmd) {
      sendSingleSelectionScreen();
    } else  if ('S' == cmd) {
      // return from single selection input, pickup the first arg which is the number
      parser.parseLong(pfodFirstArg, &pfodLongRtn); // stores result in a long
      currentSingleSelection = (int)pfodLongRtn;
      parser.print(F("{}"));
    } else  if ('F' == cmd) {
      if (!parser.isRefresh()) {
        sendFanControl();
      } else {
        sendFanControlUpdate();
      }
    } else  if ('G' == cmd) {
      if (!parser.isRefresh()) {
        sendImageMenu();
      } else {
        sendImageMenuUpdate();
      }
    } else if ('c' == cmd) { // image load/update request
      if (!parser.isRefresh() || sendCleanImage) {
        // if no cached image, i.e. not a refresh
        // OR need to send a clean image
        // then draw basic image
        sendImageDwg();
        sendCleanImage = false;
      } else {
        // send updates to the existing cached image
        sendImageDwgUpdate();
      }

    } else if ('w' == cmd) { // process image touch/drag
      long row;
      long col;
      long touchType; // 0 == click, 1 == start drag, 2 == continue drag, 3 == end drag
      byte* argPtr = pfodFirstArg;
      argPtr = parser.parseLong(argPtr, &col); // get x
      argPtr = parser.parseLong(argPtr, &row); // get y
      argPtr = parser.parseLong(argPtr, &touchType); // get touchType
      parser.print(F("{+w")); // image update default colour White
      parser.print(F("|`")); // update a 3x3 square top left where user touched
      parser.print(col);
      parser.print('`');
      parser.print(row);
      parser.print(F("`3`3")); // 3 x 3 square at row,col
      parser.print(F("}"));

    } else if ('n' == cmd) { // mark next image request to send clean image
      sendCleanImage = true;
      sendImageMenuUpdate(); //force image load now

    } else  if ('o' == cmd) {
      // return from fan control, pickup the first arg which is the number
      parser.parseLong(pfodFirstArg, &pfodLongRtn);
      fanPosition = (int)pfodLongRtn;
      sendFanControlUpdate();
    } else  if ('r' == cmd) {
      // return from red colour selector, pickup the first arg which is the number
      parser.parseLong(pfodFirstArg, &pfodLongRtn);
      redValue = (int)pfodLongRtn;
      sendColourSelectorUpdate();
    } else  if ('g' == cmd) {
      // return from green colour selector, pickup the first arg which is the number
      parser.parseLong(pfodFirstArg, &pfodLongRtn);
      greenValue = (int)pfodLongRtn;
      sendColourSelectorUpdate();
    } else  if ('b' == cmd) {
      // return from blue colour selector, pickup the first arg which is the number
      parser.parseLong(pfodFirstArg, &pfodLongRtn);
      blueValue = (int)pfodLongRtn;
      sendColourSelectorUpdate();
    } else  if ('m' == cmd) {
      sendMultiSelectionScreen();
    } else  if ('M' == cmd) {
      // return from multi selection input
      byte* argIdx = pfodFirstArg; // pickup the first arg which is the number
      // will be null if no arges
      int i = 0;
      for (; (i < MAX_MULTI_SELECTIONS) && (*argIdx != 0); i++) {
        argIdx = parser.parseLong(argIdx, &multiSelections[i]);
        //argIdx will be null after last arg is parsed
      }
      if (i < MAX_MULTI_SELECTIONS) {
        // add a -1 to terminate the array of indices
        multiSelections[i] = -1;
      }
      parser.print(F("{}"));
    } else  if ('L' == cmd) {
      if (!parser.isRefresh()) {
        sendColourSelector();
      } else {
        sendColourSelectorUpdate();
      }
    } else  if ('i' == cmd) {
      sendTextInputScreen();
    } else  if ('u' == cmd) {
      sendRawDataScreen();
    } else if ('!' == cmd) {
      // CloseConnection command
      closeConnection(parser.getPfodAppStream());
    } else {
      // unknown command
      parser.print(F("{}")); // always send back a pfod msg otherwise pfodApp will disconnect.
    }
  }

  // this section will send raw data at set intervals, in CSV format
  if (millis() - rawDataTimer > RAW_DATA_INTERVAL) { // send raw data
    rawDataTimer = millis(); // allow it to drift due to delays in sending data

    // read raw accelerometer measurements from device
    CurieIMU.readAccelerometer(axRaw, ayRaw, azRaw);

    // since we are using 2G range
    // -2g maps to a raw value of -32768
    // +2g maps to a raw value of 32767

    ax = axRaw >> 4;  // divide by 16 to make 1G about 1024 counts
    ay = ayRaw >> 4;  // divide by 16 to make 1G about 1024 counts
    az = azRaw >> 4;  // divide by 16 to make 1G about 1024 counts

    int sec = rawDataTimer / 1000;
    int remainder = rawDataTimer - (sec * 1000);
    int tenths = remainder / 100;
    parser.print(sec); // the time in seconds, first field
    parser.print('.');
    parser.print(tenths);
    parser.print(','); // field separator
    parser.print(ax); // second field
    parser.print(','); // field separator
    parser.print(ay); // third field
    parser.print(','); // field separator
    parser.print(az); // fourth field
    parser.println(); // terminate data record
  }

}

void closeConnection(Stream *io) {
  // add any special code here to force connection to be dropped
  ((pfodBLESerial*)io)->close();
}
void sendMainMenu() {
  // put main msg in input array
  parser.print(F("{,~101 Sample Screens"));
  parser.sendVersion(); // send pfod menu version
  parser.print(F("|^l~Lists|^s~Sliders and Dwgs|^i~Text Input|^u~Raw Data|^p~Plots}"));
}
void sendMainMenuUpdate() {
  parser.print(F("{;}")); // nothing to update here
}

void sendSliderMenu() {
  parser.print(F("{,~Slider Examples"));
  parser.sendVersion(); // send pfod menu version
  parser.print(F("|L<bg w>~<+1><r>Colour</r> <g>Selector</g> <bl>Example</bl>"
                 "|F~<+1>Fan Control"
                 "|G~<+1>Interactive Drawing Example"
                 "}"));
}
void sendSliderMenuUpdate() {
  parser.print(F("{;}")); // nothing to update here
}

void sendListMenu() {
  parser.print(F("{,~Lists"));
  parser.sendVersion(); // send pfod menu version
  parser.print(F("|y~Single Selection|m~Multi-selection}"));
}
void sendListMenuUpdate() {
  parser.print(F("{;}")); // nothing to update here
}

void sendTextInputScreen() {
  parser.print(F("{'x`"));
  parser.print(maxTextChars);
  parser.print(F("~Text Input\n(Limited to 11 chars by Arduino code to match allocated storage)"
                 "|"));
  parser.print((char*)currentText);
  parser.print(F("}"));

}

void sendSingleSelectionScreen() {
  parser.print(F("{?S`")); // rest of msg handle in main loop
  parser.print(currentSingleSelection);
  parser.print(F("~Single Selection"
                 "|Enable|Disable}"));
}

void sendMultiSelectionScreen() {
  parser.print(F("{*M"));
  // add the current selections
  for (int i = 0; i < MAX_MULTI_SELECTIONS; i++) {
    long idx = multiSelections[i];
    if (idx < 0) {
      break;
    } // else
    parser.print(F("`"));
    parser.print(idx);
  }
  parser.print(F("~Multi-selection"
                 "|Fade on/off|3 Levels|Feature A|Feature B|Feature C}"));
}

void sendColourSelector() {
  parser.print(F("{,"));
  sendColourScreen();
  parser.print(F("}"));
}
void sendColourSelectorUpdate() {
  parser.print(F("{;"));
  parser.print(F("<bg "));  // sent the background colour in the prompt area to match the slider settings
  if (redValue < 16) {
    parser.print('0');
  }
  parser.print(redValue, HEX);
  if (greenValue < 16) {
    parser.print('0');
  }
  parser.print(greenValue, HEX);
  if (blueValue < 16) {
    parser.print('0');
  }
  parser.print(blueValue, HEX);
  parser.print(F(">"));
  parser.print(F("|r"));
  parser.print('`');
  parser.print(redValue);
  parser.print(F("|g"));
  parser.print('`');
  parser.print(greenValue);
  parser.print(F("|b"));
  parser.print('`');
  parser.print(blueValue);

  parser.print(F("}"));
}

void sendColourScreen() {
  parser.print(F("<bg "));  // sent the background colour in the prompt area to match the slider settings
  if (redValue < 16) {
    parser.print('0');
  }
  parser.print(redValue, HEX);
  if (greenValue < 16) {
    parser.print('0');
  }
  parser.print(greenValue, HEX);
  if (blueValue < 16) {
    parser.print('0');
  }
  parser.print(blueValue, HEX);
  parser.print(F(">~"));
  parser.sendVersion(); // send pfod menu version
  parser.print(F("|r<bg r>"));
  parser.print('`');
  parser.print(redValue);
  parser.print(F("~<w>Red <b>"));
  parser.print(F("`255`0~</b>")); //
  parser.print(F("|g<bg g>"));
  parser.print('`');
  parser.print(greenValue);
  parser.print(F("~<bk>Green <b>"));
  parser.print(F("`255`0~</b>")); //
  parser.print(F("|b<bg bl>"));
  parser.print('`');
  parser.print(blueValue);
  parser.print(F("~<w>Blue <b>"));
  parser.print(F("`255`0~</b>")); //
}

void sendFanControl() {
  parser.print(F("{,"));
  sendFanControlScreen();
  parser.print(F("}"));
}
void sendFanControlUpdate() {
  parser.print(F("{;"));
  parser.print('`');
  parser.print(fanPosition);
  parser.print(F("}"));
}

void sendFanControlScreen() {
  parser.print(F("~<+5>Fan Control"));
  parser.sendVersion(); // send pfod menu version
  parser.print(F("|o"));
  parser.print('`');
  parser.print(fanPosition);
  parser.print(F("~<+4><b>Fan is "));
  parser.print(F("~~Off\\Low\\High"));
}

void sendImageDwg() { // {+`50`50~1}
  parser.print(F("{+`"));
  parser.print(imagesize);
  parser.print('`');
  parser.print(imagesize);
  parser.sendVersion(); // send ~version. Still ok to do this even if there is no version specified.
  parser.print(F("}"));
}

void sendImageDwgUpdate() { // {+|g`5`3|... |}
  parser.print(F("{+"));
  for (int i = 0; i < 10; i++) {
    sendRandomPixelUpdate();
  }
  parser.print(F("}"));
}

void sendRandomPixelUpdate() { // |g`5`3
  parser.print(F("|"));
  parser.print(random(256));
  parser.print('`');
  parser.print(random(50));
  parser.print('`');
  parser.print(random(50));
}

void sendImageMenu() {
  parser.print(F("{,"));  // start a Menu screen pfod message
  // send menu prompt
  parser.print(F("~<-2>Random Colours press and drag finger or click to draw on image<-2>\nScaled to 75% of screen width and \nnon-clickable scaled to 2 times image size`2000"));//
  parser.sendVersion(); // send ~version after prompt and before first menu item. Still ok to do this even if there is no version specified.
  parser.print(F("|+w<bg bl><bk>~c~0.75r")); // image 75% of screen width. For full screen use "|+w<bg w><bk>~c" or "|+w<bg w><bk>~c~r" or "|+w<bg w><bk>~c~1.0r"
  parser.print(F("|n<bg bl><w>~Clear Image")); // Button to reload empty image
  parser.print(F("|+!x<bg 000040><bk>~c~2c")); // 2 x image size as a label. This image size will remain approximately constant on different screen sizes
  parser.print(F("}"));  // close pfod message
}

void sendImageMenuUpdate() {  // {;|+w}  update the row containing the image to force reload/update of image
  parser.print(F("{;"));  // start a Menu screen pfod message
  parser.print(F("|w")); // force image update.  Since same image is used in two menu items, this one update will effect both rows.
  // no other changes
  parser.print(F("}"));  // close pfod message
}


void sendRawDataScreen() {
  // this illustrates how you can send much more then 255 bytes as raw data.
  // The {=Raw Data Screen} just tells the pfodApp to open the rawData screen and give it a title
  // all the rest of the text (outside the { } ) is just raw data text and can be a much as you like
  // Note the raw data includes { } using { is OK as long as the next char is not a pfodApp msg identifer
  // that is the following cannot appear in rawData {@ {. {: {^ {= {' {# {?  or {*
  parser.print(F("{=Raw Data Screen}"
                 "This is the Raw Data Screen\n\n"
                 "The pfodDevice can write more the 255 chars to this screen.\n"
                 "Any bytes sent outside the pfod message { } start/end characters are"
                 " displayed here.\n"
                 " The raw data used for plotting and data logging is also displayed here.\n"
                 "\nUse the Back Button on the mobile to go back to the previous menu."
                 "\n \n"));
}

void sendPlottingScreen() {
  parser.print(F("{=" // streaming raw data screen
                 "Accel x,y,z versus time (1024 is approx 1G)" // plot title
                 "|Time (sec)|aX~1200~-1200|aY~1200~-1200|gZ~1200~-1200}")); // show data in a plot
  // these match the 3 fields in the raw data record
}
// ========== pfodBLESerial methods
pfodBLESerial::pfodBLESerial() : BLEPeripheral() {
  setLocalName(localName);  addAttribute(uartService);  addAttribute(uartNameDescriptor);  setAdvertisedServiceUuid(uartService.uuid());
  addAttribute(rxCharacteristic);  addAttribute(rxNameDescriptor);  rxCharacteristic.setEventHandler(BLEWritten, pfodBLESerial::receiveHandler);
  setEventHandler(BLEConnected, pfodBLESerial::connectHandler);  setEventHandler(BLEDisconnected, pfodBLESerial::disconnectHandler);
  addAttribute(txCharacteristic);  addAttribute(txNameDescriptor);
};

bool pfodBLESerial::isConnected() {
  return (connected && txCharacteristic.subscribed());
}
void pfodBLESerial::begin() {
  BLEPeripheral::begin();
}

void pfodBLESerial::close() {
  BLEPeripheral::disconnect();
}

void pfodBLESerial::poll() {
  BLEPeripheral::poll();
}

size_t pfodBLESerial::write(const uint8_t* bytes, size_t len) {
  for (size_t i = 0; i < len; i++) {
    write(bytes[i]);
  }
  return len; // just assume it is all written
}

size_t pfodBLESerial::write(uint8_t b) {
  BLEPeripheral::poll();
  if (!isConnected()) {
    return 1;
  }
  txBuffer[txIdx++] = b;
  if ((txIdx == sizeof(txBuffer)) || (b == ((uint8_t)'\n')) || (b == ((uint8_t)'}')) ) {
    flush(); // send this buffer if full or end of msg or rawdata newline
  }
  return 1;
}

int pfodBLESerial::read() {
  if (rxTail == rxHead) {
    return -1;
  }
  // note increment rxHead befor writing
  // so need to increment rxTail befor reading
  rxTail = (rxTail + 1) % sizeof(rxBuffer);
  uint8_t b = rxBuffer[rxTail];
  return b;
}

// called as part of parser.parse() so will poll() each loop()
int pfodBLESerial::available() {
  BLEPeripheral::poll();
  flush(); // send any pending data now. This happens at the top of each loop()
  int rtn = ((rxHead + sizeof(rxBuffer)) - rxTail ) % sizeof(rxBuffer);
  return rtn;
}

void pfodBLESerial::flush() {
  if (txIdx == 0) {
    return;
  }
  txCharacteristic.setValue(txBuffer, txIdx);
  txIdx = 0;
  BLEPeripheral::poll();
}

int pfodBLESerial::peek() {
  BLEPeripheral::poll();
  if (rxTail == rxHead) {
    return -1;
  }
  size_t nextIdx = (rxTail + 1) % sizeof(rxBuffer);
  uint8_t byte = rxBuffer[nextIdx];
  return byte;
}

void pfodBLESerial::connectHandler(BLECentral& central) {
  // clear parser with -1 incase partial message left
  // should not be one
  addReceiveBytes(pfodEOF, sizeof(pfodEOF));
  connected = true;
}

void pfodBLESerial::disconnectHandler(BLECentral& central) {
  // parser.closeConnection();
  // clear parser with -1 and insert {!} incase connection just lost
  addReceiveBytes(pfodEOF, sizeof(pfodEOF));
  addReceiveBytes((const uint8_t*)pfodCloseConnection, sizeof(pfodCloseConnection));
  connected = false;
}

void pfodBLESerial::addReceiveBytes(const uint8_t* bytes, size_t len) {
  // note increment rxHead befor writing
  // so need to increment rxTail befor reading
  for (size_t i = 0; i < len; i++) {
    rxHead = (rxHead + 1) % sizeof(rxBuffer);
    rxBuffer[rxHead] = bytes[i];
  }
}

void pfodBLESerial::receiveHandler(BLECentral& central, BLECharacteristic& rxCharacteristic) {
  size_t len = rxCharacteristic.valueLength();
  const unsigned char *data = rxCharacteristic.value();
  addReceiveBytes((const uint8_t*)data, len);
}
//======================= end pfodBLESerial methods


int swap01(int in) {
  return (in == 0) ? 1 : 0;
}
// ============= end generated code =========

