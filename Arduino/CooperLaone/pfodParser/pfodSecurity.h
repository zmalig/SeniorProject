#ifndef pfodSecurity_h
#define pfodSecurity_h
/**
pfodSecurity for Arduino
 Parses commands of the form { cmd | arg1 ` arg2 ... } hashCode
 Arguments are separated by `
 The | and the args are optional
 This is a complete paser for ALL commands a pfodApp will send to a pfodDevice
 see www.pfod.com.au  for more details.
 see http://www.forward.com.au/pfod/secureChallengeResponse/index.html for the details on the
 128bit security and how to generate a secure secret password

  pfodSecurity adds about 2100 bytes to the program and uses about 400 bytes RAM
   and upto 19 bytes of EEPROM starting from the address passed to init()

The pfodSecurity parses messages of the form
 { cmd | arg1 ` arg2 ` ... } hashCode
 The hashCode is checked against the SipHash_2_4 using the 128bit secret key.
 The hash input is the message count, the challenge for this connection and the message
 If the hash fails the cmd is set to DisconnectNow (0xff)
 It is then the responsability of the calling program to close the connection.
 and to call pfodSecurity::disconnected() when the connection is closed.

 If the hash passes the message is parsed into the args array
  by replacing '|', '`' and '}' with '/0' (null)

  When the the end of message } is seen
  parse() returns the first byte of the cmd
  getCmd() returns a pointer to the null terminated cmd
  skipCmd() returns a pointer to the first arg (null terminated)
      or a pointer to null if there are no args
  getArgsCount() returns the number of args found.
These calls are valid until the start of the next msg { is parsed.
At which time they are reset to empty command and no args.

 (c)2013 Forward Computing and Control Pty. Ltd.
 This code may be freely used for both private and commercial use.
 Provide this copyright is maintained.
*/

#include "Arduino.h"
#include "pfodParser.h"
#include "Stream.h"
#include "pfodMAC.h"
#include "pfod_Base.h"
#include "pfodESP8266_AT.h"

class pfodSecurity : public Print {
  public:
    pfodSecurity();
    pfodSecurity(const char *_version);

    // methods required for Print
    size_t write(uint8_t);
    size_t write(const uint8_t *buffer, size_t size);


    /**
     * Set the idle Timeout in sec  i.e. 15 ==> 15sec,   0 means never timeout
     * Default is 15 (sec)
     * When set to >0 then if no incoming msg received for this time (in Sec), then the
     * parser will return DisconnectNow to disconnect the link
     *
     * Setting this to non-zero value protects against a hacker taking over your connection and just hanging on to it
     * not sending any msgs but not releasing it, so preventing you from re-connecting.
     * when setting to non-zero you can use the pfod re-request time to ask the pfodApp to re-request a menu every so often to prevent the connection timing out
     * while the pfodApp is running. See the pfod Specification for details.
     *
     * Set to >0 to enable the timeout (recommended)
     */
    void setIdleTimeout(unsigned long timeout);
    void setAuthorizationTimeout(unsigned long timeout);

    void setDebugStream(Print* debugOut);


    /**
     * initialize the Security parser
     * args
     * io_arg the Stream pointer to read and write to for pfod messages
     *
     * eepromAddress the starting address in eeprom to save the key and power cycles
     *    amount of eeprom used is (2 bytes power cycles + 1 byte key length + key bytes) ==> 3 + (input hexKey length)/2,
     *
     * hexKey  pointer to program memory F("hexString") holding the key
     *  if this key is different from the current one in eeprom the eeprom is updated and the
     *  power cycles are reset to 0xffff
     *  if changing the key suggest you add 2 to your eepromAddress to move on from the previous
     *  one.  The power cycle EEPROM addresses are are written to on each power up
     *
     */
    void connect(Stream* io_arg);
    void connect(Stream* io_arg, const __FlashStringHelper *hexKeyPgr, int eepromAddress = 0);
    void connect(pfod_Base* sms_arg);
    void connect(pfod_Base* sms_arg,  const __FlashStringHelper *hexKeyPgr, int eepromAddress = 0);

    Stream* getPfodAppStream();

    void closeConnection(); // called when connection closed or DisconnectNow returned from parser
    //static const byte DisconnectNow = '!'; // this is returned if pfodDevice should drop the connection

    byte parse(); // call this in loop() every loop, it will read bytes, if any, from the pfodAppStream and parse them
    // returns 0 if message not complete, else returns the first char of a completed and verified message
    bool isRefresh(); // starts with {: the : is dropped from the cmd
    const char *getVersionRequested(); // the version asked for in the command i.e. {versionRequested:...}
    const char* getVersion();
    void setVersion(const char* version); // no usually needed
    void sendVersion(); // send ~ version to parser.print
    byte* getCmd();  // pointer to current parsed command, points to /0 if no command, 0xff if DisconnectNow was returned from the parser
    byte* getFirstArg(); // pointer to the first command argument, points to /0 if there are no arguments
    byte* getNextArg(byte *start);
    byte getArgsCount(); // number of arguments in the current command, 0 if none
    byte* parseLong(byte* idxPtr, long *result); // parse the argument, pointed to by idxPtr, as a long
    void init();


  private:
    void connect(Stream* io_arg, Print* raw_io_arg);
    void connect(Stream* io_arg, Print* raw_io_arg, const __FlashStringHelper *hexKeyPgr, int eepromAddress = 0);
    int getBytesFromPassword(char *hexKey, int hexKeyLen, byte *keyBytes, int keyMaxLen);
    uint32_t decodePasswordBytes(byte* bytes, int idx, int bytesLen);
    uint8_t byte64ToByte(uint8_t b);
    boolean setIdleTimeoutCalled;
    unsigned long	_authorizationTimeout;
    size_t writeToPfodApp(uint8_t* idxPtr);
    size_t writeToPfodApp(uint8_t b);
    Stream *io;
    Print *raw_io; // set to null on disconnect
    Print *raw_io_connect_arg; // save for later resuse
    Print *debugOut;
    pfodParser parser;
    pfodMAC mac;
    boolean parsing; // true when parsing, after disconnected() called and before returning 0xff, false between returning 0xff and disconnected being called
    byte authorizing;
    byte challenge[pfodMAC::challengeByteSize + 1]; // add one for hash identifier
    unsigned long lastMillis; // holds the last read millis()
    unsigned long timeSinceLastConnection; // limited to 0x7fffffff
    boolean failedHash; // true if any hash check failed last connection
    long connectionTimer;
    void setSecretKey(int eepromAddress, byte *key, int len);
    boolean noPassword;
    void setAuthorizeState(int auth);
    int msgHashCount;
    static const byte Msg_Hash_Size = 8; // number of hex digits for msg hash
    static const byte Msg_Hash_Size_Bytes = (Msg_Hash_Size >> 1); // number of hex bytes for msg hash i.e. 4
    byte msgHashBytes[Msg_Hash_Size + 1]; // allow for null outgoing
    unsigned long inMsgCount;
    unsigned long outMsgCount;
    byte outputParserState;
    boolean initialization;
    unsigned long idleTimeOut;
    const __FlashStringHelper *hexKeyPgr;
    int eepromAddress;
    pfod_Base* pfod_Base_set;
    bool doFlush; // set to true for SMS /ESP-AT only, otherwise false
    bool lastConnectionClosed; // set true by constructor and closeConnection(), set false by connect()
    // this prevents call to closeConnection from connect provided last connection close cleanly.
};

#endif // pfodSecurity_h

