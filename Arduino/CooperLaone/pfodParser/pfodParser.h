#ifndef pfodParser_h
#define pfodParser_h
/**
pfodParser for Arduino
 Parses commands of the form { cmd | arg1 ` arg2 ... }
 Arguments are separated by `
 The | and the args are optional
 This is a complete paser for ALL commands a pfodApp will send to a pfodDevice
 see www.pfod.com.au  for more details.

  pfodParser adds about 482 bytes to the program and uses about 260 bytes RAM

The pfodParser parses messages of the form
 { cmd | arg1 ` arg2 ` ... }
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

#include "Arduino.h"
#include "pfodParserUtils.h"

class pfodParser: public Print {
  public:
    pfodParser(const char* version);
    pfodParser();
    void connect(Stream* ioPtr);
    void closeConnection();

    byte parse();
    bool isRefresh(); // starts with {version: and the version matches this parser's version
    const char *getVersionRequested(); // the version asked for in the command i.e. {versionRequested:...}
    const char* getVersion();
    void setVersion(const char* version); // no usually needed
    void sendVersion(); // send ~ version to parser.print
    byte* getCmd();
    byte* getFirstArg();
    byte* getNextArg(byte *start);
    byte getArgsCount();
    byte* parseLong(byte* idxPtr, long *result);
    /**
     * pfodWaitingForStart if outside msg
     * pfodMsgStarted if just seen opening {
     * pfodInMsg in msg after {
     * pfodMsgEnd if just seen closing }
     */
    byte getParserState();
    void setCmd(byte cmd);
    static const byte pfodWaitingForStart = 0xff;
    static const byte pfodMsgStarted = '{';
    static const byte pfodRefresh = ':';
    static const byte pfodInMsg = 0;
    static const byte pfodMsgEnd = '}';
    void setDebugStream(Print* debugOut); // does nothing
    size_t write(uint8_t c);
    int available();
    int read();
    int peek();
    void flush();
    void setIdleTimeout(unsigned long timeout); // does nothing in parser
    Stream* getPfodAppStream(); // get the command response stream we are writing to
    // for pfodParser this is also the rawData stream

    // this is returned if pfodDevice should drop the connection
    // only returned by pfodParser in read() returns -1
    void init();  // for now
    byte parse(byte in); // for now

  private:
    //static const byte DisconnectNow = '!';
    Stream* io;
    // you can reduce the value if you are handling smaller messages
    // but never increase it.
    static const byte pfodMaxMsgLen = 0xff; // == 255, if no closing } by now ignore msg
    byte emptyVersion[1];
    byte argsCount;  // no of arguments found in msg
    byte argsIdx;
    byte parserState;
    byte args[pfodMaxMsgLen + 1]; // allow for trailing null
    byte *versionStart;
    byte *cmdStart;
    bool refresh;
    const char *version;
    static const byte pfodBar = (byte)'|';
    static const byte pfodTilda = (byte)'~';
    static const byte pfodAccent = (byte)'`';
    static const byte pfodArgStarted = 0xfe;
};

#endif // pfodParser_h

