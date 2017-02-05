/*
Image Drawing input example
This code needs pfodParser V2.31 installed
and V2 of pfodApp
It uses Serial at 9600 (on an Uno) to communicate with a BT module.

 (c)2016 Forward Computing and Control Pty. Ltd.
 This code may be freely used for both private and commercial use.
 Provide this copyright is maintained.
 */

#include <EEPROM.h> 
#include <pfodParser.h>
pfodParser parser("1");  // need latest pfodParser library V2.31+
                          // set menu and image versions to "1" 
                          // if version is not set then will never ask for a refresh of image or menu as neither will be cached.
                          
const int imagesize = 50;  // this set the image size

bool sendCleanImage = false; // this is set when next image load/update should send blank image

void setup() {
  randomSeed(millis()); // set random number generator for random pixel updates
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  delay(1000);
  // allow a little time to connect the serialMonitor before running the rest of the setup.
  for (int i = 5; i > 0; i--) {
    delay(500);
  }
  parser.connect(&Serial);  // connect the parser to Bt connection
}

void loop() { // run over and over
  byte cmd = parser.parse(); // pass it to the parser
  // parser returns non-zero when a pfod command is fully parsed
  if (cmd != 0) { // have parsed a complete msg { to }
    byte* pfodFirstArg = parser.getFirstArg(); // may point to \0 if no arguments in this msg.
    long pfodLongRtn; // used for parsing long return arguments, if any
    if ('.' == cmd) {
      // pfodApp has connected and sent {.} , it is asking for the main menu
      // send back the menu designed
      if (parser.isRefresh()) {
        sendMainMenuRefresh(); // send refresh only for image row Cmd w
      } else {
        // send whole menu
        sendMainMenu();
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
        sendImageUpdate();
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
      sendMainMenuRefresh(); //force image load now

    } else if ('!' == cmd) {
      // CloseConnection command
      closeConnection(parser.getPfodAppStream());
    } else {
      // unknown command
      parser.print(F("{}")); // always send back a pfod msg otherwise pfodApp will disconnect.
    }
  }
}

void closeConnection(Stream *io) {
  // add any special code here to force connection to be dropped
}

void sendImageDwg() { // {+`50`50~1}
  parser.print(F("{+`"));
  parser.print(imagesize);
  parser.print('`');
  parser.print(imagesize);
  parser.sendVersion(); // send ~version. Still ok to do this even if there is no version specified.
  parser.print(F("}")); 
}

void sendImageUpdate() { // {+|g`5`3|... |}
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

void sendMainMenu() {
  parser.print(F("{,"));  // start a Menu screen pfod message
  // send menu prompt
  parser.print(F("~<-2>Random Colours press and drag finger or click to draw on image<-2>\nScaled to 75% of screen width and \nnon-clickable scaled to 2 times image size`2000"));//
  parser.sendVersion(); // send ~version after prompt and before first menu item. Still ok to do this even if there is no version specified.
  parser.print(F("|+w<bg bl><bk>~c~0.75r")); // image 75% of screen width. For full screen use "|+w<bg w><bk>~c" or "|+w<bg w><bk>~c~r" or "|+w<bg w><bk>~c~1.0r"
  parser.print(F("|n<bg bl><w>~Clear Image")); // Button to reload empty image
  parser.print(F("|+!x<bg 000040><bk>~c~2c")); // 2 x image size as a label. This image size will remain approximately constant on different screen sizes
  parser.print(F("}"));  // close pfod message
}

void sendMainMenuRefresh() {  // {;|+w}  update the row containing the image to force reload/update of image
  parser.print(F("{;"));  // start a Menu screen pfod message
  parser.print(F("|w")); // force image update.  Since same image is used in two menu items, this one update will effect both rows.
  // no other changes
  parser.print(F("}"));  // close pfod message
}


