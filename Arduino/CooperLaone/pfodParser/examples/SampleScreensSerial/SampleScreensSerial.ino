/*
  Sample pfod Screens for Arduino Uno using Serial to connect to UART BT/BLE/Wifi shield at 9600
*/
/* Code (c)2016 Forward Computing and Control Pty. Ltd.
   NSW Australia, www.forward.com.au
   This generated code may be freely used for both private and commercial use
*/
// Using Serial and 9600 baud for send and receive
// Serial D0 (RX) and D1 (TX) on Arduino Uno, Micro, ProMicro, Due, Mega, Mini, Nano, Pro and Ethernet
// This code uses Serial so remove shield when programming the board
int swap01(int); // method prototype for slider end swaps
// ======================
// this is the pfodParser.h V2 file with the class renamed pfodParser_codeGenerated and with comments, constants and un-used methods removed
class pfodParser_codeGenerated: public Print { 
  public:
    pfodParser_codeGenerated(const char* version); pfodParser_codeGenerated(); void connect(Stream* ioPtr); void closeConnection(); byte parse(); bool isRefresh(); 
    const char* getVersion(); void setVersion(const char* version); void sendVersion(); byte* getCmd(); byte* getFirstArg();
    byte* getNextArg(byte *start); byte getArgsCount(); byte* parseLong(byte* idxPtr, long *result); byte getParserState();
    void setCmd(byte cmd); void setDebugStream(Print* debugOut); size_t write(uint8_t c); int available(); int read();
    int peek(); void flush(); void setIdleTimeout(unsigned long timeout); Stream* getPfodAppStream(); void init(); byte parse(byte in); 
  private:
    Stream* io; byte emptyVersion[1] = {0}; byte argsCount; byte argsIdx; byte parserState; byte args[255 + 1]; byte *versionStart;
    byte *cmdStart; bool refresh; const char *version;
};
//============= end of pfodParser_codeGenerated.h
pfodParser_codeGenerated parser("SS_V26"); // create a parser to handle the pfod messages

const int maxTextChars = 11;
byte currentText[maxTextChars + 1]  = "Hello"; // allow max 11 chars + null == 12, note  msg {'x`11~Example Text Input screen.\n"  enforces max 11 bytes in return value

const int imagesize = 50;  // this set the image size
bool sendCleanImage = false; // this is set when next image load/update should send blank image

int currentSingleSelection = 1;

const int MAX_MULTI_SELECTIONS = 5; //max 5 possible selections out of all possible,
long multiSelections[MAX_MULTI_SELECTIONS]; // -1 means not selected

int redValue = 127;
int greenValue = 127;
int blueValue = 127;
int fanPosition = 0;

const unsigned long RAW_DATA_INTERVAL = 2000; // 2 sec
unsigned long rawDataTimer;
int counter = 0;

// the setup routine runs once when you press reset:
void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  // allow a little time to connect the serialMonitor before running the rest of the setup.
  for (int i = 5; i > 0; i--) {
    delay(1000);
    // Serial.print(F(" "));
    // Serial.print(i);
  }
  //Serial.println();
  parser.connect(&Serial); // connect the parser to the i/o stream

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
  if (cmd != 0) {  // have parsed a complete msg { to }
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
    // note DO NOT call Serial print directly as it will interfer with the msg hash on secure connections
    parser.print(rawDataTimer / 1000); // the time in seconds, first field
    parser.print(','); // field separator
    parser.print(counter++); // second field
    parser.print(','); // field separator
    parser.print(analogRead(A0)); // third field
    parser.println(); // terminate data record
    rawDataTimer = millis(); // allow it to drift due to delays in sending data
  }

  // else keep looping
}

void closeConnection(Stream *io) {
  // add any special code here to force connection to be dropped
}

void sendMainMenu() {
  // put main msg in input array
  parser.print(F("{,~Serial Sample Screens"));
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
                 "counter and ADC A0 versus time" // plot title
                 "|Time (sec)|counter|A0}")); // show data in a plot
  // these match the 3 fields in the raw data record
}

//=========================================================================
/* You can remove from here on if you have the pfodParser V2 library installed from
    http://www.forward.com.au/pfod/pfodParserLibraries/index.html
  * and add 
 #include <pfodEEPROM.h
 #include <pfodParser.h>
  * at the top of this file
  * and replace the line
 pfodParser_codeGenerated parser("V1"); // create a parser to handle the pfod messages
  * with
 pfodParser parser("V1");
*/
// this is the pfodParser.cpp V2 file with the class renamed pfodParser_codeGenerated and with comments, constants and un-used methods removed
pfodParser_codeGenerated::pfodParser_codeGenerated() { pfodParser_codeGenerated(""); }
pfodParser_codeGenerated::pfodParser_codeGenerated(const char *_version) { setVersion(_version); io = NULL; init(); }
void pfodParser_codeGenerated::init() {
  argsCount = 0; argsIdx = 0; args[0] = 0; args[1] = 0; cmdStart = args; versionStart = emptyVersion; parserState = ((byte)0xff); refresh = false;
}
void pfodParser_codeGenerated::connect(Stream* ioPtr) { init(); io = ioPtr; }
void pfodParser_codeGenerated::closeConnection() { init(); }
Stream* pfodParser_codeGenerated::getPfodAppStream() { return io; }
size_t pfodParser_codeGenerated::write(uint8_t c) {
  if (!io) {
    return 1;
  }
  return io->write(c);
}
int pfodParser_codeGenerated::available() { return 0; }
int pfodParser_codeGenerated::read() { return 0; }
int pfodParser_codeGenerated::peek() { return 0; }
void pfodParser_codeGenerated::flush() {
  if (!io) { return; }
  // nothing here for now
}
void pfodParser_codeGenerated::setIdleTimeout(unsigned long timeout) { }
void pfodParser_codeGenerated::setCmd(byte cmd) { init(); args[0] = cmd; args[1] = 0; cmdStart = args; versionStart = emptyVersion; }
byte* pfodParser_codeGenerated::getCmd() { return cmdStart; }
bool pfodParser_codeGenerated::isRefresh() { return refresh; } 
const char* pfodParser_codeGenerated::getVersion() { return version; }
void pfodParser_codeGenerated::setVersion(const char* _version) { version = _version; }
void pfodParser_codeGenerated::sendVersion() { print('~'); print(getVersion()); }
byte* pfodParser_codeGenerated::getFirstArg() {
  byte* idxPtr = cmdStart;
  while (*idxPtr != 0) { ++idxPtr; }
  if (argsCount > 0) { ++idxPtr; }
  return idxPtr;
}
byte* pfodParser_codeGenerated::getNextArg(byte *start) {
  byte* idxPtr = start;
  while ( *idxPtr != 0) { ++idxPtr; }
  if (idxPtr != start) { ++idxPtr; }
  return idxPtr;
}
byte pfodParser_codeGenerated::getArgsCount() { return argsCount; }
byte pfodParser_codeGenerated::getParserState() {
  if ((parserState == ((byte)0xff)) || (parserState == ((byte)'{')) || (parserState == 0) || (parserState == ((byte)'}')) ) {
    return parserState;
  } 
  return 0;
}
byte pfodParser_codeGenerated::parse() {
  byte rtn = 0;
  if (!io) { return rtn; }
  while (io->available()) {
    int in = io->read(); rtn = parse((byte)in);
    if (rtn != 0) {
      if (rtn == '!') { closeConnection(); }
      return rtn;
    }
  }
  return rtn;
}
byte pfodParser_codeGenerated::parse(byte in) {
  if (in == ((byte)0xff)) {
    // note 0xFF is not a valid utf-8 char
    // but is returned by underlying stream if start or end of connection
    // NOTE: Stream.read() is wrapped in while(Serial.available()) so should not get this
    // unless explicitlly added to stream buffer
    init(); // clean out last partial msg if any
    return 0;
  }
  if ((parserState == ((byte)0xff)) || (parserState == ((byte)'}'))) {
    parserState = ((byte)0xff);
    if (in == ((byte)'{')) { init(); parserState = ((byte)'{'); }
    return 0;
  }
  if ((argsIdx >= (255 - 2)) && (in != ((byte)'}'))) { init(); return 0; }
  if (parserState == ((byte)'{')) {
    parserState = 0;
    if (in == ((byte)':')) {
      refresh = true; return 0; 
    }
  }
  if ((in == ((byte)':')) && (versionStart != args)) {
    args[argsIdx++] = 0;
    versionStart = args; cmdStart = args+argsIdx; refresh = (strcmp((const char*)versionStart,version) == 0);
    return 0; 
  }
  if ((in == ((byte)'}')) || (in == ((byte)'|')) || (in == ((byte)'~')) || (in == ((byte)'`'))) {
    args[argsIdx++] = 0;
    if (parserState == ((byte)0xfe)) { argsCount++; }
    if (in == ((byte)'}')) {
      parserState = ((byte)'}'); args[argsIdx++] = 0; return cmdStart[0];
    } else {
      parserState = ((byte)0xfe);
    }
    return 0;
  }
  args[argsIdx++] = in; return 0;
}
byte* pfodParser_codeGenerated::parseLong(byte* idxPtr, long *result) {
  long rtn = 0; boolean neg = false;
  while ( *idxPtr != 0) {
    if (*idxPtr == ((byte)'-')) {
      neg = true;
    } else {
      rtn = (rtn << 3) + (rtn << 1); rtn = rtn +  (*idxPtr - '0');
    }
    ++idxPtr;
  }
  if (neg) { rtn = -rtn; }
  *result = rtn;
  return ++idxPtr;
}
void pfodParser_codeGenerated::setDebugStream(Print* debugOut) { }


int swap01(int in) {
  return (in==0)?1:0;
}
// ============= end generated code =========
 

