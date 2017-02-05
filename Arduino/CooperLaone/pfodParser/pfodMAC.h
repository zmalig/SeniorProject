#ifndef pfodMAC_h
#define pfodMAC_h
/**
pfodMAC for Arduino
 Updates power cycles, builds the Challenge, keeps track in message counts and
 hashes and check message hashes
 It uses upto 19 bytes of EEPROM starting from the address passed to init()

 (c)2013 Forward Computing and Control Pty. Ltd.
 This code may be freely used for both private and commercial use.
 Provide this copyright is maintained.
*/

#include "Arduino.h"
#include "Stream.h"

#define MAX_ELAPSED_TIME 0x7fffffff

class pfodMAC {
  public:

    pfodMAC();
    boolean init(int eepromAddress);
    void setDebugStream(Print* out);
    boolean isValid();  // powercycles > 0
    boolean isBigEndian(); // is 1 stored as 0x00000001  false for Atmel 8 bit micros

    unsigned int readSecretKey(byte* bRtn, int keyAdd);

    // utility methods
    uint8_t eeprom_read(int address);
    void eeprom_write(int address, uint8_t value);

    // useful for debugging
    void printBytesInHex(Print *out, byte* b, int len);

    /**
    * buildChallenge  -- NOTE: each call to this method increments the connection counter!!!
    * Challenge built from
    * powerCycles + connectionCounter + mS_sinceLastConnection (limited to 31 bits) 32th bit set if last connection failed
    * finally hashes the challange bytes (result stored in hash fn)
    * use getHashedResult() to retrieve pointer to result when needed for checking return value
    *
    * All counters stored in BigEndian format into the challenge byte array
    * powerCycles read on start up
    *
    * @param challengeBytes byte* to array to store challenge
    * @param mS_sinceLastConnection  mS since last connection (max value about 25days), -ve if last connection failed
    *
    * returns true if isValid() is true;
    */
    boolean buildChallenge(byte* challengeBytes, long mS_sinceLastConnection);

    void initHash(); // start a new hash by initializing with the secret key
    void putByteToHash(byte b); // add a byte to the hash
    void putBytesToHash(byte* b, int len); // add a byte array to the hash
    void putLongToHash(long l); // add a long to the hash (in BigEndian format)
    void finishHash(); // finish the hash and calculate the result
    byte* getHashedResult(); // get pointer to the 8 byte result


    /**
    * Converts msgHash from hex Digits to bytes
    * and then compares the resulting byte[] with the current hash result
    * @param msgHash null terminated ascii string of hex chars (0..9A..F) or (0..9a..f)
    *      this input is overwritten with the converted bytes
    * @param hashSize the number of bytes expected in the hash must be < length of msgHash since the converted bytes are store in that array.
    */
    boolean checkMsgHash(const byte* msgHash, int len);

    void reverseBytes(byte *b, int len); // swap from order of bytes upto len, converts between Endians, Big to Little, Little to Big

    const static unsigned char msgHashByteSize = 4; // number of byte in per msg hash
    const static unsigned char challengeHashByteSize = 8; // number of bytes in challenge hash
    const static unsigned char challengeByteSize = 8; // number of bytes in challenge
    const static unsigned char PowerCyclesOffset; // == 0 offset to power cycles from eepromAddress
    const static unsigned char KeyOffset; // offset to keyLen from eepromAddress, length (1 byte) comes first then key
    const static unsigned char maxKeySize = 16; // max size of key 128 bits

  private:
    boolean cmpToResult(const byte* response, int len);
    int CHAP_EEPROM_Address;
    Print *debugOut;
    const static unsigned char powerCycleSize; // int
    const static unsigned char timerSize; // long
    const static unsigned char counterSize; // int
    unsigned int connectionCounter; // increment each connection
    unsigned int powerCycles;
    byte key[maxKeySize]; // the secret key after reading from EEPROM and expanding to 16 bytes
    unsigned char keyLen; // 0 if no key, else 16 (after padding key with 0 if shorter)
    boolean bigEndian;  // true if bigEndian uC
    unsigned char *hashChallenge(byte *challenge); // hashes challenge returns pointer to 8 byte result

    /**
    * read power cycles if >0 decrement by 1 and save again
    * else if 0 leave as is
    *
    * @param powerCycleAdd -- the EEPROM address where the 2byte power cycle unsigned int is stored
    *
    * @return the updated power cycle count as an unsigned int
    */
    unsigned int readPowerCycles(int powerCycleAdd);

};


#endif // pfodMAC_h

