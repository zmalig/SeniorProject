/**
 * pfodESP8266_AT 
 * this code supports connections via ESP8266 using the AT command set
 *
 * Note: pfod connection timeout is reset each time either a cmd response is sent 
 * OR when raw data is sent.
 * This should keep the connection open while viewing plots.
 *
 * (c)2012 Forward Computing and Control Pty. Ltd.
 * This code may be freely used for both private and commercial use.
 * Provide this copyright is maintained.
 */
#include "pfodESP8266_AT.h"
#include "pfodParserUtils.h"
#include "pfodWaitForUtils.h"

//#define DEBUG
//#define DEBUG_SETUP
//#define DEBUG_TIMER
//#define DEBUG_WRITE
//#define DEBUG_FLUSH
//#define DEBUG_RAWDATA

pfodESP8266_AT::pfodESP8266_AT() {
  raw_io.set_pfod_Base((pfod_Base*) this);
  raw_io_ptr = &raw_io;
  espStream = NULL; // not set yet
  hardwareResetPin = -1; // not set yet
  linkConnected = false;
  espStreamReady = false;
  setIdleTimeout(getDefaultTimeOut()); // set default
}

Print* pfodESP8266_AT::getRawDataOutput() {
  return raw_io_ptr;
}

/**
 * return default time out for ESP8266 of 30sec
 */
unsigned long pfodESP8266_AT::getDefaultTimeOut() {
	return 40;  // so uses can set 30sec in pfodDesigner
}

void pfodESP8266_AT::setIdleTimeout(unsigned long timeOut) {
	if (timeOut > 28000) {
		timeOut = 28000; // 30000 returns ERROR
	}
	pfodIdleTimeout = timeOut*1000;
	idleTimeout = timeOut+10; // 10 sec longer then pfod timeout
#ifdef DEBUG
  if (debugOut != NULL) {
    debugOut->print(F("setIdleTimeout called :"));
    debugOut->print(timeOut);
    debugOut->println();
  }
#endif // DEBUG
}	
	
/**
 * Call this before calling init(..) if you want the debug output
 *
 * debugOut is not written to unless you uncomment one of the #define DEBUG_ settings above
 */
void pfodESP8266_AT::setDebugStream(Stream* out) {
  debugOut = out;
}

/**
 * Called by paser when processing ! or -1 return
 */
void pfodESP8266_AT::_closeCurrentConnection() {
#ifdef DEBUG
  if (debugOut != NULL) {
    debugOut->print(F("\n_closeCurrentConnection called, parserCleared:"));
    debugOut->print(parserCleared);
    debugOut->println();
  }
#endif // DEBUG
  if (!linkConnected) {
    parserCleared = 0;
    return;
  }

  if (parserCleared > 0) {
    parserCleared--;
    return;
  } // else real close
  // call unlink here??
  parserCleared = 0;
#ifdef DEBUG
  if (debugOut != NULL) {
    debugOut->print(F(" parserCleared == 0 close connection"));
    debugOut->print(parserCleared);
    debugOut->println();
  }
#endif // DEBUG
  disconnectId[0] = '0';
  disconnectId[1] = '\0';
  disconnect();
}

void pfodESP8266_AT::init(Stream *_espStream, const char* _ssid, const char* _pw, int _portNo, int _hardwareResetPin) {
  espStream = _espStream;
  ssid = _ssid;
  pw = _pw;
  if ((_portNo < 1) || (_portNo > 65534)) {
  	portNo = 23;
  }
  portNo = _portNo;
  hardwareResetPin = _hardwareResetPin; // -1 if not used
  init(); 
}

/**
 * Initialize the ESP8266 and call setup
 */
void pfodESP8266_AT::init() {
  foundRawDataNewline = true; // can start new rawdata line
  waitingToSend = false;
  dataSizeRemaining = 0;
  disconnectId[0] = '\0';
  ipdIdZero = false;

#ifdef DEBUG
  if (debugOut != NULL) {
    debugOut->print(F("\nNOTE: \\r \\n are replaced by [ ] for printout as in espLine:'OK[]'\n"));
  }
#endif // DEBUG
  responseSet = true; // wait for first command
  sendingResponse = false;
  sendingRawData = false;
  rxBuffer[ESP8266_RX_BUFFER_SIZE] = '\0';
  rxBuffer[0] = '\0';
  rxBufferLen = 0;
  rxBufferIdx = 0;
  rawdataTxBuffer[ESP8266_RAWDATA_TX_BUFFER_SIZE] = '\0';
  rawdataTxBufferLen = 0;
  rawdataTxBufferIdx = 0;
  disconnect();
  parserCleared = 0;
  espStreamReady = false;
  // setup pin for powerup
  // do hardware reset if available
#ifdef DEBUG_SETUP 
  unsigned long setupStartTime = millis();
#endif        
  while (1) {
    if (!ESP8266_setup()) { // starts with hardware reset
#ifdef DEBUG_SETUP
      if (debugOut != NULL) {
        debugOut->println(F("ESP8266 setup failed trying again"));
      }
#endif      
    } else {
      break;
    }
  }
#ifdef DEBUG_SETUP
      if (debugOut != NULL) {
        debugOut->print(F("\nESP8266 setup took "));
        debugOut->print((millis()-setupStartTime)/1000);
        debugOut->println(F(" sec."));
      }
#endif      
  
  espStreamReady = true;
  foundOK = true;
  okTimer = millis();
  atTimer = millis(); // reset after setup completes
}

Stream* pfodESP8266_AT::getPfodAppStream() {
  // get the command response print we are writing to
  return espStream;
}

// clears sendingResponse and responseSet also
void pfodESP8266_AT::clearTxBuffer() {
  txBuffer[ESP8266_TX_BUFFER_SIZE] = '\0';
  txBuffer[0] = '\0';
  txBufferLen = 0;
  txBufferIdx = 0;
  // sendingResponse = false; not until send finished
  responseSet = false;
}
// clears sendingResponse and responseSet also
void pfodESP8266_AT::clearRawDataTxBuffer() {
  rawdataTxBuffer[ESP8266_RAWDATA_TX_BUFFER_SIZE] = '\0';
  rawdataTxBuffer[0] = '\0';
  rawdataTxBufferLen = 0;
  rawdataTxBufferIdx = 0;
  //sendingRawData = false; not until send finished
  rawDataSet = false;
}

/**
 *  Priority for actions are
 *  Get incomming SMS msgNo_rx and then get message and delete incomming
 *    this may initiate a rawData or Alert but only if one is not waiting to be sent
 *  otherwise fills incoming stream for processing
 *  Sends of responses 0 to end  (from processing incoming stream)  drops other responses until sent.  Should not happen.
 *  Resends 0 to end   terminated by start of send of responses due to processing incoming stream
 *  send rawData or Alert if any full rawData drops other data until sent (canSendRawData == false)
 * // state controls
 * boolean expectingMessageLines; // note msg from pfodApp cannot have \r\n as are encoded
 * boolean sendingResponse; // true if processing and sending SMS response to command
 * // this takes txBuffer and encodes upto 117 bytes and sends msg
 * // continues until all of txBuffer sent
 *
 * responseSMSbuffer[0] not null => have SMS msg to send to returnConnection phno
 *
 *
 */

void pfodESP8266_AT::clearParser() {
  // put 0xff into rxBuffer before current msg
  // this will reset upstream parser like connection closed
  for (int i = ESP8266_RX_BUFFER_SIZE - 1; i > 0; i--) {
    rxBuffer[i] = rxBuffer[i - 1];
  }
  parserCleared = 0; // suppress unlink twice
  rxBuffer[0] = 0xff;
  rxBufferLen++; // add one to length
}
/**
 * must process last msg BEFORE calling this again
 */
void pfodESP8266_AT::pollBoard() {
  const char* espStreamLine = collectEspMsgs();  // not this sets global espStreamMsgIdx used below
  if (espStreamLine) {
#ifdef DEBUG
    if (debugOut != NULL) {
      // print line
      debugOut->print(F("Before processing espStreamLine:\n"));
      debugOut->print(espStreamLine);
      debugOut->println(F("----------------------"));
    }
#endif // DEBUG
    processEspLine(espStreamLine);
    // finished with line
    clearEspLine(); // clears espStreamMsgIdx
  }
  if ((millis() - okTimer) > OK_TIME) {
#ifdef DEBUG
    if (debugOut != NULL) {
      // print line
      debugOut->print(F("OK timer timed out, restart ESP8266\n"));
    }
#endif // DEBUG  	
    espStreamReady = false;  // force reset
  }

  if ((millis() - atTimer) > AT_TIME) {
    espStreamPrint("AT\r\n");
    atTimer = millis();
  }
}

const char* pfodESP8266_AT::collectEspMsgs() {
  if (dataSizeRemaining > 0) { // dataSizeRemaining is only updated in processEspLine()
    foundOK = false;
    long dataSize = dataSizeRemaining; // local copy for loop
    while ((dataSize > 0) && (espStream->available())) {
      int c = espStream->read();
      atTimer = millis();
      espStreamMsg[espStreamMsgIdx++] = c;   // writing data into array
      dataSize--;
      if (espStreamMsgIdx == MAX_UART_MSG_LEN) {
        espStreamMsg[espStreamMsgIdx] = '\0';   // terminate string
        return espStreamMsg;
      }
    }
    if (espStreamMsgIdx > 0) {
      // got all the data or nothing more avaiable now
      espStreamMsg[espStreamMsgIdx] = '\0';   // terminate string
      return espStreamMsg;
    }
    else {
      return NULL; // got nothing
    }
  }
  boolean lineEnd = false;
  while ((!lineEnd) && (espStream->available())) {
    int c = espStream->read();
    atTimer = millis();
    if (c == '\n') {
      lineEnd = true;
    }
    if (c == '\r') {
      // nothing special
    }
    espStreamMsg[espStreamMsgIdx++] = c;   // writing data into array
    // check for leading '> '
    if ((espStreamMsgIdx == 2) && (espStreamMsg[0] == '>') && (espStreamMsg[1] == ' ')) {
      lineEnd = true;
    }
    if (espStreamMsgIdx == MAX_UART_MSG_LEN) { // size 32
      lineEnd = true;
    }
  }
  // end of while either no more avail or lineEnd
  if (lineEnd) {
#ifdef DEBUG
    if (debugOut != NULL) {
      // debugOut->print(F("line end "));
      // debugOut->print(espStreamMsgIdx);
      // debugOut->println();
    }
#endif // DEBUG
    espStreamMsg[espStreamMsgIdx] = '\0';   // terminate string
    foundOK = false;
    return espStreamMsg;
  } //else {
  return NULL;
}

/**
 *  If getting incomming SMS msg then incomingMsgNo is not empty
 *
 * // may need AT timer as well?? to force OK each 30secs just in case
 */
void pfodESP8266_AT::processEspLine(const char* espLine) {
  const char *result = NULL;
#ifdef DEBUG
  if (debugOut != NULL) {
    // these msg need Serial set to 115200 or they interfer with espStream espLine
    //debugOut->print(F("expMsgLn:"));  debugOut->print(expectingMessageLines?'T':'F');
    //debugOut->println();
    debugOut->print(F("espLine:'"));
    { // block to protect debugLine var
      // replace \r with [ and \n with ]
      const char* debugLine = espLine;
      while (*debugLine != '\0') {
        if (*debugLine == '\r') {
          debugOut->print('[');
        }
        else if (*debugLine == '\n') {
          debugOut->print(']');
        }
        else {
          debugOut->print(*debugLine);
        }
        debugLine++;
      }
    }
    debugOut->print(F("'"));
    debugOut->println();
  }
#endif // DEBUG
  if (dataSizeRemaining > 0) {
    // just add this line to the data
#ifdef DEBUG
    if (debugOut != NULL) {
      debugOut->print(F("dataSizeRemaining copy data espLine len:"));
      debugOut->println(strlen(espLine));
    }
#endif // DEBUG
  if (ipdIdZero) {
    dataSizeRemaining = copyDataToRxBuffer(espLine, dataSizeRemaining); // copy starting after :
  } else {
    dataSizeRemaining = dropData(espLine, dataSizeRemaining); // copy starting after :
  	// ignore this data
  }	
#ifdef DEBUG
    if (debugOut != NULL) {
      debugOut->print(F("after copy data dataSizeRemaining:"));
      debugOut->println(dataSizeRemaining);
    }
#endif // DEBUG
  }
  else if ((result = findStr(espLine, F("Link")))) { // got connected
  }
  else if ((result = findStr(espLine, F("Unlink")))) { // got disconnected
   // disconnect();
   // disconnectId[0] = '\0'; // finished with this
#ifdef DEBUG
    	if (debugOut != NULL) {
    		debugOut->println(F("Disconnected "));
    	}
#endif // DEBUG
    }
  else if ((result = findStr(espLine, F("+IPD,")))) { // start of input data
    // find id, must be 0
    dataSizeRemaining = getDataSize(espLine);// also copies data in this line to rxBuffer
  }
  else if ((result = findStr(espLine, F("> ")))) { // F("+CMGS"))) { // reply from espStream
#ifdef DEBUG
    if (debugOut != NULL) {
      debugOut->print(F("found > -- "));
      debugOut->print(F(" responseSet:"));
      debugOut->print(responseSet ? 'T' : 'F');
      debugOut->print(F(" sendingResp:"));
      debugOut->print(sendingResponse ? 'T' : 'F');
      debugOut->print(F(" rawDataSet:"));
      debugOut->print(rawDataSet ? 'T' : 'F');
      debugOut->print(F(" sendingRawdata:"));
      debugOut->print(sendingRawData ? 'T' : 'F');
      debugOut->print(F(" strlen(const char*)espTxMsg):"));
      debugOut->print(strlen((const char*)espTxMsg));
      debugOut->print(F("\n\n"));
    }
#endif // DEBUG

    if (sendingResponse) { // sending response do this in preference to raw data
      // but may be already in the process of sendingRawdata
#ifdef DEBUG_WRITE
      if (debugOut != NULL) {
        debugOut->print(F("Found > send response :"));
        debugOut->print(espTxMsg);
        debugOut->println();
      }
#endif // DEBUG
      espStreamPrint((const char*)espTxMsg);
      if (txBufferIdx >= txBufferLen) {
#ifdef DEBUG_WRITE
        if (debugOut != NULL) {
          debugOut->print(F("finished sending response"));
          debugOut->println();
        }
#endif // DEBUG
        clearTxBuffer();
        sendingResponse = false;
      }
      // else resending or raw data
    } else if (sendingRawData) {
#ifdef DEBUG_WRITE
      if (debugOut != NULL) {
        debugOut->print(F("Found > send raw data :"));
        debugOut->print(espTxMsg);
        debugOut->println();
      }
#endif // DEBUG
      espStreamPrint((const char*)espTxMsg);
      if (rawdataTxBufferIdx >= rawdataTxBufferLen) {
#ifdef DEBUG_WRITE
        if (debugOut != NULL) {
          debugOut->print(F("finished sending rawData"));
          debugOut->println();
        }
#endif // DEBUG
        clearRawDataTxBuffer();
        sendingRawData = false;
      }
    }
    else {
      //espStreamPrint(26); // nothing to send so send nothing  this should not happen
      espStreamPrint("\r\n");
      waitingToSend = false;
      //clearTxBuffer();
      //clearRawDataTxBuffer();
    }
  }
  else if ((result = findStr(espLine, F("busy")))) {
#ifdef DEBUG
    if (debugOut != NULL) {
      debugOut->println(F("found busy do hardware reset and setup again"));
    }
#endif // DEBUG
    // send CLEAR ALL MSGS to restart
    espStreamReady = false; // next call to availble will re-init ESP8266
  }
  else if ((result = findStr(espLine, F("ERROR")))) {
#ifdef DEBUG
    if (debugOut != NULL) {
      debugOut->println(F("found ERROR do hardware reset and setup again"));
    }
#endif // DEBUG
    // send CLEAR ALL MSGS to restart
    // this happens if sending while disconnecting
    espStreamReady = false; // next call to availble will re-init ESP8266
  }
  else if ((result = findStr(espLine, F("SEND OK")))) {
#ifdef DEBUG
    if (debugOut != NULL) {
      debugOut->print(F("found SEND OK -- \n"));
    }
#endif // DEBUG
    waitingToSend = false;
    handleOK();
  }
  else if ((result = findStr(espLine, F("OK")))) {
    handleOK();
  }
}

void pfodESP8266_AT::handleOK() {
  okTimer = millis(); // reset timer
#ifdef DEBUG
  if (debugOut != NULL) {
    debugOut->print(F("found OK -- "));
    debugOut->print(F(" responseSet:"));
    debugOut->print(responseSet ? 'T' : 'F');
    debugOut->print(F(" sendingResp:"));
    debugOut->print(sendingResponse ? 'T' : 'F');
    debugOut->print(F(" rawDataSet:"));
    debugOut->print(rawDataSet ? 'T' : 'F');
    debugOut->print(F(" sendingRawdata:"));
    debugOut->print(sendingRawData ? 'T' : 'F');
    debugOut->print(F("\n\n"));
  }
#endif // DEBUG
  // !!!! -------
  // add timer to send \r and then AT\r every 10 sec or so to force found OK incase of errors
  foundOK = true;// set to false by any espStreamPrint call so have to wait for call to compete to set true again
  // can do next action if any
  if (*disconnectId != '\0') {
    // drop connection
#ifdef DEBUG
    if (debugOut != NULL) {
      debugOut->print(F("disconnectId set sending close '"));
      debugOut->print(disconnectId);
      debugOut->print(F("'"));
    }
#endif // DEBUG
    espStreamPrint(F("AT+CIPCLOSE="));
    espStreamPrint (disconnectId);
    espStreamPrint("\r\n");
    disconnectId[0] = '\0'; // finished with this
  }
  else if ((responseSet) && (!sendingRawData) && (!waitingToSend)) { //returnMsg[0]) { // set by flush() when complete response ready to go
    // if responseSet and not sendingRawData send response now
#ifdef DEBUG
    if (debugOut != NULL) {
      debugOut->print(F("responseSet true\n"));
    }
#endif // DEBUG
    if (!linkConnected) {
      clearTxBuffer();
    }
    if (txBufferIdx < txBufferLen) {
      // more to send  Note if called clearTxBuffer above then txBufferIdx == txBufferLen == 0
      int len = strncpy_safe(espTxMsg, (const char*)(txBuffer + txBufferIdx), MAX_TX_MSG_LEN); // copy at most MAX_TX_MSG_LEN
      txBufferIdx += len;
      // start sending 32 bytes
      // find remaining lenght to send
      if (len > 0) {
        sendingResponse = true;
        espStreamPrint(F("AT+CIPSEND=0,"));
        espStreamPrint(len);
        espStreamPrint(F("\r\n"));
        waitingToSend = true;
      }
      else {
        clearTxBuffer(); // should not happen
      }
    }
    else {
      // nothing more to send
      clearTxBuffer();
    }
  }
  else if (rawDataSet  && (!waitingToSend)) {
    // if rawDataSet and not responseSet above
#ifdef DEBUG
    if (debugOut != NULL) {
      debugOut->print(F("rawDataSet true\n"));
    }
#endif // DEBUG
    if (!linkConnected) {
      clearRawDataTxBuffer();
    }
    if (rawdataTxBufferIdx < rawdataTxBufferLen) {
      // more to send  Note if called clearRawDataTxBuffer above then rawdataTxBufferIdx == rawdataTxBufferLen == 0
      int len = strncpy_safe(espTxMsg, (const char*)(rawdataTxBuffer + rawdataTxBufferIdx), MAX_TX_MSG_LEN); // copy at most MAX_TX_MSG_LEN
      rawdataTxBufferIdx += len;
      // start sending 32 bytes
      // find remaining lenght to send
      if (len > 0) {
        sendingRawData = true; // the next > is for rawData not response
        espStreamPrint(F("AT+CIPSEND=0,"));
        espStreamPrint(len);
        espStreamPrint(F("\r\n"));
        waitingToSend = true;
      }
      else {
        clearRawDataTxBuffer(); // should not happen
      }
    }
    else {
      // nothing more to send
      clearRawDataTxBuffer();
    }
  }
  else {  // end of found OK
  }
}

void pfodESP8266_AT::disconnect() {
  clearEspLine();
  clearTxBuffer();
  clearRawDataTxBuffer();
  waitingToSend = false;
  sendingRawData = false;
  sendingResponse = false;
  linkConnected = false; // stop any further sends
}

// NOTE this may block the main loop for >30secs
// call by init() and if we get an ERROR return from the ESP8266
boolean pfodESP8266_AT::ESP8266_setup() {
	Stream* debugSetup = NULL;
#ifdef DEBUG_SETUP
  debugSetup = debugOut; 
#endif

  pfodWaitForUtils::dumpReply(espStream, debugSetup, 3000); // clear out any old output again
  reset();

  if (debugSetup != NULL) {
    debugSetup->print(F("\nClose any open connection\n"));
  }
  espStream->println(F("AT+CIPCLOSE=0")); // println adds \r\n required for AT cmds
  int rtn = pfodWaitForUtils::waitFor("OK", "MUX=0", espStream, debugSetup);
  if (rtn < 0) {	
    if (debugSetup != NULL) {
      debugSetup->println(F("\nAT+CIPCLOSE=0 OK or MUX=0"));
    }
    return false;
  }
  if (debugSetup != NULL) {
    debugSetup->print(F("\nDisconnect from AP"));
  }
  espStream->println(F("AT+CWQAP")); // println adds \r\n required for AT cmds
  if (!pfodWaitForUtils::waitForOK(espStream, debugSetup)) {
    if (debugSetup != NULL) {
      debugSetup->println(F("\nAT+CWQAP OK missing"));
    }
    return false;
  }
  if (debugSetup != NULL) {
    debugSetup->print(F("\nVersion Number"));
  }
  espStream->println(F("AT+GMR")); // println adds \r\n required for AT cmds
  if (!pfodWaitForUtils::waitForOK(espStream, debugSetup)) {
    if (debugSetup != NULL) {
      debugSetup->println(F("\nAT+GMR OK missing"));
    }
    return false;
  }
  // set station mode
  if (debugSetup != NULL) {
    debugSetup->print(F("\nSet Station Mode"));
  }
  // set single connection mode
  espStream->println(F("AT+CWMODE=1")); // println adds \r\n required for AT cmds
  if (!pfodWaitForUtils::waitFor("OK", "no change", espStream, debugSetup)) {
    if (debugSetup != NULL) {
      debugSetup->println(F("\nAT+CWMODE=1 OK missing"));
    }
    // try again which will do a reset first
    return false;
  }

  // need to set AT+CIPMUX=1 inorder to create a server
  espStream->println(F("AT+CIPMUX=1")); // println adds \r\n required for AT cmds
  if (!pfodWaitForUtils::waitForOK(espStream, debugSetup)) {
    if (debugSetup != NULL) {
      debugSetup->println(F("\nAT+CIPMUX=1 OK missing"));
    }
    // try again if "Link is builded" is returned
    return false;
  }

  // start server on portNo
  espStream->print(F("AT+CIPSERVER=1,"));
  espStream->println(portNo); // println adds \r\n required for AT cmds
  if (!pfodWaitForUtils::waitForOK(espStream, debugSetup)) {
    if (debugSetup != NULL) {
      debugSetup->println(F("\nAT+CIPSERVER OK missing"));
    }
    return false;
  }
  // must set this to say 30sec as dropping link from other end does not show up as Unlinked on ESP8266
  espStream->print(F("AT+CIPSTO="));  // println adds \r\n required for AT cmds was 30 
  espStream->println(idleTimeout); // set timeout in sec
  if (!pfodWaitForUtils::waitForOK(espStream, debugSetup)) {
    if (debugSetup != NULL) {
      debugSetup->println(F("\nAT+CIPSTO=.. OK missing"));
    }
    return false;
  }

  // connect to access point
  espStream->print(F("AT+CWJAP=\""));
  espStream->print(ssid);
  espStream->print(F("\",\""));
  espStream->print(pw);   // println adds \r\n required for AT cmds
  espStream->println('"');
  rtn = pfodWaitForUtils::waitFor("OK", "FAIL", "ready",espStream, debugSetup, 30000); // allow 30sec to connect to AP
  if (rtn != 0) {
    if (debugSetup != NULL) {
      debugSetup->println(F("\nAT+CWJAP OK missing"));
    }
    return false;
  }

  // get assigned IP NOTE this version of the AT commands do not allow IP tobe set
  espStream->println(F("AT+CIFSR")); // println adds \r\n required for AT cmds
  if (!pfodWaitForUtils::waitForOK(espStream, debugSetup)) {
    if (debugSetup != NULL) {
      debugSetup->println(F("\nAT+CIFSR OK missing"));
    }
    return false;
  }
  // turn off echo
  if (debugSetup != NULL) {
    debugSetup->print(F("\nTurn off Echo"));
  }
  espStream->println(F("ATE0")); // println adds \r\n required for AT cmds
  if (!pfodWaitForUtils::waitForOK(espStream, debugSetup)) {
    if (debugSetup != NULL) {
      debugSetup->println(F("\nATE0 OK missing"));
    }
    return false;
  }

  return true;
}

/**
 * If you have connected a digital output to the RST pin of the ESP8266
 * You can code this method to perform a hardware reset
 */
void pfodESP8266_AT::reset() {
	Stream* debugSetup = NULL;
#ifdef DEBUG_SETUP
  debugSetup = debugOut; 
#endif	
  if (hardwareResetPin >= 0) {
#ifdef DEBUG_SETUP
  if (debugSetup != NULL) {
     debugSetup->println(F("\nPerform Hardware reset"));
  }
#endif	
      pinMode(hardwareResetPin,OUTPUT);
    // toggle this pin
      digitalWrite(hardwareResetPin, LOW);   
      delay(2000);                  // waits for a second
      digitalWrite(hardwareResetPin, HIGH);    
  } else {
  	// just do software reset
  	  // reset ESP8266 
  	  if (debugSetup != NULL) {
  	  	debugSetup->println(F("Software reset of ESP8266, does not disconnect from access point"));
  	  }

  	  espStream->println(F("AT+RST")); // println adds \r\n required for AT cmds
  	  if (!pfodWaitForUtils::waitFor(F("ready"), espStream, debugSetup)) {	
  	  	if (debugSetup != NULL) {
  	  		debugSetup->println(F("\nready missing"));
  	  	}
  	  }
  }
  pfodWaitForUtils::dumpReply(espStream, debugSetup, 3000); // wait for 3sec of no data
}


/**
 * true if have pfod response to send and have not finished sending it
 */
boolean pfodESP8266_AT::isSendingResponse() {
  return responseSet;
}

long pfodESP8266_AT::getDataSize(const char* line) {
  const char *result = findStr(line, F("+IPD,"));
  if (result == NULL) {
#ifdef DEBUG
    if (debugOut != NULL) {
      debugOut->print(F("Could not find +IPD, in '"));
      debugOut->print(line);
      debugOut->println('\'');
    }
#endif // DEBUG
    return 0; // should not happen
  }
  const char *id_ptr;
  id_ptr = result;
  result = findStr(result, F(","));
  if (result == NULL) {
    // error !!!
#ifdef DEBUG
    if (debugOut != NULL) {
      debugOut->println(F("Could not find , after id in '"));
      debugOut->print(line);
      debugOut->println('\'');
    }
#endif // DEBUG
    return 0; // should not happen
  }    // else continue
  *(((char *) result) - 1) = '\0'; // terminate id
#ifdef DEBUG
  if (debugOut != NULL) {
    debugOut->print(F("ID:"));
    debugOut->println(id_ptr);
  }
#endif // DEBUG
  if ((strlen(id_ptr) != 1) || (*id_ptr != '0')) {
    ipdIdZero = false;
  } else {
  	ipdIdZero = true;
  	 if (!linkConnected) {
    	 // this is the first connection
    	 disconnect();
    	 linkConnected = true;
      clearParser(); // add 0xff to clear upstrem
      clearTxBuffer();// get ready for first response
   	 pfodTimeoutTimer = millis(); // restart timer
#ifdef DEBUG_TIMER
      if (debugOut != NULL) {
        debugOut->println(F("Connected "));
        debugOut->print(F("restart pfodTimeoutTimer:"));
        debugOut->println(pfodTimeoutTimer);
      }
#endif // DEBUG
     }  	 

  }
  // now get data size
  //long dataSize = 0;
  const char *dataSize_ptr = result;
  result = findStr(result, F(":"));
  if (result == NULL) {
    // error !!!
#ifdef DEBUG
    if (debugOut != NULL) {
      debugOut->println(F("Could not find : after dataSize in '"));
      debugOut->print(line);
      debugOut->println('\'');
    }
#endif // DEBUG
  }
  // else continue
  *(((char *) result) - 1) = '\0'; // terminate dataSize
  long dataSize = 0;
  parser.parseLong((byte*) dataSize_ptr, &dataSize);
#ifdef DEBUG
  if (debugOut != NULL) {
    debugOut->print(F("DataSize:"));
    debugOut->println(dataSize);
  }
#endif // DEBUG
  // have we got all the data
  //  long dataLen = strlen(result); // nulls not allowed in data!!! rest of code will not handle them
  // if leading char is 0xff then ignore this data as it is the connect msg
  if (*result == 0xff) {
  	ipdIdZero = false; // do not copy to result buffer
  } // else
  if (ipdIdZero) {
    dataSize = copyDataToRxBuffer(result, dataSize); // copy starting after :
  } else {
    dataSize = dropData(result, dataSize); // copy starting after :
  	// ignore this data
  }	
  	
#ifdef DEBUG
  if (debugOut != NULL) {
    debugOut->print(F("dataSizeRemaining:"));
    debugOut->println(dataSize);
  }
#endif // DEBUG
  return dataSize;
}

// copy upto dataSizeRemaining chars to rxBuffer
// return updated dataRemaining;
long pfodESP8266_AT::copyDataToRxBuffer(const char* data, long dataRemaining) {
  while ((*data != '\0') && (rxBufferIdx < ESP8266_RX_BUFFER_SIZE) && (dataRemaining > 0)) {
    rxBuffer[rxBufferIdx++] = *data;
    data++;
    dataRemaining--;
  }
  rxBufferLen = rxBufferIdx; // this depends on the parser consuming the whole buffer each time
  rxBuffer[rxBufferIdx] = '\0'; // terminate
  rxBufferIdx = 0;
  return dataRemaining;
}
// copy upto dataSizeRemaining chars to rxBuffer
// return updated dataRemaining;
long pfodESP8266_AT::dropData(const char* data, long dataRemaining) {
  while ((*data != '\0') && (dataRemaining > 0)) {
    data++;
    dataRemaining--;
  }
  return dataRemaining;
}

void pfodESP8266_AT::clearEspLine() {
#ifdef DEBUG
  if (debugOut != NULL) {
    // consolePrint(F("clearEspLine"));
    // consolePrintln();
  }
#endif // DEBUG
  espStreamMsgIdx = 0;
  espStreamMsg[0] = '\0';
}

/**
 * call this often
 * returns number of bytes left to read from SMS receive buffer
 */
int pfodESP8266_AT::available(void) {
  if (!espStreamReady) {
    init(); // reset it
  }
  if (linkConnected) {
  	if ((millis() - pfodTimeoutTimer) > pfodIdleTimeout) {
#ifdef DEBUG
  	if (debugOut != NULL) {
  		debugOut->println(F("pfodTimeoutTimer Timed out close connection"));
  	}
#endif // DEBUG
  		// disconnect since no command and timed out
  		_closeCurrentConnection();
  	}
  }	
  if (rxBufferLen == rxBufferIdx) {
    pollBoard(); // see is there is more
  }
  return (rxBufferLen - rxBufferIdx);
}

int pfodESP8266_AT::peek(void) {
  if (available()) {
    return (0xff & ((int) rxBuffer[rxBufferIdx]));
  }
  else {
    return -1;
  }
}

int pfodESP8266_AT::read(void) {
  if (available()) {
    int b = ((int) rxBuffer[rxBufferIdx++]);
    if (rxBufferIdx == rxBufferLen) {
      // buffer emptied
      rxBufferIdx = 0;
      rxBufferLen = 0;
    }
    available(); // poll again
    return (0xff & b);
  }
  else {
    return -1;
  }
}

// make this private
void pfodESP8266_AT::flush() {
#ifdef DEBUG_FLUSH
  if (debugOut != NULL) {
    debugOut->print(F("flush() txBufferIdx:"));
    debugOut->print(txBufferIdx);
    debugOut->print(F(" txBufferLen:"));
    debugOut->print(txBufferLen);
    debugOut->println();
  }
#endif // DEBUG_FLUSH
  txBuffer[txBufferIdx] = '\0';
#ifdef DEBUG_FLUSH
  if (debugOut != NULL) {
    debugOut->print(F("flush :'"));
    debugOut->print((char*) txBuffer);
    debugOut->print(F("'\n"));
  }
#endif // DEBUG_FLUSH
  txBufferLen = txBufferIdx;
  txBufferIdx = 0;
  // sendingResponse = true;
#ifdef DEBUG_WRITE
  if (debugOut != NULL) {
    //  debugOut->print(F("\nresponseSet = true\n"));
  }
#endif // DEBUG
  responseSet = true;  // prevent another response until this one written new cmd received
  // do not call OK here as should get OK from end of command
  pfodTimeoutTimer = millis(); // restart timer
#ifdef DEBUG_TIMER
      if (debugOut != NULL) {
        debugOut->print(F("flushed response restart pfodTimeoutTimer:"));
        debugOut->println(pfodTimeoutTimer);
      }
#endif // DEBUG
}


/**
 * This depends on all the raw data being written in one loop
 * otherwise get a bit of the last one
 * if no newLine then just send when buffer full.
 * while sending check other data being written and any char (other than \n) found
 * wait for \n befor filling buffer again to prevent partial data lines
 */
size_t pfodESP8266_AT::writeRawData(uint8_t c) {
  if ((!linkConnected) || (rawdataTxBufferLen != 0) || rawDataSet || isSendingResponse()) {
#ifdef DEBUG_RAWDATA
    if (debugOut != NULL) {
      if (c == '\n') {
        // got another line of raw data but still handling last one or handling response
        debugOut->println(F(" Still sending pfod response OR writeRawData waiting to send last buffer"));
      }
    }
#endif // DEBUG
    if (c != '\n') {
      foundRawDataNewline = false;
    }
    return 1;
  }    // else
#ifdef DEBUG_RAWDATA
  if (debugOut != NULL) {
    // debugOut->print(F("writeRawData("));
    // debugOut->print((char)c);
    // debugOut->print(F(")"));
  }
#endif // DEBUG
  if (!foundRawDataNewline) {
    // wait for new line to start next data line
    if (c == '\n') {
      foundRawDataNewline = true;
      // ok found one next char will start new data line
    }
    return 1; // wait for new line to start next output
  }
  rawdataTxBuffer[rawdataTxBufferIdx++] = c;
   pfodTimeoutTimer = millis(); // restart timer 
   // for consistency with pfodSecurity restart time for each raw data char
   // note: for pfod cmds only restart on response, while pfodSecurity restarts on cmd received
   // perhaps will change this later
#ifdef DEBUG_TIMER
      if (debugOut != NULL) {
        debugOut->print(F("rawdata char -- restart pfodTimeoutTimer:"));
        debugOut->println(pfodTimeoutTimer);
      }
#endif // DEBUG
 
  if ((c == '\n') || (rawdataTxBufferIdx >= ESP8266_RAWDATA_TX_BUFFER_SIZE)) {
    // end of line flush this data
    flushRawData();    // this sets rawdataTxBufferLen
    // seting rawdataTxBufferLen prevents any further raw data being written
#ifdef DEBUG_RAWDATA
    if (debugOut != NULL) {
      debugOut->println(F("\nflush RawData"));
    }
#endif // DEBUG
  }
  return 1;
}

void pfodESP8266_AT::flushRawData() {
  foundRawDataNewline = true; // can collect next line
  rawdataTxBufferLen = rawdataTxBufferIdx; // this triggers send on OK if rawdataTxBufferLen > 0
  rawdataTxBufferIdx = 0;
#ifdef DEBUG_RAWDATA
  if (debugOut != NULL) {
    debugOut->write((const char*) rawdataTxBuffer, rawdataTxBufferLen);
    debugOut->println();
  }
#endif // DEBUG
  //  rawdataTxBufferLen = 0;
  //  rawdataTxBufferIdx = 0;
  rawDataSet = true;
}

size_t pfodESP8266_AT::write(uint8_t c) {
#ifdef DEBUG_WRITE
  if (debugOut != NULL) {
    //  debugOut->print(F("write("));
    //  debugOut->print((char) c);
    //  debugOut->print(F(")"));
  }
#endif // DEBUG

  // note can start reloading buffer once cleared.
  if ((responseSet) || (!linkConnected)) {
#ifdef DEBUG
    if (debugOut != NULL) {
      debugOut->print(F("responseSet or not connected return\n"));
    }
#endif // DEBUG
    return 1; // waiting for next command
  }
  txBuffer[txBufferIdx++] = (byte) c;
  // flush on  buffer full
  if ((txBufferIdx >= ESP8266_TX_BUFFER_SIZE)) { // || ((byte) c == '}')) {  do not flush on } let parser do this
    flush();
  }
  return 1;
}

// will return pointer to null if match at end of string
// otherwise returns pointer to next char AFTER the match!!
const char* pfodESP8266_AT::findStr(const char* msg, const __FlashStringHelper *ifsh) {
  const int MAX_LEN = 16;
  char str[MAX_LEN]; // allow for null
  getProgStr(ifsh, str, MAX_LEN);
  return findStr(msg, (char*) str);
}

// will return pointer to null if match at end of string
// otherwise returns pointer to next char AFTER the match!! if found
// else return NULL if NOT found
const char * pfodESP8266_AT::findStr(const char* str, const char* target) {
  if (str == NULL) {
    return NULL;
  }
  if (target == NULL) {
    return NULL;
  }
  size_t targetLen = 0;
  size_t index = 0;
  size_t strIdx = 0;
  targetLen = strlen(target);
  if (targetLen == 0) {
    return str;
  }
  index = 0;
  char c;
  while ((c = str[strIdx++])) { // not end of null
    if (c != target[index]) {
      index = 0; // reset index if any char does not match
    } // no else here
    if (c == target[index]) {
      if (++index >= targetLen) { // return true if all chars in the target match
        return (str + strIdx);
      }
    }
  }
  return NULL;
}

void pfodESP8266_AT::espStreamPrint(const char* str) {
  if (espStream) {
    foundOK = false; // wait for OK
#ifdef DEBUG_WRITE
    if (debugOut != NULL) {
      //debugOut->println("espStreamPrint:");
      debugOut->print(str);
      // debugOut->println("'");
    }
#endif // DEBUG
    espStream->print(str);
  }
}

int pfodESP8266_AT::espStreamPrint(const char* str, int len) {
  if (espStream) {
    foundOK = false; // wait for OK
    for (int i = len; i > 0; i--) {
      if (*str != '\0') {
        espStream->print(*str);
        str++;
      }
      else {
        break;
      }
    }
    return len; // say every thing was sent
  }
  return len;
}

void pfodESP8266_AT::espStreamPrint(int i) {
  if (espStream) {
    foundOK = false; // wait for OK
#ifdef DEBUG_WRITE
    if (debugOut != NULL) {
      debugOut->print(i);
    }
#endif // DEBUG
    espStream->print(i);
  }
}

void pfodESP8266_AT::espStreamPrint(char c) {
  if (espStream) {
    foundOK = false; // wait for OK
#ifdef DEBUG_WRITE
    if (debugOut != NULL) {
      debugOut->print((char) c);
    }
#endif // DEBUG
    espStream->print(c);
  }
}

void pfodESP8266_AT::espStreamPrint(const __FlashStringHelper *ifsh) {
  if (espStream) {
    foundOK = false; // wait for OK
#ifdef DEBUG_WRITE
    if (debugOut != NULL) {
      debugOut->print(ifsh);
    }
#endif // DEBUG
    espStream->print(ifsh);
  }
}

