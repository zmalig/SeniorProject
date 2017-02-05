/**
pfodParser for Arduino
The pfodParser parses messages of the form
 { cmd ` arg1 ` arg2 ` ... }
 or
 { cmd ~ arg1}
 or
 { cmd }
 The args are optional
 This is a complete paser for ALL commands a pfodApp will send to a pfodDevice
 see www.pfod.com.au  for more details.

  pfodParser adds about 482 bytes to the program and uses about 260 bytes RAM

The pfodParser parses messages of the form
 { cmd ` arg1 ` arg2 ` ... }
 or
 { cmd ~ arg1}
 or
 { cmd }
The message is parsed into the args array by replacing '|', '`' and '}' with '/0' (null)
When the the end of message } is seen
  parse() returns the first byte of the cmd
  getCmd() returns a pointer to the null terminated cmd
  skipCmd() returns a pointer to the first arg (null terminated)
      or a pointer to null if there are no args
  getArgsCount() returns the number of args found.
These calls are valid until the start of the next msg { is parsed.
At which time they are reset to empty command and no args.

 (c)2012 Forward Computing and Control Pty. Ltd.
 This code may be freely used for both private and commercial use.
 Provide this copyright is maintained.
*/

#include "pfodParser.h"

pfodParser::pfodParser() {
	pfodParser("");
}

pfodParser::pfodParser(const char *_version) {
  setVersion(_version);
  io = NULL;
  emptyVersion[0] = 0;
  init();
}

/**
 * Note: this must NOT null the io stream
 */
void pfodParser::init() {
  argsCount = 0;
  argsIdx = 0;
  args[0] = 0; // no cmd yet
  args[1] = 0; //
  args[pfodMaxMsgLen] = 0; // double terminate
  args[pfodMaxMsgLen-1] = 0;
  cmdStart = args; // if no version
  versionStart = emptyVersion; // not used if : not found
  parserState = pfodWaitingForStart; // not started yet pfodInMsg when have seen {
  refresh = false;
}

void pfodParser::connect(Stream* ioPtr) {
  init();
  io = ioPtr;
}

void pfodParser::closeConnection() {
  //io = NULL; cannot clear this here
  init();
}

Stream* pfodParser::getPfodAppStream() {
  return io;
}

size_t pfodParser::write(uint8_t c) {
  if (!io) {
    return 1; // cannot write if io null but just pretend to
  }
  return io->write(c);
}

int pfodParser::available() {
  return 0;
}

int pfodParser::read() {
  return 0;
}
int pfodParser::peek() {
  return 0;
}

void pfodParser::flush() {
  if (!io) {
    return ; // cannot write if io null but just pretend to
  }
  //io->flush();
}

void pfodParser::setIdleTimeout(unsigned long timeout) {
  // do nothing here
}

void pfodParser::setCmd(byte cmd) {
  init();
  args[0] = cmd;
  args[1] = 0;
  cmdStart = args;
  versionStart = emptyVersion; // leave refresh unchanged
}

/**
 * Return pointer to start of message return start of cmd
 */
byte* pfodParser::getCmd() {
  return cmdStart;
}

/**
 * msg starts with {: the : is dropped from the cmd
 */
bool pfodParser::isRefresh() {
	return refresh; 
}	

/**
 * the version asked for in the command i.e. {versionRequested:...}
 */
const char *pfodParser::getVersionRequested() {
	return (const char*)versionStart; // may be empty if not version requested
}
 
const char* pfodParser::getVersion() {
	return version;
}

void pfodParser::setVersion(const char* _version) {
  version = _version;
}

void pfodParser::sendVersion() {
	print('~');
	print((char*)getVersion());
}

/**
 * Return pointer to first arg (or pointer to null if no args)
 *
 * Start at args[0] and scan for first null
 * if argsCount > 0 increment to point to  start of first arg
 * else if argsCount == 0 leave pointing to null
 */
byte* pfodParser::getFirstArg() {
  byte* idxPtr = cmdStart;
  while ( *idxPtr != 0) {
    ++idxPtr;
  }
  if (argsCount > 0) {
    ++idxPtr;
  }
  return idxPtr;
}

/**
 * Return pointer to next arg or pointer to null if end of args
 * Need to call getFirstArg() first
 * Start at current pointer and scan for first null
 * if scanned over a non empty arg then skip terminating null and return
 * pointer to next arg, else return start if start points to null already
 */
byte* pfodParser::getNextArg(byte *start) {
  byte* idxPtr = start;
  while ( *idxPtr != 0) {
    ++idxPtr;
  }
  if (idxPtr != start) {
    ++idxPtr; // skip null
  } // else this was the last arg
  return idxPtr;
}

/**
 * Return number of args in last parsed msg
 */
byte pfodParser::getArgsCount() {
  return argsCount;
}


/**
 * pfodWaitingForStart if outside msg
 * pfodMsgStarted if just seen opening {
 * pfodInMsg in msg after {
 * pfodMsgEnd if just seen closing }
 */
byte pfodParser::getParserState() {
  if ((parserState == pfodWaitingForStart) ||
      (parserState == pfodMsgStarted) ||
      (parserState == pfodInMsg) ||
      (parserState == pfodMsgEnd) ) {
    return parserState;
  } //else {
  return pfodInMsg;
}

byte pfodParser::parse() {
  byte rtn = 0;
  if (!io) {
    return rtn;
  }
  while (io->available()) {
    int in = io->read();
    rtn = parse((byte)in);
    if (rtn != 0) {
      // found msg
      if (rtn == '!') {
        closeConnection();
      }
      return rtn;
    }
  }
  return rtn;
}

/**
 * parse
 * Inputs:
 * byte in -- byte read from Serial port
 * Return:
 * return 0 if complete message not found yet
 * else return first char of cmd when see closing }
 * or ignore msg if pfodMaxCmdLen bytes after {
 * On non-zero return args[] contains
 * the cmd null terminated followed by the args null terminated
 * argsCount is the number of args
 *
 * parses
 * { cmd | arg1 ` arg2 ... }
 * { cmd ` arg1 ` arg2 ... }
 * { cmd ~ arg1 ~ arg2 ... }
 * save the cmd in args[] replace |, ~ and ` with null (\0)
 * then save arg1,arg2 etc in args[]
 * count of args saved in argCount
 * on seeing } return first char of cmd
 * if no } seen for pfodMaxCmdLen bytes after starting { then
 * ignore msg and start looking for { again
 *
 * States:
 * before {   parserState == pfodWaitingForStart
 * when   { seen parserState == pfodInMsg
 */
byte pfodParser::parse(byte in) {
  if (in == 0xff) {
    // note 0xFF is not a valid utf-8 char
    // but is returned by underlying stream if start or end of connection
    // NOTE: Stream.read() is wrapped in while(Serial.available()) so should not get this
    // unless explicitlly added to stream buffer
    init(); // clean out last partial msg if any
    return 0;
  }
  if ((parserState == pfodWaitingForStart) || (parserState == pfodMsgEnd)) {
    parserState = pfodWaitingForStart;
    if (in == pfodMsgStarted) { // found {
      init(); // clean out last cmd
      parserState = pfodMsgStarted;
    }
    // else ignore this char as waiting for start {
    // always reset counter if waiting for {
    return 0;
  }

  // else have seen {
  if ((argsIdx >= (pfodMaxMsgLen - 2)) && // -2 since { never stored
      (in != pfodMsgEnd)) {
    // ignore this msg and reset
    // should not happen as pfodApp should limit
    // msgs sent to pfodDevice to <=255 bytes
    init();
    return 0;
  }
  // first char after opening {
  if (parserState == pfodMsgStarted) {
    parserState = pfodInMsg;
    if (in == pfodRefresh) {
      refresh = true;
      return 0; // skip this byte if get {:
    }
  }
  // else continue. Check for version:
  if ((in == pfodRefresh) && (versionStart != args)) {
    // found first : set version pointer
    args[argsIdx++] = 0;
  	versionStart = args;
    cmdStart = args+argsIdx; // next byte after :
    refresh = (strcmp((const char*)versionStart,(const char*)version) == 0);
    return 0; 
  }
   
  // else process this msg char
  // look for special chars | ' } 
  if ((in == pfodMsgEnd) || (in == pfodBar) || (in == pfodTilda) || (in == pfodAccent)) {
  	args[argsIdx++] = 0;
    if (parserState == pfodArgStarted) {
      // increase count if was parsing arg
      argsCount++;
    }
    if (in == pfodMsgEnd) {
      parserState = pfodMsgEnd; // reset state
      args[argsIdx++] = 0;
      // return command byte found
      // this will return 0 when parsing {} msg
      return cmdStart[0];
    } else {
      parserState = pfodArgStarted;
    }
    return 0;
  }
  // else normal byte
  args[argsIdx++] = in;
  return 0;
}


/**
 * parseLong
 * will parse between  -2,147,483,648 to 2,147,483,647
 * No error checking done.
 * will return 0 for empty string, i.e. first byte == null
 *
 * Inputs:
 *  idxPtr - byte* pointer to start of bytes to parse
 *  result - long* pointer to where to store result
 * return
 *   byte* updated pointer to bytes after skipping terminator
 *
 */
byte* pfodParser::parseLong(byte* idxPtr, long *result) {
  long rtn = 0;
  boolean neg = false;
  while ( *idxPtr != 0) {
    if (*idxPtr == '-') {
      neg = true;
    } else {
      rtn = (rtn << 3) + (rtn << 1); // *10 = *8+*2
      rtn = rtn +  (*idxPtr - '0');
    }
    ++idxPtr;
  }
  if (neg) {
    rtn = -rtn;
  }
  *result = rtn;
  return ++idxPtr; // skip null
}


void pfodParser::setDebugStream(Print* debugOut) {
  ; // does nothing
}

