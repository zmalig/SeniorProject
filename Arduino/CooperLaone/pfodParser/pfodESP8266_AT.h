#ifndef pfodESP8266_AT_h
#define pfodESP8266_AT_h
/**

 (c)2012 Forward Computing and Control Pty. Ltd.
 This code may be freely used for both private and commercial use.
 Provide this copyright is maintained.
 This library uses about 17K of ROM (program space) and 3200 bytes of RAM
 and so is NOT suitable for use on an UNO
 Use a Mega2560 or similar with more then 4K of RAM
 */

#include "Arduino.h"
#include "Stream.h"
#include "pfodParser.h"
#include "pfod_Base.h"
#include "pfod_rawOutput.h"

class pfodESP8266_AT : public pfod_Base {

  public:
    pfodESP8266_AT();
    void init(Stream *_espStream, const char* _ssid, const char* _pw, int _portNo, int _hardwareResetPin);
    void setDebugStream(Stream* out);
    void setIdleTimeout(unsigned long timeOut);
    void _closeCurrentConnection();
    unsigned long getDefaultTimeOut();
    Print* getRawDataOutput();
    int available(void);
    int peek(void);
    int read(void);
    void flush(void);
    size_t write(uint8_t);
    size_t writeRawData(uint8_t c);
    Stream* getPfodAppStream(); // use pfodSecurity.getPfodAppStream() instead get the command response stream we are reading from and writing to

  private:
    //MAX_UART_MSG_LEN  +IPD,0,123456789:...
    const static int MAX_UART_MSG_LEN = 32; // max msg len then null
    const static int MAX_TX_MSG_LEN = 32; // max msg len for sending then null
    const static int ESP8266_MAX_SIZE = 160;
    const static int ESP8266_RX_BUFFER_SIZE = MAX_UART_MSG_LEN + 32; // always larger
    // must be greater than MAX_UART_MSG_LEN
    const static int ESP8266_TX_MSG_SIZE = 1026;
    // 1024 + 2
    const static int ESP8266_TX_BUFFER_SIZE = (ESP8266_TX_MSG_SIZE + 8 + 2);
    // add a little 1026 + 8(hash) + 2(extra) + null

    int parserCleared; // not used in this code, needs to be removed
    int hardwareResetPin;
    boolean linkConnected;
    long dataSizeRemaining; // how much more data are we expecting
    void init();
    pfodParser parser;
    unsigned long getIdleTimeout();

    void clearParser();
    boolean ESP8266_setup();
    void reset();

    const char* findStr(const char* msg, const __FlashStringHelper *ifsh);
    const char *findStr(const char* str, const char* target);

    //espLine:'[]' = 2
    // max incomming message size from pfodApp
    //espLine:'+IPD,0,256:<data>[]'  = 13+256+8(hash) + 2(extra) = 279
    //espLine:'[]' = 2
    //espLine:'OK[]' 4
    // max msg size after decode  2 + 279 + 2 + 4 +null = 288
    // max pfod msg size

    char espStreamMsg[MAX_UART_MSG_LEN + 1]; // plus null  idx runs from 0 to MAX_UART_MSG_LEN
    int espStreamMsgIdx; // = 0;
    char espTxMsg[MAX_TX_MSG_LEN + 1]; // plus null  idx runs from 0 to MAX_TX_MSG_LEN

    size_t rxBufferLen;  // runs from 0 to SMS_RX_BUFFER_SIZE
    size_t txBufferLen;  // runs from 0 to SMS_TX_BUFFER_SIZE
    size_t rxBufferIdx; // runs from 0 to rxBufferLen
    size_t txBufferIdx; // runs from 0 to txBufferLen
    byte rxBuffer[ESP8266_RX_BUFFER_SIZE + 1]; // allow for terminating null
    byte txBuffer[ESP8266_TX_BUFFER_SIZE + 1]; // allow for terminating null
    static const int ESP8266_RAWDATA_TX_BUFFER_SIZE = 256; // a bit arbitary
    byte rawdataTxBuffer[ESP8266_RAWDATA_TX_BUFFER_SIZE + 1]; // allow for terminating null and force null at end
    size_t rawdataTxBufferLen;  // runs from 0 to SMS_RAWDATA_TX_BUFFER_SIZE
    size_t rawdataTxBufferIdx; // runs from 0 to rawdataTxBufferLen
    void flushRawData();
    void clearRawDataTxBuffer();
    boolean isSendingResponse();
    void handleOK();
    void disconnect();
    unsigned long atTimer;
    unsigned long okTimer;
    const static unsigned long AT_TIME = 5000;
    const static unsigned long OK_TIME = 2 * AT_TIME; // if miss an OK from AT then restart.
    boolean sendingRawData; // true if processing and sending SMS response to command
    boolean sendingResponse; // true if processing and sending SMS response to command
    // if sendingResponse then canSendRawData() return false and any write is just dropped
    boolean responseSet; // true if tx_buffer holds respons upto } and no new cmd recieved
    boolean rawDataSet; // true if have tx buffer of raw data
    boolean waitingToSend; // true if waiting for >
    boolean foundOK; // true when OK was last espStream msg

    void pollBoard();
    void sendRawData();
    const char* collectEspMsgs();
    void clearEspLine();
    void processEspLine(const char* line);
    Stream *espStream; // sms shield serial connection
    const char* ssid;
    const char* pw;
    
    int portNo;
    
    const static byte invalidSMSCharReturn = 0xff; // == 255,
    long copyDataToRxBuffer(const char* data, long dataRemaining);
    long dropData(const char* data, long dataRemaining);
    long getDataSize(const char* line);
    boolean espStreamReady;
    void clearTxBuffer();
    void clearReadSmsMsgs();
    void clearAllSmsMsgs();
    void emptyIncomingMsgNo();
    void espStreamPrint(const char* str);
    int espStreamPrint(const char* str, int len);
    void espStreamPrint(char c);
    void espStreamPrint(const __FlashStringHelper *ifsh);
    void espStreamPrint(int i);
    boolean haveIncomingMsgNo();
    boolean addIncomingMsgNo(const char* msgNo);
    const char* getIncomingMsgNo();
    void deleteIncomingMsg(const char *newMsgNo);
    pfod_rawOutput raw_io;
    pfod_rawOutput* raw_io_ptr;
    boolean foundRawDataNewline;
    Stream* debugOut;
    static const int DisconnectId_Max_Len = 5;
    char disconnectId[DisconnectId_Max_Len + 1]; // \0 for none else id to disconnect
    boolean ipdIdZero; // true if this input is for connection 0
    unsigned long idleTimeout; // if not data sent received
    unsigned long pfodIdleTimeout; // if not command recieved
    unsigned long pfodTimeoutTimer;
};

#endif // pfodESP8266_AT_h

